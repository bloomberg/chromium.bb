// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_url_data_manager_backend.h"

#include <set>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/ui/webui/shared_resources_data_source.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/url_util.h"
#include "grit/platform_locale_settings.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"

using content::BrowserThread;

namespace {

// TODO(tsepez) remove unsafe-eval when bidichecker_packaged.js fixed.
const char kChromeURLContentSecurityPolicyHeaderBase[] =
    "Content-Security-Policy: script-src chrome://resources "
    "'self' 'unsafe-eval'; ";

// TODO(tsepez) The following should be replaced with a centralized table.
// See crbug.com/104631.

// If you are inserting new exemptions into this list, then you have a bug.
// It is not acceptable to disable content-security-policy on chrome:// pages
// to permit functionality excluded by the above policy, such as inline script.
// Instead, you must go back and change your WebUI page so that it is compliant
// with the policy. This typically involves ensuring that all script is
// delivered through the data manager backend.
class ChromeURLContentSecurityPolicyExceptionSet
    : public std::set<std::string> {
 public:
  ChromeURLContentSecurityPolicyExceptionSet() : std::set<std::string>() {
    // TODO(tsepez) whittle this list down to nothing.
    insert(chrome::kChromeUICloudPrintResourcesHost);
    insert(chrome::kChromeUICloudPrintSetupHost);
    insert(chrome::kChromeUIDevToolsHost);
    insert(chrome::kChromeUIDialogHost);
    insert(chrome::kChromeUIInputWindowDialogHost);
    insert(chrome::kChromeUINewTabHost);
#if defined(OS_CHROMEOS)
    insert(chrome::kChromeUIMobileSetupHost);
    insert(chrome::kChromeUIOobeHost);
    insert(chrome::kChromeUIOSCreditsHost);
    insert(chrome::kChromeUIProxySettingsHost);
    insert(chrome::kChromeUIRegisterPageHost);
    insert(chrome::kChromeUISimUnlockHost);
    insert(chrome::kChromeUISystemInfoHost);
#endif
#if defined(OS_CHROMEOS) || defined(USE_AURA)
    insert(chrome::kChromeUICollectedCookiesHost);
    insert(chrome::kChromeUIHttpAuthHost);
    insert(chrome::kChromeUITabModalConfirmDialogHost);
#endif
  }
};

// It is OK to add URLs to these maps which map specific URLs to custom CSP
// directives thereby slightly reducing the protection applied to the page.
class ChromeURLObjectSrcExceptionMap
    : public std::map<std::string, std::string> {
 public:
  ChromeURLObjectSrcExceptionMap() : std::map<std::string, std::string>() {
    insert(std::pair<std::string, std::string>(
        chrome::kChromeUIPrintHost, "object-src 'self';"));
  }
};

class ChromeURLFrameSrcExceptionMap
    : public std::map<std::string, std::string> {
 public:
  ChromeURLFrameSrcExceptionMap() : std::map<std::string, std::string>() {
    insert(std::pair<std::string, std::string>(
        chrome::kChromeUIUberHost, "frame-src chrome:;"));
    insert(std::pair<std::string, std::string>(
        chrome::kChromeUIUberFrameHost, "frame-src chrome:;"));
  }
};

base::LazyInstance<ChromeURLContentSecurityPolicyExceptionSet>
    g_chrome_url_content_security_policy_exception_set =
        LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<ChromeURLObjectSrcExceptionMap>
    g_chrome_url_object_src_exception_map = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<ChromeURLFrameSrcExceptionMap>
    g_chrome_url_frame_src_exception_map = LAZY_INSTANCE_INITIALIZER;

// Determine the least-privileged content security policy header, if any,
// that is compatible with a given WebUI URL, and append it to the existing
// response headers.
void AddContentSecurityPolicyHeader(
    const GURL& url, net::HttpResponseHeaders* headers) {
  ChromeURLContentSecurityPolicyExceptionSet* exceptions =
      g_chrome_url_content_security_policy_exception_set.Pointer();

  if (exceptions->find(url.host()) == exceptions->end()) {
    std::string base = kChromeURLContentSecurityPolicyHeaderBase;

    ChromeURLObjectSrcExceptionMap* object_map =
        g_chrome_url_object_src_exception_map.Pointer();
    ChromeURLObjectSrcExceptionMap::iterator object_iter =
        object_map->find(url.host());
    base.append(object_iter == object_map->end() ?
                "object-src 'none';" : object_iter->second);

    ChromeURLFrameSrcExceptionMap* frame_map =
        g_chrome_url_frame_src_exception_map.Pointer();
    ChromeURLFrameSrcExceptionMap::iterator frame_iter =
        frame_map->find(url.host());
    base.append(frame_iter == frame_map->end() ?
                "frame-src 'none';" : frame_iter->second);

    headers->AddHeader(base);
  }
}

const char kChromeURLXFrameOptionsHeader[] = "X-Frame-Options: DENY";

// It is OK to add exceptions to this set as needed.
class ChromeURLXFrameOptionsExceptionSet
    : public std::set<std::string> {
 public:
  ChromeURLXFrameOptionsExceptionSet() : std::set<std::string>() {
    insert(chrome::kChromeUIExtensionsFrameHost);
    insert(chrome::kChromeUIHelpFrameHost);
    insert(chrome::kChromeUIHistoryFrameHost);
    insert(chrome::kChromeUISettingsFrameHost);
    insert(chrome::kChromeUIUberFrameHost);
#if defined(OS_CHROMEOS)
    // chrome://terms page is embedded in iframe to chrome://oobe.
    insert(chrome::kChromeUITermsHost);
#endif
  }
};

base::LazyInstance<ChromeURLXFrameOptionsExceptionSet>
    g_chrome_url_x_frame_options_exception_set = LAZY_INSTANCE_INITIALIZER;

void AddXFrameOptionsHeader(
    const GURL& url, net::HttpResponseHeaders* headers) {
  ChromeURLXFrameOptionsExceptionSet* exceptions =
      g_chrome_url_x_frame_options_exception_set.Pointer();
  if (exceptions->find(url.host()) == exceptions->end())
    headers->AddHeader(kChromeURLXFrameOptionsHeader);
}

// Parse a URL into the components used to resolve its request. |source_name|
// is the hostname and |path| is the remaining portion of the URL.
void URLToRequest(const GURL& url, std::string* source_name,
                  std::string* path) {
  DCHECK(url.SchemeIs(chrome::kChromeDevToolsScheme) ||
         url.SchemeIs(chrome::kChromeUIScheme));

  if (!url.is_valid()) {
    NOTREACHED();
    return;
  }

  // Our input looks like: chrome://source_name/extra_bits?foo .
  // So the url's "host" is our source, and everything after the host is
  // the path.
  source_name->assign(url.host());

  const std::string& spec = url.possibly_invalid_spec();
  const url_parse::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  // + 1 to skip the slash at the beginning of the path.
  int offset = parsed.CountCharactersBefore(url_parse::Parsed::PATH, false) + 1;

  if (offset < static_cast<int>(spec.size()))
    path->assign(spec.substr(offset));
}

}  // namespace

