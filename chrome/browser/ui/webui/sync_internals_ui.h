// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_UI_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_controller.h"
#include "sync/js/js_event_handler.h"
#include "sync/js/js_reply_handler.h"

namespace syncer {
class JsController;
}  // namespace syncer

// The implementation for the chrome://sync-internals page.
class SyncInternalsUI : public content::WebUIController,
                        public syncer::JsEventHandler,
                        public syncer::JsReplyHandler {
 public:
  explicit SyncInternalsUI(content::WebUI* web_ui);
  virtual ~SyncInternalsUI();

  // WebUIController implementation.
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
  virtual bool OverrideHandleWebUIMessage(const GURL& source_url,
                                          const std::string& name,
                                          const base::ListValue& args) OVERRIDE;

  // syncer::JsEventHandler implementation.
  virtual void HandleJsEvent(
      const std::string& name,
      const syncer::JsEventDetails& details) OVERRIDE;

  // syncer::JsReplyHandler implementation.
  virtual void HandleJsReply(
      const std::string& name,
      const syncer::JsArgList& args) OVERRIDE;

 private:
  base::WeakPtr<syncer::JsController> js_controller_;
  base::WeakPtrFactory<SyncInternalsUI> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_UI_H_
