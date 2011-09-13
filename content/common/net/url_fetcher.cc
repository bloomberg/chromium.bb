// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/net/url_fetcher.h"

#include <set>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_throttler_manager.h"

static const int kBufferSize = 4096;
const int URLFetcher::kInvalidHttpResponseCode = -1;

class URLFetcher::Core
    : public base::RefCountedThreadSafe<URLFetcher::Core>,
      public net::URLRequest::Delegate {
 public:
  // For POST requests, set |content_type| to the MIME type of the content
  // and set |content| to the data to upload.  |flags| are flags to apply to
  // the load operation--these should be one or more of the LOAD_* flags
  // defined in net/base/load_flags.h.
  Core(URLFetcher* fetcher,
       const GURL& original_url,
       RequestType request_type,
       URLFetcher::Delegate* d);

  // Starts the load.  It's important that this not happen in the constructor
  // because it causes the IO thread to begin AddRef()ing and Release()ing
  // us.  If our caller hasn't had time to fully construct us and take a
  // reference, the IO thread could interrupt things, run a task, Release()
  // us, and destroy us, leaving the caller with an already-destroyed object
  // when construction finishes.
  void Start();

  // Stops any in-progress load and ensures no callback will happen.  It is
  // safe to call this multiple times.
  void Stop();

  // Reports that the received content was malformed (i.e. failed parsing
  // or validation).  This makes the throttling logic that does exponential
  // back-off when servers are having problems treat the current request as
  // a failure.  Your call to this method will be ignored if your request is
  // already considered a failure based on the HTTP response code or response
  // headers.
  void ReceivedContentWasMalformed();

  // Overridden from net::URLRequest::Delegate:
  virtual void OnResponseStarted(net::URLRequest* request);
  virtual void OnReadCompleted(net::URLRequest* request, int bytes_read);

  URLFetcher::Delegate* delegate() const { return delegate_; }
  static void CancelAll();

 private:
  friend class base::RefCountedThreadSafe<URLFetcher::Core>;

  class Registry {
   public:
    Registry();
    ~Registry();

    void AddURLFetcherCore(Core* core);
    void RemoveURLFetcherCore(Core* core);

    void CancelAll();

    int size() const {
      return fetchers_.size();
    }

   private:
    std::set<Core*> fetchers_;

    DISALLOW_COPY_AND_ASSIGN(Registry);
  };

  // Class TempFileWriter encapsulates all state involved in writing
  // response bytes to a temporary file. It is only used if
  // |Core::response_destination_| == TEMP_FILE.  Each instance of
  // TempFileWriter is owned by a URLFetcher::Core, which manages
  // its lifetime and never transfers ownership.  While writing to
  // a file, all function calls happen on the IO thread.
  class TempFileWriter {
   public:
    TempFileWriter(
        URLFetcher::Core* core,
        scoped_refptr<base::MessageLoopProxy> file_message_loop_proxy);

    ~TempFileWriter();
    void CreateTempFile();
    void DidCreateTempFile(base::PlatformFileError error_code,
                           base::PassPlatformFile file_handle,
                           FilePath file_path);

    // Record |num_bytes_| response bytes in |core_->buffer_| to the file.
    void WriteBuffer(int num_bytes);

    // Called when a write has been done.  Continues writing if there are
    // any more bytes to write.  Otherwise, initiates a read in core_.
    void ContinueWrite(base::PlatformFileError error_code,
                       int bytes_written);

    // Drop ownership of the file at path |temp_file_|. This class
    // will not delete it or write to it again.
    void DisownTempFile();

    // Close the temp file if it is open.
    void CloseTempFileAndCompleteRequest();

    // Remove the temp file if we we created one.
    void RemoveTempFile();

    const FilePath& temp_file() const { return temp_file_; }
    int64 total_bytes_written() { return total_bytes_written_; }
    base::PlatformFileError error_code() const { return error_code_; }

   private:
    // Callback which gets the result of closing the temp file.
    void DidCloseTempFile(base::PlatformFileError error);

    // The URLFetcher::Core which instantiated this class.
    URLFetcher::Core* core_;

    // The last error encountered on a file operation.  base::PLATFORM_FILE_OK
    // if no error occurred.
    base::PlatformFileError error_code_;

    // Callbacks are created for use with base::FileUtilProxy.
    base::ScopedCallbackFactory<URLFetcher::Core::TempFileWriter>
        callback_factory_;

    // Message loop on which file opperations should happen.
    scoped_refptr<base::MessageLoopProxy> file_message_loop_proxy_;

    // Path to the temporary file.  This path is empty when there
    // is no temp file.
    FilePath temp_file_;

    // Handle to the temp file.
    base::PlatformFile temp_file_handle_;

    // We always append to the file.  Track the total number of bytes
    // written, so that writes know the offset to give.
    int64 total_bytes_written_;

    // How many bytes did the last Write() try to write?  Needed so
    // that if not all the bytes get written on a Write(), we can
    // call Write() again with the rest.
    int pending_bytes_;

    // When writing, how many bytes from the buffer have been successfully
    // written so far?
    int buffer_offset_;
  };

  virtual ~Core();

  // Wrapper functions that allow us to ensure actions happen on the right
  // thread.
  void StartOnIOThread();
  void StartURLRequest();
  void StartURLRequestWhenAppropriate();
  void CancelURLRequest();
  void OnCompletedURLRequest(base::TimeDelta backoff_delay);
  void InformDelegateFetchIsComplete();
  void NotifyMalformedContent();
  void RetryOrCompleteUrlFetch();

  // Deletes the request, removes it from the registry, and removes the
  // destruction observer.
  void ReleaseRequest();

  // Returns the max value of exponential back-off release time for
  // |original_url_| and |url_|.
  base::TimeTicks GetBackoffReleaseTime();

  void CompleteAddingUploadDataChunk(const std::string& data,
                                     bool is_last_chunk);

  // Adds a block of data to be uploaded in a POST body. This can only be
  // called after Start().
  void AppendChunkToUpload(const std::string& data, bool is_last_chunk);

  // Store the response bytes in |buffer_| in the container indicated by
  // |response_destination_|. Return true if the write has been
  // done, and another read can overwrite |buffer_|.  If this function
  // returns false, it will post a task that will read more bytes once the
  // write is complete.
  bool WriteBuffer(int num_bytes);

  // Read response bytes from the request.
  void ReadResponse();

  // Drop ownership of any temp file managed by |temp_file_|.
  void DisownTempFile();

  URLFetcher* fetcher_;              // Corresponding fetcher object
  GURL original_url_;                // The URL we were asked to fetch
  GURL url_;                         // The URL we eventually wound up at
  RequestType request_type_;         // What type of request is this?
  net::URLRequestStatus status_;     // Status of the request
  URLFetcher::Delegate* delegate_;   // Object to notify on completion
  scoped_refptr<base::MessageLoopProxy> delegate_loop_proxy_;
                                     // Message loop proxy of the creating
                                     // thread.
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
                                     // The message loop proxy for the thread
                                     // on which the request IO happens.
  scoped_refptr<base::MessageLoopProxy> file_message_loop_proxy_;
                                     // The message loop proxy for the thread
                                     // on which file access happens.
  scoped_ptr<net::URLRequest> request_;   // The actual request this wraps
  int load_flags_;                   // Flags for the load operation
  int response_code_;                // HTTP status code for the request
  std::string data_;                 // Results of the request, when we are
                                     // storing the response as a string.
  scoped_refptr<net::IOBuffer> buffer_;
                                     // Read buffer
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
                                     // Cookie/cache info for the request
  net::ResponseCookies cookies_;     // Response cookies
  net::HttpRequestHeaders extra_request_headers_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  bool was_fetched_via_proxy_;
  net::HostPortPair socket_address_;

  std::string upload_content_;       // HTTP POST payload
  std::string upload_content_type_;  // MIME type of POST payload
  std::string referrer_;             // HTTP Referer header value
  bool is_chunked_upload_;           // True if using chunked transfer encoding

  // Used to determine how long to wait before making a request or doing a
  // retry.
  // Both of them can only be accessed on the IO thread.
  // We need not only the throttler entry for |original_URL|, but also the one
  // for |url|. For example, consider the case that URL A redirects to URL B,
  // for which the server returns a 500 response. In this case, the exponential
  // back-off release time of URL A won't increase. If we retry without
  // considering the back-off constraint of URL B, we may send out too many
  // requests for URL A in a short period of time.
  scoped_refptr<net::URLRequestThrottlerEntryInterface>
      original_url_throttler_entry_;
  scoped_refptr<net::URLRequestThrottlerEntryInterface> url_throttler_entry_;

  // |num_retries_| indicates how many times we've failed to successfully
  // fetch this URL.  Once this value exceeds the maximum number of retries
  // specified by the owner URLFetcher instance, we'll give up.
  int num_retries_;

  // True if the URLFetcher has been cancelled.
  bool was_cancelled_;

  // If writing results to a file, |temp_file_writer_| will manage creation,
  // writing, and destruction of that file.
  scoped_ptr<TempFileWriter> temp_file_writer_;

  // Where should responses be saved?
  ResponseDestinationType response_destination_;

  // If |automatically_retry_on_5xx_| is false, 5xx responses will be
  // propagated to the observer, if it is true URLFetcher will automatically
  // re-execute the request, after the back-off delay has expired.
  // true by default.
  bool automatically_retry_on_5xx_;
  // Maximum retries allowed.
  int max_retries_;

  static base::LazyInstance<Registry> g_registry;

  friend class URLFetcher;
  DISALLOW_COPY_AND_ASSIGN(Core);
};