// URLRequestChromeJob is a net::URLRequestJob that manages running
// chrome-internal resource requests asynchronously.
// It hands off URL requests to ChromeURLDataManager, which asynchronously
// calls back once the data is available.
class URLRequestChromeJob : public net::URLRequestJob,
                            public base::SupportsWeakPtr<URLRequestChromeJob> {
 public:
  // |is_incognito| set when job is generated from an incognito profile.
  URLRequestChromeJob(net::URLRequest* request,
                      net::NetworkDelegate* network_delegate,
                      ChromeURLDataManagerBackend* backend,
                      bool is_incognito);

  // net::URLRequestJob implementation.
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;

  // Used to notify that the requested data's |mime_type| is ready.
  void MimeTypeAvailable(const std::string& mime_type);

  // Called by ChromeURLDataManager to notify us that the data blob is ready
  // for us.
  void DataAvailable(base::RefCountedMemory* bytes);

  void set_mime_type(const std::string& mime_type) {
    mime_type_ = mime_type;
  }

  void set_allow_caching(bool allow_caching) {
    allow_caching_ = allow_caching;
  }

  // Returns true when job was generated from an incognito profile.
  bool is_incognito() const {
    return is_incognito_;
  }

 private:
  virtual ~URLRequestChromeJob();

  // Helper for Start(), to let us start asynchronously.
  // (This pattern is shared by most net::URLRequestJob implementations.)
  void StartAsync();

  // Do the actual copy from data_ (the data we're serving) into |buf|.
  // Separate from ReadRawData so we can handle async I/O.
  void CompleteRead(net::IOBuffer* buf, int buf_size, int* bytes_read);

  // The actual data we're serving.  NULL until it's been fetched.
  scoped_refptr<base::RefCountedMemory> data_;
  // The current offset into the data that we're handing off to our
  // callers via the Read interfaces.
  int data_offset_;

  // For async reads, we keep around a pointer to the buffer that
  // we're reading into.
  scoped_refptr<net::IOBuffer> pending_buf_;
  int pending_buf_size_;
  std::string mime_type_;

  // If true, set a header in the response to prevent it from being cached.
  bool allow_caching_;

  // True when job is generated from an incognito profile.
  const bool is_incognito_;

  // The backend is owned by ChromeURLRequestContext and always outlives us.
  ChromeURLDataManagerBackend* backend_;

  base::WeakPtrFactory<URLRequestChromeJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestChromeJob);
};

