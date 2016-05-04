// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POPULAR_SITES_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_POPULAR_SITES_INTERNALS_MESSAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

class PopularSites;

// The implementation for the chrome://popular-sites-internals page.
class PopularSitesInternalsMessageHandler
    : public content::WebUIMessageHandler {
 public:
  PopularSitesInternalsMessageHandler();
  ~PopularSitesInternalsMessageHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  void HandleRegisterForEvents(const base::ListValue* args);
  void HandleUpdate(const base::ListValue* args);
  void HandleViewJson(const base::ListValue* args);

  void SendOverrides();
  void SendDownloadResult(bool success);
  void SendSites();
  void SendJson(const std::string& json);

  void OnPopularSitesAvailable(bool explicit_request, bool success);

  std::unique_ptr<PopularSites> popular_sites_;

  base::WeakPtrFactory<PopularSitesInternalsMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PopularSitesInternalsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_POPULAR_SITES_INTERNALS_MESSAGE_HANDLER_H_