URLFetcher::Core::Registry::Registry() {}
URLFetcher::Core::Registry::~Registry() {}

void URLFetcher::Core::Registry::AddURLFetcherCore(Core* core) {
  DCHECK(!ContainsKey(fetchers_, core));
  fetchers_.insert(core);
}

void URLFetcher::Core::Registry::RemoveURLFetcherCore(Core* core) {
  DCHECK(ContainsKey(fetchers_, core));
  fetchers_.erase(core);
}

void URLFetcher::Core::Registry::CancelAll() {
  while (!fetchers_.empty())
    (*fetchers_.begin())->CancelURLRequest();
}

// static
base::LazyInstance<URLFetcher::Core::Registry>
    URLFetcher::Core::g_registry(base::LINKER_INITIALIZED);

URLFetcher::Core::TempFileWriter::TempFileWriter(
    URLFetcher::Core* core,
    scoped_refptr<base::MessageLoopProxy> file_message_loop_proxy)
    : core_(core),
      error_code_(base::PLATFORM_FILE_OK),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      file_message_loop_proxy_(file_message_loop_proxy),
      temp_file_handle_(base::kInvalidPlatformFileValue) {
}

URLFetcher::Core::TempFileWriter::~TempFileWriter() {
  RemoveTempFile();
}

void URLFetcher::Core::TempFileWriter::CreateTempFile() {
  DCHECK(core_->io_message_loop_proxy_->BelongsToCurrentThread());
  CHECK(file_message_loop_proxy_.get());
  base::FileUtilProxy::CreateTemporary(
      file_message_loop_proxy_,
      0,  // No additional file flags.
      callback_factory_.NewCallback(
          &URLFetcher::Core::TempFileWriter::DidCreateTempFile));
}