URLRequestChromeJob::URLRequestChromeJob(net::URLRequest* request,
                                         net::NetworkDelegate* network_delegate,
                                         ChromeURLDataManagerBackend* backend,
                                         bool is_incognito)
    : net::URLRequestJob(request, network_delegate),
      data_offset_(0),
      pending_buf_size_(0),
      allow_caching_(true),
      is_incognito_(is_incognito),
      backend_(backend),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(backend);
}

URLRequestChromeJob::~URLRequestChromeJob() {
  CHECK(!backend_->HasPendingJob(this));
}

void URLRequestChromeJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestChromeJob::StartAsync,
                 weak_factory_.GetWeakPtr()));

  TRACE_EVENT_ASYNC_BEGIN1("browser", "DataManager:Request", this, "URL",
      request_->url().possibly_invalid_spec());
}

void URLRequestChromeJob::Kill() {
  backend_->RemoveRequest(this);
}

bool URLRequestChromeJob::GetMimeType(std::string* mime_type) const {
  *mime_type = mime_type_;
  return !mime_type_.empty();
}

void URLRequestChromeJob::GetResponseInfo(net::HttpResponseInfo* info) {
  DCHECK(!info->headers);
  // Set the headers so that requests serviced by ChromeURLDataManager return a
  // status code of 200. Without this they return a 0, which makes the status
  // indistiguishable from other error types. Instant relies on getting a 200.
  info->headers = new net::HttpResponseHeaders("HTTP/1.1 200 OK");
  AddContentSecurityPolicyHeader(request_->url(), info->headers);
  AddXFrameOptionsHeader(request_->url(), info->headers);
  if (!allow_caching_)
    info->headers->AddHeader("Cache-Control: no-cache");
}

void URLRequestChromeJob::MimeTypeAvailable(const std::string& mime_type) {
  set_mime_type(mime_type);
  NotifyHeadersComplete();
}

void URLRequestChromeJob::DataAvailable(base::RefCountedMemory* bytes) {
  TRACE_EVENT_ASYNC_END0("browser", "DataManager:Request", this);
  if (bytes) {
    // The request completed, and we have all the data.
    // Clear any IO pending status.
    SetStatus(net::URLRequestStatus());

    data_ = bytes;
    int bytes_read;
    if (pending_buf_.get()) {
      CHECK(pending_buf_->data());
      CompleteRead(pending_buf_, pending_buf_size_, &bytes_read);
      pending_buf_ = NULL;
      NotifyReadComplete(bytes_read);
    }
  } else {
    // The request failed.
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                     net::ERR_FAILED));
  }
}

bool URLRequestChromeJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                      int* bytes_read) {
  if (!data_.get()) {
    SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
    DCHECK(!pending_buf_.get());
    CHECK(buf->data());
    pending_buf_ = buf;
    pending_buf_size_ = buf_size;
    return false;  // Tell the caller we're still waiting for data.
  }

  // Otherwise, the data is available.
  CompleteRead(buf, buf_size, bytes_read);
  return true;
}

void URLRequestChromeJob::CompleteRead(net::IOBuffer* buf, int buf_size,
                                       int* bytes_read) {
  int remaining = static_cast<int>(data_->size()) - data_offset_;
  if (buf_size > remaining)
    buf_size = remaining;
  if (buf_size > 0) {
    memcpy(buf->data(), data_->front() + data_offset_, buf_size);
    data_offset_ += buf_size;
  }
  *bytes_read = buf_size;
}

void URLRequestChromeJob::StartAsync() {
  if (!request_)
    return;

  if (!backend_->StartRequest(request_->url(), this)) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_INVALID_URL));
  }
}

