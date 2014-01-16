// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_MESSAGE_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "sync/js/js_controller.h"
#include "sync/js/js_event_handler.h"
#include "sync/js/js_reply_handler.h"

class ProfileSyncService;

// The implementation for the chrome://sync-internals page.
class SyncInternalsMessageHandler
    : public content::WebUIMessageHandler,
      public syncer::JsEventHandler,
      public syncer::JsReplyHandler {
 public:
  SyncInternalsMessageHandler();
  virtual ~SyncInternalsMessageHandler();

  virtual void RegisterMessages() OVERRIDE;

  void ForwardToJsController(const std::string& name, const base::ListValue*);
  void OnGetAboutInfo(const base::ListValue*);
  void OnGetListOfTypes(const base::ListValue*);

  // syncer::JsEventHandler implementation.
  virtual void HandleJsEvent(
      const std::string& name,
      const syncer::JsEventDetails& details) OVERRIDE;

  // syncer::JsReplyHandler implementation.
  virtual void HandleJsReply(
      const std::string& name,
      const syncer::JsArgList& args) OVERRIDE;

 private:
  void RegisterJsControllerCallback(const std::string& name);
  ProfileSyncService* GetProfileSyncService();

  base::WeakPtr<syncer::JsController> js_controller_;
  base::WeakPtrFactory<SyncInternalsMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncInternalsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_MESSAGE_HANDLER_H_
