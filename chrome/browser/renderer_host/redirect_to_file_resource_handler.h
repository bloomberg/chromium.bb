// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_REDIRECT_TO_FILE_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_REDIRECT_TO_FILE_RESOURCE_HANDLER_H_

#include "base/file_path.h"
#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "base/scoped_callback_factory.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/renderer_host/resource_handler.h"
#include "net/base/completion_callback.h"

class RefCountedPlatformFile;
class ResourceDispatcherHost;

namespace net {
class FileStream;
class GrowableIOBuffer;
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
  bool OnUploadProgress(int request_id, uint64 position, uint64 size);
  bool OnRequestRedirected(int request_id, const GURL& new_url,
                           ResourceResponse* response, bool* defer);
  bool OnResponseStarted(int request_id, ResourceResponse* response);
  bool OnWillStart(int request_id, const GURL& url, bool* defer);
  bool OnWillRead(int request_id, net::IOBuffer** buf, int* buf_size,
                  int min_size);
  bool OnReadCompleted(int request_id, int* bytes_read);
  bool OnResponseCompleted(int request_id,
                           const URLRequestStatus& status,
                           const std::string& security_info);
  void OnRequestClosed();

 private:
  virtual ~RedirectToFileResourceHandler();
  void DidCreateTemporaryFile(base::PlatformFileError error_code,
                              base::PassPlatformFile file_handle,
                              FilePath file_path);
  void DidWriteToFile(int result);
  bool WriteMore();
  bool BufIsFull() const;

  base::ScopedCallbackFactory<RedirectToFileResourceHandler> callback_factory_;

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

  FilePath file_path_;
  scoped_ptr<net::FileStream> file_stream_;
  net::CompletionCallbackImpl<RedirectToFileResourceHandler> write_callback_;
  bool write_callback_pending_;

  DISALLOW_COPY_AND_ASSIGN(RedirectToFileResourceHandler);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_REDIRECT_TO_FILE_RESOURCE_HANDLER_H_