namespace {

// Gets mime type for data that is available from |source| by |path|.
// After that, notifies |job| that mime type is available. This method
// should be called on the UI thread, but notification is performed on
// the IO thread.
void GetMimeTypeOnUI(URLDataSourceImpl* source,
                     const std::string& path,
                     const base::WeakPtr<URLRequestChromeJob>& job) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string mime_type = source->source()->GetMimeType(path);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&URLRequestChromeJob::MimeTypeAvailable, job, mime_type));
}

}  // namespace

namespace {

class ChromeProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // |is_incognito| should be set for incognito profiles.
  explicit ChromeProtocolHandler(ChromeURLDataManagerBackend* backend,
                                 bool is_incognito);
  ~ChromeProtocolHandler();

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;

 private:
  // These members are owned by ProfileIOData, which owns this ProtocolHandler.
  ChromeURLDataManagerBackend* const backend_;

  // True when generated from an incognito profile.
  const bool is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(ChromeProtocolHandler);
};

ChromeProtocolHandler::ChromeProtocolHandler(
    ChromeURLDataManagerBackend* backend, bool is_incognito)
    : backend_(backend), is_incognito_(is_incognito) {}

ChromeProtocolHandler::~ChromeProtocolHandler() {}

net::URLRequestJob* ChromeProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  DCHECK(request);

  // Fall back to using a custom handler
  return new URLRequestChromeJob(request, network_delegate, backend_,
                                 is_incognito_);
}

}  // namespace

ChromeURLDataManagerBackend::ChromeURLDataManagerBackend()
    : next_request_id_(0) {
  content::URLDataSource* shared_source = new SharedResourcesDataSource();
  URLDataSourceImpl* source_impl =
      new URLDataSourceImpl(shared_source->GetSource(), shared_source);
  AddDataSource(source_impl);
}

ChromeURLDataManagerBackend::~ChromeURLDataManagerBackend() {
  for (DataSourceMap::iterator i = data_sources_.begin();
       i != data_sources_.end(); ++i) {
    i->second->backend_ = NULL;
  }
  data_sources_.clear();
}

// static
net::URLRequestJobFactory::ProtocolHandler*
ChromeURLDataManagerBackend::CreateProtocolHandler(
    ChromeURLDataManagerBackend* backend, bool is_incognito) {
  DCHECK(backend);
  return new ChromeProtocolHandler(backend, is_incognito);
}

void ChromeURLDataManagerBackend::AddDataSource(
    URLDataSourceImpl* source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DataSourceMap::iterator i = data_sources_.find(source->source_name());
  if (i != data_sources_.end()) {
    if (!source->source()->ShouldReplaceExistingSource())
      return;
    i->second->backend_ = NULL;
  }
  data_sources_[source->source_name()] = source;
  source->backend_ = this;
}

bool ChromeURLDataManagerBackend::HasPendingJob(
    URLRequestChromeJob* job) const {
  for (PendingRequestMap::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->second == job)
      return true;
  }
  return false;
}

bool ChromeURLDataManagerBackend::StartRequest(const GURL& url,
                                               URLRequestChromeJob* job) {
  // Parse the URL into a request for a source and path.
  std::string source_name;
  std::string path;
  URLToRequest(url, &source_name, &path);

  // Look up the data source for the request.
  DataSourceMap::iterator i = data_sources_.find(source_name);
  if (i == data_sources_.end())
    return false;

  URLDataSourceImpl* source = i->second;

  // Save this request so we know where to send the data.
  RequestID request_id = next_request_id_++;
  pending_requests_.insert(std::make_pair(request_id, job));

  job->set_allow_caching(source->source()->AllowCaching());

  // Forward along the request to the data source.
  MessageLoop* target_message_loop =
      source->source()->MessageLoopForRequestPath(path);
  if (!target_message_loop) {
    job->MimeTypeAvailable(source->source()->GetMimeType(path));

    // The DataSource is agnostic to which thread StartDataRequest is called
    // on for this path.  Call directly into it from this thread, the IO
    // thread.
    source->source()->StartDataRequest(
        path, job->is_incognito(),
        base::Bind(&URLDataSourceImpl::SendResponse, source, request_id));
  } else {
    // URLRequestChromeJob should receive mime type before data. This
    // is guaranteed because request for mime type is placed in the
    // message loop before request for data. And correspondingly their
    // replies are put on the IO thread in the same order.
    target_message_loop->PostTask(
        FROM_HERE,
        base::Bind(&GetMimeTypeOnUI,
                   scoped_refptr<URLDataSourceImpl>(source),
                   path, job->AsWeakPtr()));

    // The DataSource wants StartDataRequest to be called on a specific thread,
    // usually the UI thread, for this path.
    target_message_loop->PostTask(
        FROM_HERE,
        base::Bind(&ChromeURLDataManagerBackend::CallStartRequest,
                   make_scoped_refptr(source), path, job->is_incognito(),
                   request_id));
  }
  return true;
}

