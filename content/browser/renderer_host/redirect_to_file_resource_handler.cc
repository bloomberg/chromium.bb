// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/redirect_to_file_resource_handler.h"

#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/task.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/common/resource_response.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/base/net_errors.h"
#include "webkit/blob/deletable_file_reference.h"

using webkit_blob::DeletableFileReference;

// TODO(darin): Use the buffer sizing algorithm from AsyncResourceHandler.
static const int kReadBufSize = 32768;

RedirectToFileResourceHandler::RedirectToFileResourceHandler(
    ResourceHandler* next_handler,
    int process_id,
    ResourceDispatcherHost* host)
    : callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      host_(host),
      next_handler_(next_handler),
      process_id_(process_id),
      request_id_(-1),
      buf_(new net::GrowableIOBuffer()),
      buf_write_pending_(false),
      write_cursor_(0),
      write_callback_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
                      &RedirectToFileResourceHandler::DidWriteToFile),
      write_callback_pending_(false) {
}

bool RedirectToFileResourceHandler::OnUploadProgress(int request_id,
                                                     uint64 position,
                                                     uint64 size) {
  return next_handler_->OnUploadProgress(request_id, position, size);
}

bool RedirectToFileResourceHandler::OnRequestRedirected(
    int request_id,
    const GURL& new_url,
    ResourceResponse* response,
    bool* defer) {
  return next_handler_->OnRequestRedirected(request_id, new_url, response,
                                            defer);
}

bool RedirectToFileResourceHandler::OnResponseStarted(
    int request_id,
    ResourceResponse* response) {
  if (response->response_head.status.is_success()) {
    DCHECK(deletable_file_ && !deletable_file_->path().empty());
    response->response_head.download_file_path = deletable_file_->path();
  }
  return next_handler_->OnResponseStarted(request_id, response);
}

bool RedirectToFileResourceHandler::OnWillStart(int request_id,
                                                const GURL& url,
                                                bool* defer) {
  request_id_ = request_id;
  if (!deletable_file_) {
    // Defer starting the request until we have created the temporary file.
    // TODO(darin): This is sub-optimal.  We should not delay starting the
    // network request like this.
    *defer = true;
    base::FileUtilProxy::CreateTemporary(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
        callback_factory_.NewCallback(
            &RedirectToFileResourceHandler::DidCreateTemporaryFile));
    return true;
  }
  return next_handler_->OnWillStart(request_id, url, defer);
}

bool RedirectToFileResourceHandler::OnWillRead(int request_id,
                                               net::IOBuffer** buf,
                                               int* buf_size,
                                               int min_size) {
  DCHECK(min_size == -1);

  if (!buf_->capacity())
    buf_->SetCapacity(kReadBufSize);

  // We should have paused this network request already if the buffer is full.
  DCHECK(!BufIsFull());

  *buf = buf_;
  *buf_size = buf_->RemainingCapacity();

  buf_write_pending_ = true;
  return true;
}

bool RedirectToFileResourceHandler::OnReadCompleted(int request_id,
                                                    int* bytes_read) {
  if (!buf_write_pending_) {
    // Ignore spurious OnReadCompleted!  PauseRequest(true) called from within
    // OnReadCompleted tells the ResourceDispatcherHost that we did not consume
    // the data.  PauseRequest(false) then repeats the last OnReadCompleted
    // call.  We pause the request so that we can copy our buffer to disk, so
    // we need to consume the data now.  The ResourceDispatcherHost pause
    // mechanism does not fit our use case very well.
    // TODO(darin): Fix the ResourceDispatcherHost to avoid this hack!
    return true;
  }

  buf_write_pending_ = false;

  // We use the buffer's offset field to record the end of the buffer.

  int new_offset = buf_->offset() + *bytes_read;
  DCHECK(new_offset <= buf_->capacity());
  buf_->set_offset(new_offset);

  if (BufIsFull())
    host_->PauseRequest(process_id_, request_id, true);

  if (*bytes_read > 0)
    next_handler_->OnDataDownloaded(request_id, *bytes_read);

  return WriteMore();
}

bool RedirectToFileResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  return next_handler_->OnResponseCompleted(request_id, status, security_info);
}

void RedirectToFileResourceHandler::OnRequestClosed() {
  // We require this explicit call to Close since file_stream_ was constructed
  // directly from a PlatformFile.
  file_stream_->Close();
  file_stream_.reset();
  deletable_file_ = NULL;

  next_handler_->OnRequestClosed();
}

RedirectToFileResourceHandler::~RedirectToFileResourceHandler() {
  DCHECK(!file_stream_.get());
}

void RedirectToFileResourceHandler::DidCreateTemporaryFile(
    base::PlatformFileError /*error_code*/,
    base::PassPlatformFile file_handle,
    FilePath file_path) {
  deletable_file_ = DeletableFileReference::GetOrCreate(
      file_path,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  file_stream_.reset(new net::FileStream(file_handle.ReleaseValue(),
                                         base::PLATFORM_FILE_WRITE |
                                         base::PLATFORM_FILE_ASYNC));
  host_->RegisterDownloadedTempFile(
      process_id_, request_id_, deletable_file_.get());
  host_->StartDeferredRequest(process_id_, request_id_);
}

void RedirectToFileResourceHandler::DidWriteToFile(int result) {
  write_callback_pending_ = false;

  bool failed = false;
  if (result > 0) {
    write_cursor_ += result;
    failed = !WriteMore();
  } else {
    failed = true;
  }

  if (failed)
    host_->CancelRequest(process_id_, request_id_, false);
}

bool RedirectToFileResourceHandler::WriteMore() {
  DCHECK(file_stream_.get());
  for (;;) {
    if (write_cursor_ == buf_->offset()) {
      // We've caught up to the network load, but it may be in the process of
      // appending more data to the buffer.
      if (!buf_write_pending_) {
        if (BufIsFull())
          host_->PauseRequest(process_id_, request_id_, false);
        buf_->set_offset(0);
        write_cursor_ = 0;
      }
      return true;
    }
    if (write_callback_pending_)
      return true;
    DCHECK(write_cursor_ < buf_->offset());
    int rv = file_stream_->Write(buf_->StartOfBuffer() + write_cursor_,
                                 buf_->offset() - write_cursor_,
                                 &write_callback_);
    if (rv == net::ERR_IO_PENDING) {
      write_callback_pending_ = true;
      return true;
    }
    if (rv < 0)
      return false;
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
