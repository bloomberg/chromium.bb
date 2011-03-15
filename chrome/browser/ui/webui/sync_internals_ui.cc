// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals_ui.h"

#include <string>

#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "base/tracked_objects.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/js_arg_list.h"
#include "chrome/browser/sync/js_frontend.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/sync_internals_html_source.h"
#include "chrome/common/render_messages_params.h"
#include "content/browser/browser_thread.h"

SyncInternalsUI::SyncInternalsUI(TabContents* contents)
    : WebUI(contents) {
  browser_sync::JsFrontend* backend = GetJsFrontend();
  if (backend) {
    backend->AddHandler(this);
  }
  // If this PostTask() call fails, it's most likely because this is
  // being run from a unit test.  The created objects will be cleaned
  // up, anyway.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(
      new SyncInternalsHTMLSource());
}

SyncInternalsUI::~SyncInternalsUI() {
  browser_sync::JsFrontend* backend = GetJsFrontend();
  if (backend) {
    backend->RemoveHandler(this);
  }
}

void SyncInternalsUI::ProcessWebUIMessage(
    const ViewHostMsg_DomMessage_Params& params) {
  const std::string& name = params.name;
  browser_sync::JsArgList args(params.arguments);
  VLOG(1) << "Received message: " << name << " with args "
          << args.ToString();
  // We handle this case directly because it needs to work even if
  // the sync service doesn't exist.
  if (name == "getAboutInfo") {
    ListValue args;
    DictionaryValue* about_info = new DictionaryValue();
    args.Append(about_info);
    ProfileSyncService* service = GetProfile()->GetProfileSyncService();
    sync_ui_util::ConstructAboutInformation(service, about_info);
    HandleJsEvent("onGetAboutInfoFinished",
                  browser_sync::JsArgList(args));
  } else {
    browser_sync::JsFrontend* backend = GetJsFrontend();
    if (backend) {
      backend->ProcessMessage(name, args, this);
    } else {
      LOG(WARNING) << "No sync service; dropping message " << name
                   << " with args " << args.ToString();
    }
  }
}

void SyncInternalsUI::HandleJsEvent(const std::string& name,
                                    const browser_sync::JsArgList& args) {
  VLOG(1) << "Handling event: " << name << " with args " << args.ToString();
  std::vector<const Value*> arg_list(args.Get().begin(), args.Get().end());
  CallJavascriptFunction(name, arg_list);
}

browser_sync::JsFrontend* SyncInternalsUI::GetJsFrontend() {
  // If this returns NULL that means that sync is disabled for
  // whatever reason.
  ProfileSyncService* sync_service = GetProfile()->GetProfileSyncService();
  return sync_service ? sync_service->GetJsFrontend() : NULL;
}