void ChromeURLDataManagerBackend::CallStartRequest(
    scoped_refptr<URLDataSourceImpl> source,
    const std::string& path,
    bool is_incognito,
    int request_id) {
  source->source()->StartDataRequest(
      path,
      is_incognito,
      base::Bind(&URLDataSourceImpl::SendResponse, source, request_id));
}

void ChromeURLDataManagerBackend::RemoveRequest(URLRequestChromeJob* job) {
  // Remove the request from our list of pending requests.
  // If/when the source sends the data that was requested, the data will just
  // be thrown away.
  for (PendingRequestMap::iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->second == job) {
      pending_requests_.erase(i);
      return;
    }
  }
}

void ChromeURLDataManagerBackend::DataAvailable(RequestID request_id,
                                                base::RefCountedMemory* bytes) {
  // Forward this data on to the pending net::URLRequest, if it exists.
  PendingRequestMap::iterator i = pending_requests_.find(request_id);
  if (i != pending_requests_.end()) {
    URLRequestChromeJob* job(i->second);
    pending_requests_.erase(i);
    job->DataAvailable(bytes);
  }
}

namespace {

#if defined(DEBUG_DEVTOOLS)
bool IsSupportedDevToolsURL(const GURL& url, FilePath* path) {
  if (!url.SchemeIs(chrome::kChromeDevToolsScheme))
    return false;

  if (!url.is_valid()) {
    NOTREACHED();
    return false;
  }

  // Remove Query and Ref from URL.
  GURL stripped_url;
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  stripped_url = url.ReplaceComponents(replacements);

  std::string source_name;
  std::string relative_path;
  URLToRequest(stripped_url, &source_name, &relative_path);

  if (source_name != chrome::kChromeUIDevToolsHost)
    return false;

  // Check that |relative_path| is not an absolute path (otherwise
  // AppendASCII() will DCHECK).  The awkward use of StringType is because on
  // some systems FilePath expects a std::string, but on others a std::wstring.
  FilePath p(FilePath::StringType(relative_path.begin(), relative_path.end()));
  if (p.IsAbsolute())
    return false;

  FilePath inspector_dir;
  if (!PathService::Get(chrome::DIR_INSPECTOR, &inspector_dir))
    return false;

  if (inspector_dir.empty())
    return false;

  *path = inspector_dir.AppendASCII(relative_path);
  return true;
}
#endif  // defined(DEBUG_DEVTOOLS)

class DevToolsJobFactory
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // |is_incognito| should be set for incognito profiles.
  DevToolsJobFactory(ChromeURLDataManagerBackend* backend,
                     net::NetworkDelegate* network_delegate,
                     bool is_incognito);
  virtual ~DevToolsJobFactory();

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;

 private:
  // |backend_| and |network_delegate_| are owned by ProfileIOData, which owns
  // this ProtocolHandler.
  ChromeURLDataManagerBackend* const backend_;
  net::NetworkDelegate* network_delegate_;

  // True when generated from an incognito profile.
  const bool is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsJobFactory);
};

DevToolsJobFactory::DevToolsJobFactory(ChromeURLDataManagerBackend* backend,
                                       net::NetworkDelegate* network_delegate,
                                       bool is_incognito)
    : backend_(backend),
      network_delegate_(network_delegate),
      is_incognito_(is_incognito) {
  DCHECK(backend_);
}

DevToolsJobFactory::~DevToolsJobFactory() {}

net::URLRequestJob*
DevToolsJobFactory::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
#if defined(DEBUG_DEVTOOLS)
  FilePath path;
  if (IsSupportedDevToolsURL(request->url(), &path))
    return new net::URLRequestFileJob(request, network_delegate, path);
#endif
  return new URLRequestChromeJob(request, network_delegate, backend_,
                                 is_incognito_);
}

}  // namespace

net::URLRequestJobFactory::ProtocolHandler*
CreateDevToolsProtocolHandler(ChromeURLDataManagerBackend* backend,
                              net::NetworkDelegate* network_delegate,
                              bool is_incognito) {
  return new DevToolsJobFactory(backend, network_delegate, is_incognito);
}
