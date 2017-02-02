// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_RESOURCE_HANDLER_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/download/save_types.h"
#include "content/browser/loader/resource_handler.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace content {
class ResourceController;
class SaveFileManager;

// Forwards data to the save thread.
class SaveFileResourceHandler : public ResourceHandler {
 public:
  // Unauthorized requests are cancelled from OnWillStart callback.
  //
  // This way of handling unauthorized requests allows unified handling of all
  // SaveFile requests - communicating the failure to OnResponseCompleted
  // happens in a generic, typical way, reusing common infrastructure code
  // (rather than forcing an ad-hoc, Save-File-specific call to
  // OnResponseCompleted from ResourceDispatcherHostImpl::BeginSaveFile).
  enum class AuthorizationState {
    AUTHORIZED,
    NOT_AUTHORIZED,
  };

  SaveFileResourceHandler(net::URLRequest* request,
                          SaveItemId save_item_id,
                          SavePackageId save_package_id,
                          int render_process_host_id,
                          int render_frame_routing_id,
                          const GURL& url,
                          AuthorizationState authorization_state);
  ~SaveFileResourceHandler() override;

  // ResourceHandler Implementation:

  // Saves the redirected URL to final_url_, we need to use the original
  // URL to match original request.
  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      ResourceResponse* response,
      std::unique_ptr<ResourceController> controller) override;

  // Sends the download creation information to the download thread.
  void OnResponseStarted(
      ResourceResponse* response,
      std::unique_ptr<ResourceController> controller) override;

  // Pass-through implementation.
  void OnWillStart(const GURL& url,
                   std::unique_ptr<ResourceController> controller) override;

  // Creates a new buffer, which will be handed to the download thread for file
  // writing and deletion.
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size) override;

  // Passes the buffer to the download file writer.
  void OnReadCompleted(int bytes_read,
                       std::unique_ptr<ResourceController> controller) override;

  void OnResponseCompleted(
      const net::URLRequestStatus& status,
      std::unique_ptr<ResourceController> controller) override;

  // N/A to this flavor of SaveFileResourceHandler.
  void OnDataDownloaded(int bytes_downloaded) override;

  // If the content-length header is not present (or contains something other
  // than numbers), StringToInt64 returns 0, which indicates 'unknown size' and
  // is handled correctly by the SaveManager.
  void set_content_length(const std::string& content_length);

  void set_content_disposition(const std::string& content_disposition) {
    content_disposition_ = content_disposition;
  }

 private:
  SaveItemId save_item_id_;
  SavePackageId save_package_id_;
  int render_process_id_;
  int render_frame_routing_id_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  std::string content_disposition_;
  GURL url_;
  GURL final_url_;
  int64_t content_length_;
  SaveFileManager* save_manager_;

  AuthorizationState authorization_state_;

  static const int kReadBufSize = 32768;  // bytes

  DISALLOW_COPY_AND_ASSIGN(SaveFileResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_RESOURCE_HANDLER_H_
