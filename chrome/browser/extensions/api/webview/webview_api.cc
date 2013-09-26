// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webview/webview_api.h"

#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/guestview/webview/webview_guest.h"
#include "chrome/common/extensions/api/webview.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/error_utils.h"

using extensions::api::tabs::InjectDetails;
namespace webview = extensions::api::webview;

namespace extensions {

namespace {
int MaskForKey(const char* key) {
  if (strcmp(key, extension_browsing_data_api_constants::kAppCacheKey) == 0)
    return content::StoragePartition::REMOVE_DATA_MASK_APPCACHE;
  if (strcmp(key, extension_browsing_data_api_constants::kCookiesKey) == 0)
    return content::StoragePartition::REMOVE_DATA_MASK_COOKIES;
  if (strcmp(key, extension_browsing_data_api_constants::kFileSystemsKey) == 0)
    return content::StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS;
  if (strcmp(key, extension_browsing_data_api_constants::kIndexedDBKey) == 0)
    return content::StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  if (strcmp(key, extension_browsing_data_api_constants::kLocalStorageKey) == 0)
    return content::StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  if (strcmp(key, extension_browsing_data_api_constants::kWebSQLKey) == 0)
    return content::StoragePartition::REMOVE_DATA_MASK_WEBSQL;
  return 0;
}

}  // namespace

WebviewClearDataFunction::WebviewClearDataFunction()
    : remove_mask_(0),
      bad_message_(false) {
};

WebviewClearDataFunction::~WebviewClearDataFunction() {
};

// Parses the |dataToRemove| argument to generate the remove mask. Sets
// |bad_message_| (like EXTENSION_FUNCTION_VALIDATE would if this were a bool
// method) if 'dataToRemove' is not present.
uint32 WebviewClearDataFunction::GetRemovalMask() {
  base::DictionaryValue* data_to_remove;
  if (!args_->GetDictionary(2, &data_to_remove)) {
    bad_message_ = true;
    return 0;
  }

  uint32 remove_mask = 0;
  for (base::DictionaryValue::Iterator i(*data_to_remove);
       !i.IsAtEnd();
       i.Advance()) {
    bool selected = false;
    if (!i.value().GetAsBoolean(&selected)) {
      bad_message_ = true;
      return 0;
    }
    if (selected)
      remove_mask |= MaskForKey(i.key().c_str());
  }

  return remove_mask;
}

// TODO(lazyboy): Parameters in this extension function are similar (or a
// sub-set) to BrowsingDataRemoverFunction. How can we share this code?
bool WebviewClearDataFunction::RunImpl() {
  content::RecordAction(content::UserMetricsAction("WebView.ClearData"));
  int instance_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &instance_id));

  // Grab the initial |options| parameter, and parse out the arguments.
  base::DictionaryValue* options;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &options));
  DCHECK(options);

  // If |ms_since_epoch| isn't set, default it to 0.
  double ms_since_epoch;
  if (!options->GetDouble(extension_browsing_data_api_constants::kSinceKey,
                          &ms_since_epoch)) {
    ms_since_epoch = 0;
  }

  // base::Time takes a double that represents seconds since epoch. JavaScript
  // gives developers milliseconds, so do a quick conversion before populating
  // the object. Also, Time::FromDoubleT converts double time 0 to empty Time
  // object. So we need to do special handling here.
  remove_since_ = (ms_since_epoch == 0) ?
      base::Time::UnixEpoch() :
      base::Time::FromDoubleT(ms_since_epoch / 1000.0);

  remove_mask_ = GetRemovalMask();
  if (bad_message_)
    return false;

  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), instance_id);
  if (!guest)
    return false;

  AddRef();  // Balanced below or in WebviewClearDataFunction::Done().

  bool scheduled = false;
  if (remove_mask_) {
    scheduled = guest->ClearData(
        remove_since_,
        remove_mask_,
        base::Bind(&WebviewClearDataFunction::ClearDataDone,
                   this));
  }
  if (!remove_mask_ || !scheduled) {
    SendResponse(false);
    Release();  // Balanced above.
    return false;
  }

  // Will finish asynchronously.
  return true;
}

void WebviewClearDataFunction::ClearDataDone() {
  Release();  // Balanced in RunImpl().
  SendResponse(true);
}

WebviewExecuteCodeFunction::WebviewExecuteCodeFunction()
    : guest_instance_id_(0) {
}

WebviewExecuteCodeFunction::~WebviewExecuteCodeFunction() {
}