void URLFetcher::Core::TempFileWriter::DidCreateTempFile(
    base::PlatformFileError error_code,
    base::PassPlatformFile file_handle,
    FilePath file_path) {
  DCHECK(core_->io_message_loop_proxy_->BelongsToCurrentThread());

  if (base::PLATFORM_FILE_OK != error_code) {
    error_code_ = error_code;
    RemoveTempFile();
    core_->delegate_loop_proxy_->PostTask(
        FROM_HERE,
        NewRunnableMethod(core_, &Core::InformDelegateFetchIsComplete));
    return;
  }

  temp_file_ = file_path;
  temp_file_handle_ = file_handle.ReleaseValue();
  total_bytes_written_ = 0;

  core_->io_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(core_, &Core::StartURLRequestWhenAppropriate));
}

void URLFetcher::Core::TempFileWriter::WriteBuffer(int num_bytes) {
  DCHECK(core_->io_message_loop_proxy_->BelongsToCurrentThread());

  // Start writing to the temp file by setting the initial state
  // of |pending_bytes_| and |buffer_offset_| to indicate that the
  // entire buffer has not yet been written.
  pending_bytes_ = num_bytes;
  buffer_offset_ = 0;
  ContinueWrite(base::PLATFORM_FILE_OK, 0);
}

void URLFetcher::Core::TempFileWriter::ContinueWrite(
    base::PlatformFileError error_code,
    int bytes_written) {
  DCHECK(core_->io_message_loop_proxy_->BelongsToCurrentThread());

  if (base::PLATFORM_FILE_OK != error_code) {
    error_code_ = error_code;
    RemoveTempFile();
    core_->delegate_loop_proxy_->PostTask(
        FROM_HERE,
        NewRunnableMethod(core_, &Core::InformDelegateFetchIsComplete));
    return;
  }

  total_bytes_written_ += bytes_written;
  buffer_offset_ += bytes_written;
  pending_bytes_ -= bytes_written;

  if (pending_bytes_ > 0) {
    base::FileUtilProxy::Write(
        file_message_loop_proxy_,
        temp_file_handle_,
        total_bytes_written_,  // Append to the end
        (core_->buffer_->data() + buffer_offset_),
        pending_bytes_,
        callback_factory_.NewCallback(
            &URLFetcher::Core::TempFileWriter::ContinueWrite));
  } else {
    // Finished writing core_->buffer_ to the file. Read some more.
    core_->ReadResponse();
  }
}

