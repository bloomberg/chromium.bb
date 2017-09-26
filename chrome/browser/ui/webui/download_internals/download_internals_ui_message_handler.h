// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOAD_INTERNALS_DOWNLOAD_INTERNALS_UI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOAD_INTERNALS_DOWNLOAD_INTERNALS_UI_MESSAGE_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/logger.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace download {
class DownloadService;
}

namespace download_internals {

// Class acting as a controller of the chrome://download-internals WebUI.
class DownloadInternalsUIMessageHandler : public content::WebUIMessageHandler,
                                          public download::Logger::Observer {
 public:
  DownloadInternalsUIMessageHandler();
  ~DownloadInternalsUIMessageHandler() override;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // download::Logger::Observer implementation.
  void OnServiceStatusChanged(const base::Value& service_status) override;

 private:
  // Get the current DownloadService and sub component statuses.
  void HandleGetServiceStatus(const base::ListValue* args);

  download::DownloadService* download_service_;

  base::WeakPtrFactory<DownloadInternalsUIMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadInternalsUIMessageHandler);
};

}  // namespace download_internals

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOAD_INTERNALS_DOWNLOAD_INTERNALS_UI_MESSAGE_HANDLER_H_
