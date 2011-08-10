// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_UI_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/js/js_event_handler.h"
#include "chrome/browser/sync/js/js_reply_handler.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"

class ProfileSyncService;

namespace browser_sync {
class JsController;
}  // namespace browser_sync

// The implementation for the chrome://sync-internals page.
class SyncInternalsUI : public ChromeWebUI,
                        public browser_sync::JsEventHandler,
                        public browser_sync::JsReplyHandler {
 public:
  explicit SyncInternalsUI(TabContents* contents);
  virtual ~SyncInternalsUI();

  // WebUI implementation.
  //
  // The following messages are processed:
  //
  // getAboutInfo():
  //   Immediately fires a onGetAboutInfoFinished() event with a
  //   dictionary of sync-related stats and info.
  //
  // All other messages are routed to the sync service if it exists,
  // and dropped otherwise.
  //
  // TODO(akalin): Add a simple isSyncEnabled() message and make
  // getAboutInfo() be handled by the sync service.
  virtual void OnWebUISend(const GURL& source_url,
                           const std::string& name,
                           const base::ListValue& args) OVERRIDE;

  // browser_sync::JsEventHandler implementation.
  virtual void HandleJsEvent(
      const std::string& name,
      const browser_sync::JsEventDetails& details) OVERRIDE;

  // browser_sync::JsReplyHandler implementation.
  virtual void HandleJsReply(
      const std::string& name,
      const browser_sync::JsArgList& args) OVERRIDE;

 private:
  base::WeakPtrFactory<SyncInternalsUI> weak_ptr_factory_;
  base::WeakPtr<browser_sync::JsController> js_controller_;

  DISALLOW_COPY_AND_ASSIGN(SyncInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_UI_H_