void URLFetcher::Core::TempFileWriter::DisownTempFile() {
  DCHECK(core_->io_message_loop_proxy_->BelongsToCurrentThread());

  // Disowning is done by the delegate's OnURLFetchComplete method.
  // The temp file should be closed by the time that method is called.
  DCHECK(temp_file_handle_ == base::kInvalidPlatformFileValue);

  // Forget about any temp file by reseting the path.
  temp_file_ = FilePath();
}

void URLFetcher::Core::TempFileWriter::CloseTempFileAndCompleteRequest() {
  DCHECK(core_->io_message_loop_proxy_->BelongsToCurrentThread());

  if (temp_file_handle_ != base::kInvalidPlatformFileValue) {
    base::FileUtilProxy::Close(
        file_message_loop_proxy_,
        temp_file_handle_,
        callback_factory_.NewCallback(
            &URLFetcher::Core::TempFileWriter::DidCloseTempFile));
    temp_file_handle_ = base::kInvalidPlatformFileValue;
  }
}

void URLFetcher::Core::TempFileWriter::DidCloseTempFile(
  base::PlatformFileError error_code) {
  DCHECK(core_->io_message_loop_proxy_->BelongsToCurrentThread());

  if (base::PLATFORM_FILE_OK != error_code) {
    error_code_ = error_code;
    RemoveTempFile();
    core_->delegate_loop_proxy_->PostTask(
        FROM_HERE,
        NewRunnableMethod(core_, &Core::InformDelegateFetchIsComplete));
    return;
  }

  // If the file was successfully closed, then the URL request is complete.
  core_->RetryOrCompleteUrlFetch();
}

void URLFetcher::Core::TempFileWriter::RemoveTempFile() {
  DCHECK(core_->io_message_loop_proxy_->BelongsToCurrentThread());

  // Close the temp file if it is open.
  if (temp_file_handle_ != base::kInvalidPlatformFileValue) {
    base::FileUtilProxy::Close(
        file_message_loop_proxy_,
        temp_file_handle_,
        NULL);  // No callback: Ignore errors.
    temp_file_handle_ = base::kInvalidPlatformFileValue;
  }

  if (!temp_file_.empty()) {
    base::FileUtilProxy::Delete(
        file_message_loop_proxy_,
        temp_file_,
        false,  // No need to recurse, as the path is to a file.
        NULL);  // No callback: Ignore errors.
    DisownTempFile();
  }
}

// static
URLFetcher::Factory* URLFetcher::factory_ = NULL;

void URLFetcher::Delegate::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  NOTREACHED() << "If you don't implemnt this, the no-params version "
               << "should also be implemented, in which case this "
               << "method won't be called...";
}

// TODO(skerner): This default implementation will be removed, and the
// method made pure virtual, once all users of URLFetcher are updated
// to not expect response data as a string argument.  Once this is removed,
// the method URLFetcher::GetResponseStringRef() can be removed as well.
// crbug.com/83592 tracks this.
void URLFetcher::Delegate::OnURLFetchComplete(const URLFetcher* source) {
  // A delegate that did not override this method is using the old
  // parameter list to OnURLFetchComplete(). If a user asked to save
  // the response to a file, they must use the new parameter list,
  // in which case we can not get here.
  // To avoid updating all callers, thunk to the old prototype for now.
  OnURLFetchComplete(source,
                     source->url(),
                     source->status(),
                     source->response_code(),
                     source->cookies(),
                     source->GetResponseStringRef());
}

// static
bool URLFetcher::g_interception_enabled = false;

URLFetcher::URLFetcher(const GURL& url,
                       RequestType request_type,
                       Delegate* d)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
      core_(new Core(this, url, request_type, d))) {
}

URLFetcher::~URLFetcher() {
  core_->Stop();
}

// static
URLFetcher* URLFetcher::Create(int id, const GURL& url,
                               RequestType request_type, Delegate* d) {
  return factory_ ? factory_->CreateURLFetcher(id, url, request_type, d) :
                    new URLFetcher(url, request_type, d);
}