bool WebviewExecuteCodeFunction::Init() {
  if (details_.get())
    return true;

  if (!args_->GetInteger(0, &guest_instance_id_))
    return false;

  if (!guest_instance_id_)
    return false;

  base::DictionaryValue* details_value = NULL;
  if (!args_->GetDictionary(1, &details_value))
    return false;
  scoped_ptr<InjectDetails> details(new InjectDetails());
  if (!InjectDetails::Populate(*details_value, details.get()))
    return false;

  details_ = details.Pass();
  return true;
}

bool WebviewExecuteCodeFunction::ShouldInsertCSS() const {
  return false;
}

bool WebviewExecuteCodeFunction::CanExecuteScriptOnPage() {
  return true;
}

extensions::ScriptExecutor* WebviewExecuteCodeFunction::GetScriptExecutor() {
  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), guest_instance_id_);
  if (!guest)
    return NULL;

  return guest->script_executor();
}

bool WebviewExecuteCodeFunction::IsWebView() const {
  return true;
}

WebviewExecuteScriptFunction::WebviewExecuteScriptFunction() {
}

void WebviewExecuteScriptFunction::OnExecuteCodeFinished(
    const std::string& error,
    int32 on_page_id,
    const GURL& on_url,
    const base::ListValue& result) {
  content::RecordAction(content::UserMetricsAction("WebView.ExecuteScript"));
  if (error.empty())
    SetResult(result.DeepCopy());
  WebviewExecuteCodeFunction::OnExecuteCodeFinished(error, on_page_id, on_url,
                                                    result);
}

WebviewInsertCSSFunction::WebviewInsertCSSFunction() {
}

bool WebviewInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

WebviewGoFunction::WebviewGoFunction() {
}

WebviewGoFunction::~WebviewGoFunction() {
}

bool WebviewGoFunction::RunImpl() {
  content::RecordAction(content::UserMetricsAction("WebView.Go"));
  scoped_ptr<webview::Go::Params> params(webview::Go::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), params->instance_id);
  if (!guest)
    return false;

  guest->Go(params->relative_index);
  return true;
}

WebviewReloadFunction::WebviewReloadFunction() {
}

WebviewReloadFunction::~WebviewReloadFunction() {
}

bool WebviewReloadFunction::RunImpl() {
  content::RecordAction(content::UserMetricsAction("WebView.Reload"));
  scoped_ptr<webview::Reload::Params> params(
      webview::Reload::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), params->instance_id);
  if (!guest)
    return false;

  guest->Reload();
  return true;
}

WebviewSetPermissionFunction::WebviewSetPermissionFunction() {
}

WebviewSetPermissionFunction::~WebviewSetPermissionFunction() {
}

bool WebviewSetPermissionFunction::RunImpl() {
  scoped_ptr<webview::SetPermission::Params> params(
      webview::SetPermission::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), params->instance_id);
  if (!guest)
    return false;

  EXTENSION_FUNCTION_VALIDATE(
      guest->SetPermission(params->request_id,
                           params->should_allow,
                           params->user_input));
  return true;
}

WebviewOverrideUserAgentFunction::WebviewOverrideUserAgentFunction() {
}

WebviewOverrideUserAgentFunction::~WebviewOverrideUserAgentFunction() {
}

bool WebviewOverrideUserAgentFunction::RunImpl() {
  scoped_ptr<extensions::api::webview::OverrideUserAgent::Params> params(
      extensions::api::webview::OverrideUserAgent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  int instance_id = params->instance_id;
  std::string user_agent_override = params->user_agent_override;

  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), instance_id);
  if (!guest)
    return false;

  guest->SetUserAgentOverride(user_agent_override);
  return true;
}

WebviewStopFunction::WebviewStopFunction() {
}

WebviewStopFunction::~WebviewStopFunction() {
}

bool WebviewStopFunction::RunImpl() {
  content::RecordAction(content::UserMetricsAction("WebView.Stop"));
  scoped_ptr<webview::Stop::Params> params(
      webview::Stop::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), params->instance_id);
  if (!guest)
    return false;

  guest->Stop();
  return true;
}

WebviewTerminateFunction::WebviewTerminateFunction() {
}

WebviewTerminateFunction::~WebviewTerminateFunction() {
}

bool WebviewTerminateFunction::RunImpl() {
  content::RecordAction(content::UserMetricsAction("WebView.Terminate"));
  scoped_ptr<webview::Terminate::Params> params(
      webview::Terminate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), params->instance_id);
  if (!guest)
    return false;

  guest->Terminate();
  return true;
}

}  // namespace extensions
