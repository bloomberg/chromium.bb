// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_REDIRECT_TO_FILE_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_REDIRECT_TO_FILE_RESOURCE_HANDLER_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "net/url_request/url_request_status.h"

namespace net {
class FileStream;
class GrowableIOBuffer;
}

namespace webkit_blob {
class ShareableFileReference;
}

namespace content {
class ResourceDispatcherHostImpl;

// Redirects network data to a file.  This is intended to be layered in front
// of either the AsyncResourceHandler or the SyncResourceHandler.
class RedirectToFileResourceHandler : public LayeredResourceHandler {
 public:
  RedirectToFileResourceHandler(
      scoped_ptr<ResourceHandler> next_handler,
      int process_id,
      ResourceDispatcherHostImpl* resource_dispatcher_host);
  virtual ~RedirectToFileResourceHandler();

  // ResourceHandler implementation:
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response,
                                 bool* defer) OVERRIDE;
  virtual bool OnWillStart(int request_id,
                           const GURL& url,
                           bool* defer) OVERRIDE;
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int request_id,
                               int bytes_read,
                               bool* defer) OVERRIDE;
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE;

 private:
  void DidCreateTemporaryFile(base::PlatformFileError error_code,
                              base::PassPlatformFile file_handle,
                              const base::FilePath& file_path);
  void DidWriteToFile(int result);
  bool WriteMore();
  bool BufIsFull() const;
  void ResumeIfDeferred();

  base::WeakPtrFactory<RedirectToFileResourceHandler> weak_factory_;

  ResourceDispatcherHostImpl* host_;
  int process_id_;
  int request_id_;

  // We allocate a single, fixed-size IO buffer (buf_) used to read from the
  // network (buf_write_pending_ is true while the system is copying data into
  // buf_), and then write this buffer out to disk (write_callback_pending_ is
  // true while writing to disk).  Reading from the network is suspended while
  // the buffer is full (BufIsFull returns true).  The write_cursor_ member
  // tracks the offset into buf_ that we are writing to disk.

  scoped_refptr<net::GrowableIOBuffer> buf_;
  bool buf_write_pending_;
  int write_cursor_;

  scoped_ptr<net::FileStream> file_stream_;
  bool write_callback_pending_;

  // |next_buffer_size_| is the size of the buffer to be allocated on the next
  // OnWillRead() call.  We exponentially grow the size of the buffer allocated
  // when our owner fills our buffers. On the first OnWillRead() call, we
  // allocate a buffer of 32k and double it in OnReadCompleted() if the buffer
  // was filled, up to a maximum size of 512k.
  int next_buffer_size_;

  // We create a ShareableFileReference that's deletable for the temp
  // file created as  a result of the download.
  scoped_refptr<webkit_blob::ShareableFileReference> deletable_file_;

  bool did_defer_ ;

  bool completed_during_write_;
  net::URLRequestStatus completed_status_;
  std::string completed_security_info_;

  DISALLOW_COPY_AND_ASSIGN(RedirectToFileResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_REDIRECT_TO_FILE_RESOURCE_HANDLER_H_