URLFetcher::Core::Core(URLFetcher* fetcher,
                       const GURL& original_url,
                       RequestType request_type,
                       URLFetcher::Delegate* d)
    : fetcher_(fetcher),
      original_url_(original_url),
      request_type_(request_type),
      delegate_(d),
      delegate_loop_proxy_(
          base::MessageLoopProxy::current()),
      request_(NULL),
      load_flags_(net::LOAD_NORMAL),
      response_code_(URLFetcher::kInvalidHttpResponseCode),
      buffer_(new net::IOBuffer(kBufferSize)),
      was_fetched_via_proxy_(false),
      is_chunked_upload_(false),
      num_retries_(0),
      was_cancelled_(false),
      response_destination_(STRING),
      automatically_retry_on_5xx_(true),
      max_retries_(0) {
}

URLFetcher::Core::~Core() {
  // |request_| should be NULL.  If not, it's unsafe to delete it here since we
  // may not be on the IO thread.
  DCHECK(!request_.get());
}

void URLFetcher::Core::Start() {
  DCHECK(delegate_loop_proxy_);
  CHECK(request_context_getter_) << "We need an URLRequestContext!";
  if (io_message_loop_proxy_) {
    DCHECK_EQ(io_message_loop_proxy_,
              request_context_getter_->GetIOMessageLoopProxy());
  } else {
    io_message_loop_proxy_ = request_context_getter_->GetIOMessageLoopProxy();
  }
  CHECK(io_message_loop_proxy_.get()) << "We need an IO message loop proxy";

  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &Core::StartOnIOThread));
}

void URLFetcher::Core::StartOnIOThread() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  switch (response_destination_) {
    case STRING:
      StartURLRequestWhenAppropriate();
      break;

    case TEMP_FILE:
      DCHECK(file_message_loop_proxy_.get())
        << "Need to set the file message loop proxy.";

      temp_file_writer_.reset(
          new TempFileWriter(this, file_message_loop_proxy_));

      // If the temp file is successfully created,
      // Core::StartURLRequestWhenAppropriate() will be called.
      temp_file_writer_->CreateTempFile();
      break;

    default:
      NOTREACHED();
  }
}

void URLFetcher::Core::Stop() {
  DCHECK(delegate_loop_proxy_->BelongsToCurrentThread());
  delegate_ = NULL;
  fetcher_ = NULL;
  if (io_message_loop_proxy_.get()) {
    io_message_loop_proxy_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &Core::CancelURLRequest));
  }
}

void URLFetcher::Core::ReceivedContentWasMalformed() {
  DCHECK(delegate_loop_proxy_->BelongsToCurrentThread());
  if (io_message_loop_proxy_.get()) {
    io_message_loop_proxy_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &Core::NotifyMalformedContent));
  }
}

void URLFetcher::Core::CancelAll() {
  g_registry.Get().CancelAll();
}

void URLFetcher::Core::OnResponseStarted(net::URLRequest* request) {
  DCHECK_EQ(request, request_.get());
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  if (request_->status().is_success()) {
    response_code_ = request_->GetResponseCode();
    response_headers_ = request_->response_headers();
    socket_address_ = request_->GetSocketAddress();
    was_fetched_via_proxy_ = request_->was_fetched_via_proxy();
  }

  ReadResponse();
}

void URLFetcher::Core::CompleteAddingUploadDataChunk(
    const std::string& content, bool is_last_chunk) {
  DCHECK(is_chunked_upload_);
  DCHECK(request_.get());
  DCHECK(!content.empty());
  request_->AppendChunkToUpload(content.data(),
                                static_cast<int>(content.length()),
                                is_last_chunk);
}

void URLFetcher::Core::AppendChunkToUpload(const std::string& content,
                                           bool is_last_chunk) {
  DCHECK(delegate_loop_proxy_);
  CHECK(io_message_loop_proxy_.get());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &Core::CompleteAddingUploadDataChunk, content,
                        is_last_chunk));
}

// Return true if the write was done and reading may continue.
// Return false if the write is pending, and the next read will
// be done later.
bool URLFetcher::Core::WriteBuffer(int num_bytes) {
  bool write_complete = false;
  switch (response_destination_) {
    case STRING:
      data_.append(buffer_->data(), num_bytes);
      write_complete = true;
      break;

    case TEMP_FILE:
      temp_file_writer_->WriteBuffer(num_bytes);
      // WriteBuffer() sends a request the file thread.
      // The write is not done yet.
      write_complete = false;
      break;

    default:
      NOTREACHED();
  }
  return write_complete;
}

