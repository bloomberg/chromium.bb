// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/redirect_to_file_resource_handler.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/resource_response.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/base/net_errors.h"
#include "webkit/blob/shareable_file_reference.h"

using webkit_blob::ShareableFileReference;

namespace {

// This class is similar to identically named classes in AsyncResourceHandler
// and BufferedResourceHandler, but not quite.
// TODO(ncbray) generalize and unify these cases?
// In general, it's a bad idea to point to a subbuffer (particularly with
// GrowableIOBuffer) because the backing IOBuffer may realloc its data.  In this
// particular case we know RedirectToFileResourceHandler will not realloc its
// buffer while a write is occurring, so we should be safe.  This property is
// somewhat fragile, however, and depending on it is dangerous.  A more
// principled approach would require significant refactoring, however, so for
// the moment we're relying on fragile properties.
class DependentIOBuffer : public net::WrappedIOBuffer {
 public:
  DependentIOBuffer(net::IOBuffer* backing, char* memory)
      : net::WrappedIOBuffer(memory),
        backing_(backing) {
  }
 private:
  virtual ~DependentIOBuffer() {}

  scoped_refptr<net::IOBuffer> backing_;
};

}  // namespace

namespace content {

static const int kInitialReadBufSize = 32768;
static const int kMaxReadBufSize = 524288;

RedirectToFileResourceHandler::RedirectToFileResourceHandler(
    scoped_ptr<ResourceHandler> next_handler,
    int process_id,
    ResourceDispatcherHostImpl* host)
    : LayeredResourceHandler(next_handler.Pass()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      host_(host),
      process_id_(process_id),
      request_id_(-1),
      buf_(new net::GrowableIOBuffer()),
      buf_write_pending_(false),
      write_cursor_(0),
      write_callback_pending_(false),
      next_buffer_size_(kInitialReadBufSize),
      did_defer_(false),
      completed_during_write_(false) {
}

RedirectToFileResourceHandler::~RedirectToFileResourceHandler() {
}

bool RedirectToFileResourceHandler::OnResponseStarted(
    int request_id,
    ResourceResponse* response,
    bool* defer) {
  if (response->head.error_code == net::OK ||
      response->head.error_code == net::ERR_IO_PENDING) {
    DCHECK(deletable_file_ && !deletable_file_->path().empty());
    response->head.download_file_path = deletable_file_->path();
  }
  return next_handler_->OnResponseStarted(request_id, response, defer);
}

bool RedirectToFileResourceHandler::OnWillStart(int request_id,
                                                const GURL& url,
                                                bool* defer) {
  request_id_ = request_id;
  if (!deletable_file_) {
    // Defer starting the request until we have created the temporary file.
    // TODO(darin): This is sub-optimal.  We should not delay starting the
    // network request like this.
    did_defer_ = *defer = true;
    base::FileUtilProxy::CreateTemporary(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
        base::PLATFORM_FILE_ASYNC,
        base::Bind(&RedirectToFileResourceHandler::DidCreateTemporaryFile,
                   weak_factory_.GetWeakPtr()));
    return true;
  }
  return next_handler_->OnWillStart(request_id, url, defer);
}

bool RedirectToFileResourceHandler::OnWillRead(int request_id,
                                               net::IOBuffer** buf,
                                               int* buf_size,
                                               int min_size) {
  DCHECK_EQ(-1, min_size);

  if (buf_->capacity() < next_buffer_size_)
    buf_->SetCapacity(next_buffer_size_);

  // We should have paused this network request already if the buffer is full.
  DCHECK(!BufIsFull());

  *buf = buf_;
  *buf_size = buf_->RemainingCapacity();

  buf_write_pending_ = true;
  return true;
}

bool RedirectToFileResourceHandler::OnReadCompleted(int request_id,
                                                    int bytes_read,
                                                    bool* defer) {
  DCHECK(buf_write_pending_);
  buf_write_pending_ = false;

  // We use the buffer's offset field to record the end of the buffer.
  int new_offset = buf_->offset() + bytes_read;
  DCHECK(new_offset <= buf_->capacity());
  buf_->set_offset(new_offset);

  if (BufIsFull()) {
    did_defer_ = *defer = true;

    if (buf_->capacity() == bytes_read) {
      // The network layer has saturated our buffer in one read. Next time, we
      // should give it a bigger buffer for it to fill.
      next_buffer_size_ = std::min(next_buffer_size_ * 2, kMaxReadBufSize);
    }
  }

  return WriteMore();
}

bool RedirectToFileResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  if (write_callback_pending_) {
    completed_during_write_ = true;
    completed_status_ = status;
    completed_security_info_ = security_info;
    return false;
  }
  return next_handler_->OnResponseCompleted(request_id, status, security_info);
}

void RedirectToFileResourceHandler::DidCreateTemporaryFile(
    base::PlatformFileError /*error_code*/,
    base::PassPlatformFile file_handle,
    const FilePath& file_path) {
  deletable_file_ = ShareableFileReference::GetOrCreate(
      file_path, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  file_stream_.reset(new net::FileStream(file_handle.ReleaseValue(),
                                         base::PLATFORM_FILE_WRITE |
                                         base::PLATFORM_FILE_ASYNC,
                                         NULL));
  host_->RegisterDownloadedTempFile(
      process_id_, request_id_, deletable_file_.get());
  ResumeIfDeferred();
}

void RedirectToFileResourceHandler::DidWriteToFile(int result) {
  write_callback_pending_ = false;

  bool failed = false;
  if (result > 0) {
    next_handler_->OnDataDownloaded(request_id_, result);
    write_cursor_ += result;
    failed = !WriteMore();
  } else {
    failed = true;
  }

  if (failed) {
    ResumeIfDeferred();
  } else if (completed_during_write_) {
    if (next_handler_->OnResponseCompleted(request_id_, completed_status_,
                                           completed_security_info_)) {
      ResumeIfDeferred();
    }
  }
}

bool RedirectToFileResourceHandler::WriteMore() {
  DCHECK(file_stream_.get());
  for (;;) {
    if (write_cursor_ == buf_->offset()) {
      // We've caught up to the network load, but it may be in the process of
      // appending more data to the buffer.
      if (!buf_write_pending_) {
        if (BufIsFull())
          ResumeIfDeferred();
        buf_->set_offset(0);
        write_cursor_ = 0;
      }
      return true;
    }
    if (write_callback_pending_)
      return true;
    DCHECK(write_cursor_ < buf_->offset());

    // Create a temporary buffer pointing to a subsection of the data buffer so
    // that it can be passed to Write.  This code makes some crazy scary
    // assumptions about object lifetimes, thread sharing, and that buf_ will
    // not realloc durring the write due to how the state machine in this class
    // works.
    // Note that buf_ is also shared with the code that writes data into the
    // cache, so modifying it can cause some pretty subtle race conditions:
    // https://code.google.com/p/chromium/issues/detail?id=152076
    // We're using DependentIOBuffer instead of DrainableIOBuffer to dodge some
    // of these issues, for the moment.
    // TODO(ncbray) make this code less crazy scary.
    // Also note that Write may increase the refcount of "wrapped" deep in the
    // bowels of its implementation, the use of scoped_refptr here is not
    // spurious.
    scoped_refptr<DependentIOBuffer> wrapped = new DependentIOBuffer(
        buf_, buf_->StartOfBuffer() + write_cursor_);
    int write_len = buf_->offset() - write_cursor_;

    int rv = file_stream_->Write(
        wrapped,
        write_len,
        base::Bind(&RedirectToFileResourceHandler::DidWriteToFile,
                   base::Unretained(this)));
    if (rv == net::ERR_IO_PENDING) {
      write_callback_pending_ = true;
      return true;
    }
    if (rv <= 0)
      return false;
    next_handler_->OnDataDownloaded(request_id_, rv);
    write_cursor_ += rv;
  }
}

bool RedirectToFileResourceHandler::BufIsFull() const {
  // This is a hack to workaround BufferedResourceHandler's inability to
  // deal with a ResourceHandler that returns a buffer size of less than
  // 2 * net::kMaxBytesToSniff from its OnWillRead method.
  // TODO(darin): Fix this retardation!
  return buf_->RemainingCapacity() <= (2 * net::kMaxBytesToSniff);
}

void RedirectToFileResourceHandler::ResumeIfDeferred() {
  if (did_defer_) {
    did_defer_ = false;
    controller()->Resume();
  }
}

}  // namespace content
