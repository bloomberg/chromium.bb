// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_SYNC_INTERNALS_UI_H_
#define CHROME_BROWSER_WEBUI_SYNC_INTERNALS_UI_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/sync/js_event_handler.h"
#include "chrome/browser/webui/web_ui.h"

namespace browser_sync {
class JsFrontend;
}  // namespace browser_sync

// The implementation for the chrome://sync-internals page.
class SyncInternalsUI : public WebUI, public browser_sync::JsEventHandler {
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
  virtual void ProcessWebUIMessage(
      const ViewHostMsg_DomMessage_Params& params);

  // browser_sync::JsEventHandler implementation.
  virtual void HandleJsEvent(const std::string& name,
                             const browser_sync::JsArgList& args);

 private:
  // Returns the sync service's JsFrontend object, or NULL if the sync
  // service does not exist.
  browser_sync::JsFrontend* GetJsFrontend();

  DISALLOW_COPY_AND_ASSIGN(SyncInternalsUI);
};

#endif  // CHROME_BROWSER_WEBUI_SYNC_INTERNALS_UI_H_