void URLFetcher::Core::OnReadCompleted(net::URLRequest* request,
                                       int bytes_read) {
  DCHECK(request == request_);
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  url_ = request->url();
  url_throttler_entry_ =
      net::URLRequestThrottlerManager::GetInstance()->RegisterRequestUrl(url_);

  bool waiting_on_write = false;
  do {
    if (!request_->status().is_success() || bytes_read <= 0)
      break;

    if (!WriteBuffer(bytes_read)) {
      // If WriteBuffer() returns false, we have a pending write to
      // wait on before reading further.
      waiting_on_write = true;
      break;
    }
  } while (request_->Read(buffer_, kBufferSize, &bytes_read));

  const net::URLRequestStatus status = request_->status();

  if (status.is_success())
    request_->GetResponseCookies(&cookies_);

  // See comments re: HEAD requests in ReadResponse().
  if ((!status.is_io_pending() && !waiting_on_write) ||
      (request_type_ == HEAD)) {
    status_ = status;
    ReleaseRequest();

    // If a temp file is open, close it.
    if (temp_file_writer_.get()) {
      // If the file is open, close it.  After closing the file,
      // RetryOrCompleteUrlFetch() will be called.
      temp_file_writer_->CloseTempFileAndCompleteRequest();
    } else {
      // Otherwise, complete or retry the URL request directly.
      RetryOrCompleteUrlFetch();
    }
  }
}

void URLFetcher::Core::RetryOrCompleteUrlFetch() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  base::TimeDelta backoff_delay;

  // Checks the response from server.
  if (response_code_ >= 500 ||
      status_.error() == net::ERR_TEMPORARILY_THROTTLED) {
    // When encountering a server error, we will send the request again
    // after backoff time.
    ++num_retries_;

    // Note that backoff_delay_ may be 0 because (a) the URLRequestThrottler
    // code does not necessarily back off on the first error, and (b) it
    // only backs off on some of the 5xx status codes.
    base::TimeTicks backoff_release_time = GetBackoffReleaseTime();
    backoff_delay = backoff_release_time - base::TimeTicks::Now();
    if (backoff_delay < base::TimeDelta())
      backoff_delay = base::TimeDelta();

    if (automatically_retry_on_5xx_ &&
        num_retries_ <= max_retries_) {
      StartOnIOThread();
      return;
    }
  } else {
    backoff_delay = base::TimeDelta();
  }
  request_context_getter_ = NULL;
  bool posted = delegate_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &Core::OnCompletedURLRequest,
                        backoff_delay));

  // If the delegate message loop does not exist any more, then the delegate
  // should be gone too.
  DCHECK(posted || !delegate_);
}

void URLFetcher::Core::ReadResponse() {
  // Some servers may treat HEAD requests as GET requests.  To free up the
  // network connection as soon as possible, signal that the request has
  // completed immediately, without trying to read any data back (all we care
  // about is the response code and headers, which we already have).
  int bytes_read = 0;
  if (request_->status().is_success() && (request_type_ != HEAD))
    request_->Read(buffer_, kBufferSize, &bytes_read);
  OnReadCompleted(request_.get(), bytes_read);
}

void URLFetcher::Core::DisownTempFile() {
  temp_file_writer_->DisownTempFile();
}

void URLFetcher::Core::StartURLRequest() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (was_cancelled_) {
    // Since StartURLRequest() is posted as a *delayed* task, it may
    // run after the URLFetcher was already stopped.
    return;
  }

  CHECK(request_context_getter_);
  DCHECK(!request_.get());

  g_registry.Get().AddURLFetcherCore(this);
  request_.reset(new net::URLRequest(original_url_, this));
  int flags = request_->load_flags() | load_flags_;
  if (!g_interception_enabled) {
    flags = flags | net::LOAD_DISABLE_INTERCEPT;
  }
  if (is_chunked_upload_)
    request_->EnableChunkedUpload();
  request_->set_load_flags(flags);
  request_->set_context(request_context_getter_->GetURLRequestContext());
  request_->set_referrer(referrer_);

  switch (request_type_) {
    case GET:
      break;

    case POST:
      DCHECK(!upload_content_.empty() || is_chunked_upload_);
      DCHECK(!upload_content_type_.empty());

      request_->set_method("POST");
      extra_request_headers_.SetHeader(net::HttpRequestHeaders::kContentType,
                                       upload_content_type_);
      if (!upload_content_.empty()) {
        request_->AppendBytesToUpload(
            upload_content_.data(), static_cast<int>(upload_content_.length()));
      }
      break;

    case HEAD:
      request_->set_method("HEAD");
      break;

    default:
      NOTREACHED();
  }

  if (!extra_request_headers_.IsEmpty())
    request_->SetExtraRequestHeaders(extra_request_headers_);

  // There might be data left over from a previous request attempt.
  data_.clear();

  // If we are writing the response to a file, the only caller
  // of this function should have created it and not written yet.
  CHECK(!temp_file_writer_.get() ||
        temp_file_writer_->total_bytes_written() == 0);

  request_->Start();
}

