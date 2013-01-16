// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals_ui.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/tracked_objects.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/sync_internals_resources.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/js/js_arg_list.h"
#include "sync/js/js_controller.h"
#include "sync/js/js_event_details.h"
#include "ui/base/resource/resource_bundle.h"

using syncer::JsArgList;
using syncer::JsEventDetails;
using syncer::JsReplyHandler;
using syncer::WeakHandle;
using content::WebContents;

namespace {

ChromeWebUIDataSource* CreateSyncInternalsHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUISyncInternalsHost);

  source->set_json_path("strings.js");
  source->add_resource_path("sync_index.js", IDR_SYNC_INTERNALS_INDEX_JS);
  source->add_resource_path("chrome_sync.js",
                            IDR_SYNC_INTERNALS_CHROME_SYNC_JS);
  source->add_resource_path("sync_log.js", IDR_SYNC_INTERNALS_SYNC_LOG_JS);
  source->add_resource_path("sync_node_browser.js",
                            IDR_SYNC_INTERNALS_SYNC_NODE_BROWSER_JS);
  source->add_resource_path("sync_search.js",
                            IDR_SYNC_INTERNALS_SYNC_SEARCH_JS);
  source->add_resource_path("about.js", IDR_SYNC_INTERNALS_ABOUT_JS);
  source->add_resource_path("data.js", IDR_SYNC_INTERNALS_DATA_JS);
  source->add_resource_path("events.js", IDR_SYNC_INTERNALS_EVENTS_JS);
  source->add_resource_path("notifications.js",
                            IDR_SYNC_INTERNALS_NOTIFICATIONS_JS);
  source->add_resource_path("search.js", IDR_SYNC_INTERNALS_SEARCH_JS);
  source->add_resource_path("node_browser.js",
                            IDR_SYNC_INTERNALS_NODE_BROWSER_JS);
  source->add_resource_path("traffic.js",
                            IDR_SYNC_INTERNALS_TRAFFIC_JS);
  source->set_default_resource(IDR_SYNC_INTERNALS_INDEX_HTML);
  return source;
}

}  // namespace

namespace {

// Gets the ProfileSyncService of the underlying original profile.
// May return NULL (e.g., if sync is disabled on the command line).
ProfileSyncService* GetProfileSyncService(Profile* profile) {
  return ProfileSyncServiceFactory::GetInstance()->GetForProfile(
      profile->GetOriginalProfile());
}

}  // namespace

SyncInternalsUI::SyncInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  // TODO(akalin): Fix.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSourceImpl(profile,
                                          CreateSyncInternalsHTMLSource());
  ProfileSyncService* sync_service = GetProfileSyncService(profile);
  if (sync_service) {
    js_controller_ = sync_service->GetJsController();
  }
  if (js_controller_.get()) {
    js_controller_->AddJsEventHandler(this);
  }
}

SyncInternalsUI::~SyncInternalsUI() {
  if (js_controller_.get()) {
    js_controller_->RemoveJsEventHandler(this);
  }
}

bool SyncInternalsUI::OverrideHandleWebUIMessage(const GURL& source_url,
                                                 const std::string& name,
                                                 const ListValue& content) {
  scoped_ptr<ListValue> content_copy(content.DeepCopy());
  JsArgList args(content_copy.get());
  VLOG(1) << "Received message: " << name << " with args "
          << args.ToString();
  // We handle this case directly because it needs to work even if
  // the sync service doesn't exist.
  if (name == "getAboutInfo") {
    ListValue return_args;
    Profile* profile = Profile::FromWebUI(web_ui());
    ProfileSyncService* service = GetProfileSyncService(profile);
    return_args.Append(
        sync_ui_util::ConstructAboutInformation(service).release());
    HandleJsReply(name, JsArgList(&return_args));
  } else {
    if (js_controller_.get()) {
      js_controller_->ProcessJsMessage(
          name, args,
          MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()));
    } else {
      LOG(WARNING) << "No sync service; dropping message " << name
                   << " with args " << args.ToString();
    }
  }
  return true;
}

void SyncInternalsUI::HandleJsEvent(
    const std::string& name, const JsEventDetails& details) {
  VLOG(1) << "Handling event: " << name << " with details "
          << details.ToString();
  const std::string& event_handler = "chrome.sync." + name + ".fire";
  std::vector<const Value*> arg_list(1, &details.Get());
  web_ui()->CallJavascriptFunction(event_handler, arg_list);
}

void SyncInternalsUI::HandleJsReply(
    const std::string& name, const JsArgList& args) {
  VLOG(1) << "Handling reply for " << name << " message with args "
          << args.ToString();
  const std::string& reply_handler = "chrome.sync." + name + ".handleReply";
  std::vector<const Value*> arg_list(args.Get().begin(), args.Get().end());
  web_ui()->CallJavascriptFunction(reply_handler, arg_list);
}
