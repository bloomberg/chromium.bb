// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_REDIRECT_TO_FILE_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_REDIRECT_TO_FILE_RESOURCE_HANDLER_H_

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "content/browser/renderer_host/resource_handler.h"
#include "net/url_request/url_request_status.h"

class ResourceDispatcherHost;

namespace net {
class FileStream;
class GrowableIOBuffer;
}

namespace webkit_blob {
class DeletableFileReference;
}

// Redirects network data to a file.  This is intended to be layered in front
// of either the AsyncResourceHandler or the SyncResourceHandler.
class RedirectToFileResourceHandler : public ResourceHandler {
 public:
  RedirectToFileResourceHandler(
      ResourceHandler* next_handler,
      int process_id,
      ResourceDispatcherHost* resource_dispatcher_host);

  // ResourceHandler implementation:
  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) OVERRIDE;
  virtual bool OnRequestRedirected(int request_id,
                                   const GURL& new_url,
                                   content::ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(int request_id,
                                 content::ResourceResponse* response) OVERRIDE;
  virtual bool OnWillStart(int request_id,
                           const GURL& url,
                           bool* defer) OVERRIDE;
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int request_id,
                               int* bytes_read) OVERRIDE;
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE;
  virtual void OnRequestClosed() OVERRIDE;

 private:
  virtual ~RedirectToFileResourceHandler();
  void DidCreateTemporaryFile(base::PlatformFileError error_code,
                              base::PassPlatformFile file_handle,
                              FilePath file_path);
  void DidWriteToFile(int result);
  bool WriteMore();
  bool BufIsFull() const;

  base::WeakPtrFactory<RedirectToFileResourceHandler> weak_factory_;

  ResourceDispatcherHost* host_;
  scoped_refptr<ResourceHandler> next_handler_;
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

  // We create a DeletableFileReference for the temp file created as
  // a result of the download.
  scoped_refptr<webkit_blob::DeletableFileReference> deletable_file_;

  // True if OnRequestClosed() has already been called.
  bool request_was_closed_;

  bool completed_during_write_;
  net::URLRequestStatus completed_status_;
  std::string completed_security_info_;

  DISALLOW_COPY_AND_ASSIGN(RedirectToFileResourceHandler);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_REDIRECT_TO_FILE_RESOURCE_HANDLER_H_