void URLFetcher::Core::StartURLRequestWhenAppropriate() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (was_cancelled_)
    return;

  if (original_url_throttler_entry_ == NULL) {
    original_url_throttler_entry_ =
        net::URLRequestThrottlerManager::GetInstance()->RegisterRequestUrl(
            original_url_);
  }

  int64 delay = original_url_throttler_entry_->ReserveSendingTimeForNextRequest(
      GetBackoffReleaseTime());
  if (delay == 0) {
    StartURLRequest();
  } else {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        NewRunnableMethod(this, &Core::StartURLRequest),
        delay);
  }
}

void URLFetcher::Core::CancelURLRequest() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (request_.get()) {
    request_->Cancel();
    ReleaseRequest();
  }
  // Release the reference to the request context. There could be multiple
  // references to URLFetcher::Core at this point so it may take a while to
  // delete the object, but we cannot delay the destruction of the request
  // context.
  request_context_getter_ = NULL;
  was_cancelled_ = true;
  temp_file_writer_.reset();
}

void URLFetcher::Core::OnCompletedURLRequest(
    base::TimeDelta backoff_delay) {
  DCHECK(delegate_loop_proxy_->BelongsToCurrentThread());

  // Save the status and backoff_delay so that delegates can read it.
  if (delegate_) {
    fetcher_->backoff_delay_ = backoff_delay;
    InformDelegateFetchIsComplete();
  }
}

void URLFetcher::Core::InformDelegateFetchIsComplete() {
  CHECK(delegate_loop_proxy_->BelongsToCurrentThread());
  if (delegate_) {
    delegate_->OnURLFetchComplete(fetcher_);
  }
}

void URLFetcher::Core::NotifyMalformedContent() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  if (url_throttler_entry_ != NULL) {
    int status_code = response_code_;
    if (status_code == kInvalidHttpResponseCode) {
      // The status code will generally be known by the time clients
      // call the |ReceivedContentWasMalformed()| function (which ends up
      // calling the current function) but if it's not, we need to assume
      // the response was successful so that the total failure count
      // used to calculate exponential back-off goes up.
      status_code = 200;
    }
    url_throttler_entry_->ReceivedContentWasMalformed(status_code);
  }
}

void URLFetcher::Core::ReleaseRequest() {
  request_.reset();
  g_registry.Get().RemoveURLFetcherCore(this);
}

base::TimeTicks URLFetcher::Core::GetBackoffReleaseTime() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(original_url_throttler_entry_ != NULL);

  base::TimeTicks original_url_backoff =
      original_url_throttler_entry_->GetExponentialBackoffReleaseTime();
  base::TimeTicks destination_url_backoff;
  if (url_throttler_entry_ != NULL &&
      original_url_throttler_entry_ != url_throttler_entry_) {
    destination_url_backoff =
        url_throttler_entry_->GetExponentialBackoffReleaseTime();
  }

  return original_url_backoff > destination_url_backoff ?
      original_url_backoff : destination_url_backoff;
}

void URLFetcher::set_upload_data(const std::string& upload_content_type,
                                 const std::string& upload_content) {
  DCHECK(!core_->is_chunked_upload_);
  core_->upload_content_type_ = upload_content_type;
  core_->upload_content_ = upload_content;
}

void URLFetcher::set_chunked_upload(const std::string& content_type) {
  DCHECK(core_->is_chunked_upload_ ||
         (core_->upload_content_type_.empty() &&
          core_->upload_content_.empty()));
  core_->upload_content_type_ = content_type;
  core_->upload_content_.clear();
  core_->is_chunked_upload_ = true;
}

void URLFetcher::AppendChunkToUpload(const std::string& data,
                                     bool is_last_chunk) {
  DCHECK(data.length());
  core_->AppendChunkToUpload(data, is_last_chunk);
}

const std::string& URLFetcher::upload_data() const {
  return core_->upload_content_;
}

void URLFetcher::set_referrer(const std::string& referrer) {
  core_->referrer_ = referrer;
}

void URLFetcher::set_load_flags(int load_flags) {
  core_->load_flags_ = load_flags;
}

int URLFetcher::load_flags() const {
  return core_->load_flags_;
}

void URLFetcher::set_extra_request_headers(
    const std::string& extra_request_headers) {
  core_->extra_request_headers_.Clear();
  core_->extra_request_headers_.AddHeadersFromString(extra_request_headers);
}

void URLFetcher::GetExtraRequestHeaders(net::HttpRequestHeaders* headers) {
  headers->CopyFrom(core_->extra_request_headers_);
}

void URLFetcher::set_request_context(
    net::URLRequestContextGetter* request_context_getter) {
  DCHECK(!core_->request_context_getter_);
  core_->request_context_getter_ = request_context_getter;
}

void URLFetcher::set_automatically_retry_on_5xx(bool retry) {
  core_->automatically_retry_on_5xx_ = retry;
}

int URLFetcher::max_retries() const {
  return core_->max_retries_;
}

void URLFetcher::set_max_retries(int max_retries) {
  core_->max_retries_ = max_retries;
}

void URLFetcher::SaveResponseToTemporaryFile(
    scoped_refptr<base::MessageLoopProxy> file_message_loop_proxy) {
  core_->file_message_loop_proxy_ = file_message_loop_proxy;
  core_->response_destination_ = TEMP_FILE;
}

net::HttpResponseHeaders* URLFetcher::response_headers() const {
  return core_->response_headers_;
}

// TODO(panayiotis): socket_address_ is written in the IO thread,
// if this is accessed in the UI thread, this could result in a race.
// Same for response_headers_ above and was_fetched_via_proxy_ below.
net::HostPortPair URLFetcher::socket_address() const {
  return core_->socket_address_;
}

bool URLFetcher::was_fetched_via_proxy() const {
  return core_->was_fetched_via_proxy_;
}

void URLFetcher::Start() {
  core_->Start();
}

void URLFetcher::StartWithRequestContextGetter(
    net::URLRequestContextGetter* request_context_getter) {
  set_request_context(request_context_getter);
  core_->Start();
}

const GURL& URLFetcher::original_url() const {
  return core_->original_url_;
}

const GURL& URLFetcher::url() const {
  return core_->url_;
}

const net::URLRequestStatus& URLFetcher::status() const {
  return core_->status_;
}

int URLFetcher::response_code() const {
  return core_->response_code_;
}

const net::ResponseCookies& URLFetcher::cookies() const {
  return core_->cookies_;
}

bool URLFetcher::FileErrorOccurred(
    base::PlatformFileError* out_error_code) const {

  // Can't have a file error if no file is being created or written to.
  if (!core_->temp_file_writer_.get()) {
    return false;
  }

  base::PlatformFileError error_code = core_->temp_file_writer_->error_code();
  if (error_code == base::PLATFORM_FILE_OK)
    return false;

  *out_error_code = error_code;
  return true;
}

void URLFetcher::ReceivedContentWasMalformed() {
  core_->ReceivedContentWasMalformed();
}

bool URLFetcher::GetResponseAsString(std::string* out_response_string) const {
  if (core_->response_destination_ != STRING)
    return false;

  *out_response_string = core_->data_;
  return true;
}

const std::string& URLFetcher::GetResponseStringRef() const {
  CHECK(core_->response_destination_ == STRING);
  return core_->data_;
}

void URLFetcher::SetResponseDestinationForTesting(
    ResponseDestinationType value) {
  core_->response_destination_ = value;
}

URLFetcher::ResponseDestinationType
URLFetcher::GetResponseDestinationForTesting() const {
  return core_->response_destination_;
}

bool URLFetcher::GetResponseAsFilePath(bool take_ownership,
                                       FilePath* out_response_path) const {
  DCHECK(core_->delegate_loop_proxy_->BelongsToCurrentThread());
  if (core_->response_destination_ != TEMP_FILE ||
      !core_->temp_file_writer_.get())
    return false;

  *out_response_path = core_->temp_file_writer_->temp_file();

  if (take_ownership) {
    core_->io_message_loop_proxy_->PostTask(
        FROM_HERE,
        NewRunnableMethod(core_.get(), &Core::DisownTempFile));
  }
  return true;
}

// static
void URLFetcher::CancelAll() {
  Core::CancelAll();
}

// static
int URLFetcher::GetNumFetcherCores() {
  return Core::g_registry.Get().size();
}

URLFetcher::Delegate* URLFetcher::delegate() const {
  return core_->delegate();
}
