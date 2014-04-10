// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webview/webview_api.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"
#include "chrome/browser/extensions/api/context_menus/context_menus_api.h"
#include "chrome/browser/extensions/api/context_menus/context_menus_api_helpers.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/webview.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/stop_find_action.h"
#include "extensions/common/error_utils.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"

using content::WebContents;
using extensions::api::tabs::InjectDetails;
using extensions::api::webview::SetPermission::Params;
namespace helpers = extensions::context_menus_api_helpers;
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

bool WebviewExtensionFunction::RunImpl() {
  int instance_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &instance_id));
  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), instance_id);
  if (!guest)
    return false;

  return RunImplSafe(guest);
}

// TODO(lazyboy): Add checks similar to
// WebviewExtensionFunction::RunImplSafe(WebViewGuest*).
bool WebviewContextMenusCreateFunction::RunImpl() {
  scoped_ptr<webview::ContextMenusCreate::Params> params(
      webview::ContextMenusCreate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MenuItem::Id id(
      Profile::FromBrowserContext(browser_context())->IsOffTheRecord(),
      MenuItem::ExtensionKey(extension_id(), params->instance_id));

  if (params->create_properties.id.get()) {
    id.string_uid = *params->create_properties.id;
  } else {
    // The Generated Id is added by webview_custom_bindings.js.
    base::DictionaryValue* properties = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &properties));
    EXTENSION_FUNCTION_VALIDATE(
        properties->GetInteger(helpers::kGeneratedIdKey, &id.uid));
  }

  bool success = extensions::context_menus_api_helpers::CreateMenuItem(
      params->create_properties,
      Profile::FromBrowserContext(browser_context()),
      GetExtension(),
      id,
      &error_);

  SendResponse(success);
  return success;
}

bool WebviewContextMenusUpdateFunction::RunImpl() {
  scoped_ptr<webview::ContextMenusUpdate::Params> params(
      webview::ContextMenusUpdate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  MenuItem::Id item_id(
      profile->IsOffTheRecord(),
      MenuItem::ExtensionKey(extension_id(), params->instance_id));

  if (params->id.as_string)
    item_id.string_uid = *params->id.as_string;
  else if (params->id.as_integer)
    item_id.uid = *params->id.as_integer;
  else
    NOTREACHED();

  bool success = extensions::context_menus_api_helpers::UpdateMenuItem(
      params->update_properties, profile, GetExtension(), item_id, &error_);
  SendResponse(success);
  return success;
}

bool WebviewContextMenusRemoveFunction::RunImpl() {
  scoped_ptr<webview::ContextMenusRemove::Params> params(
      webview::ContextMenusRemove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MenuManager* menu_manager =
      MenuManager::Get(Profile::FromBrowserContext(browser_context()));

  MenuItem::Id id(
      Profile::FromBrowserContext(browser_context())->IsOffTheRecord(),
      MenuItem::ExtensionKey(extension_id(), params->instance_id));

  if (params->menu_item_id.as_string) {
    id.string_uid = *params->menu_item_id.as_string;
  } else if (params->menu_item_id.as_integer) {
    id.uid = *params->menu_item_id.as_integer;
  } else {
    NOTREACHED();
  }

  bool success = true;
  MenuItem* item = menu_manager->GetItemById(id);
  // Ensure one <webview> can't remove another's menu items.
  if (!item || item->id().extension_key != id.extension_key) {
    error_ = ErrorUtils::FormatErrorMessage(
        context_menus_api_helpers::kCannotFindItemError,
        context_menus_api_helpers::GetIDString(id));
    success = false;
  } else if (!menu_manager->RemoveContextMenuItem(id)) {
    success = false;
  }

  SendResponse(success);
  return success;
}

bool WebviewContextMenusRemoveAllFunction::RunImpl() {
  scoped_ptr<webview::ContextMenusRemoveAll::Params> params(
      webview::ContextMenusRemoveAll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MenuManager* menu_manager =
      MenuManager::Get(Profile::FromBrowserContext(browser_context()));

  int webview_instance_id = params->instance_id;
  menu_manager->RemoveAllContextItems(
      MenuItem::ExtensionKey(GetExtension()->id(), webview_instance_id));
  SendResponse(true);
  return true;
}

WebviewClearDataFunction::WebviewClearDataFunction()
    : remove_mask_(0), bad_message_(false) {}

WebviewClearDataFunction::~WebviewClearDataFunction() {}

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
bool WebviewClearDataFunction::RunImplSafe(WebViewGuest* guest) {
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
    : guest_instance_id_(0), guest_src_(GURL::EmptyGURL()) {}

WebviewExecuteCodeFunction::~WebviewExecuteCodeFunction() {
}

bool WebviewExecuteCodeFunction::Init() {
  if (details_.get())
    return true;

  if (!args_->GetInteger(0, &guest_instance_id_))
    return false;

  if (!guest_instance_id_)
    return false;

  std::string src;
  if (!args_->GetString(1, &src))
    return false;

  guest_src_ = GURL(src);
  if (!guest_src_.is_valid())
    return false;

  base::DictionaryValue* details_value = NULL;
  if (!args_->GetDictionary(2, &details_value))
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

const GURL& WebviewExecuteCodeFunction::GetWebViewSrc() const {
  return guest_src_;
}

WebviewExecuteScriptFunction::WebviewExecuteScriptFunction() {
}

void WebviewExecuteScriptFunction::OnExecuteCodeFinished(
    const std::string& error,
    int32 on_page_id,
    const GURL& on_url,
    const base::ListValue& result) {
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

WebviewCaptureVisibleRegionFunction::WebviewCaptureVisibleRegionFunction() {
}

WebviewCaptureVisibleRegionFunction::~WebviewCaptureVisibleRegionFunction() {
}

bool WebviewCaptureVisibleRegionFunction::IsScreenshotEnabled() {
  return true;
}

WebContents* WebviewCaptureVisibleRegionFunction::GetWebContentsForID(
    int instance_id) {
  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), instance_id);
  return guest ? guest->guest_web_contents() : NULL;
}

void WebviewCaptureVisibleRegionFunction::OnCaptureFailure(
    FailureReason reason) {
  SendResponse(false);
}

WebviewSetZoomFunction::WebviewSetZoomFunction() {
}

WebviewSetZoomFunction::~WebviewSetZoomFunction() {
}

bool WebviewSetZoomFunction::RunImplSafe(WebViewGuest* guest) {
  scoped_ptr<webview::SetZoom::Params> params(
      webview::SetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest->SetZoom(params->zoom_factor);

  SendResponse(true);
  return true;
}

WebviewGetZoomFunction::WebviewGetZoomFunction() {
}

WebviewGetZoomFunction::~WebviewGetZoomFunction() {
}

bool WebviewGetZoomFunction::RunImplSafe(WebViewGuest* guest) {
  scoped_ptr<webview::GetZoom::Params> params(
      webview::GetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  double zoom_factor = guest->GetZoom();
  SetResult(base::Value::CreateDoubleValue(zoom_factor));
  SendResponse(true);
  return true;
}

WebviewFindFunction::WebviewFindFunction() {
}

WebviewFindFunction::~WebviewFindFunction() {
}

bool WebviewFindFunction::RunImplSafe(WebViewGuest* guest) {
  scoped_ptr<webview::Find::Params> params(
      webview::Find::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Convert the std::string search_text to string16.
  base::string16 search_text;
  base::UTF8ToUTF16(params->search_text.c_str(),
                    params->search_text.length(),
                    &search_text);

  // Set the find options to their default values.
  blink::WebFindOptions options;
  if (params->options) {
    options.forward =
        params->options->backward ? !*params->options->backward : true;
    options.matchCase =
        params->options->match_case ? *params->options->match_case : false;
  }

  guest->Find(search_text, options, this);
  return true;
}

WebviewStopFindingFunction::WebviewStopFindingFunction() {
}

WebviewStopFindingFunction::~WebviewStopFindingFunction() {
}

bool WebviewStopFindingFunction::RunImplSafe(WebViewGuest* guest) {
  scoped_ptr<webview::StopFinding::Params> params(
      webview::StopFinding::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Set the StopFindAction.
  content::StopFindAction action;
  switch (params->action) {
    case webview::StopFinding::Params::ACTION_CLEAR:
      action = content::STOP_FIND_ACTION_CLEAR_SELECTION;
      break;
    case webview::StopFinding::Params::ACTION_KEEP:
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
      break;
    case webview::StopFinding::Params::ACTION_ACTIVATE:
      action = content::STOP_FIND_ACTION_ACTIVATE_SELECTION;
      break;
    default:
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
  }

  guest->StopFinding(action);
  return true;
}

WebviewGoFunction::WebviewGoFunction() {
}

WebviewGoFunction::~WebviewGoFunction() {
}

bool WebviewGoFunction::RunImplSafe(WebViewGuest* guest) {
  scoped_ptr<webview::Go::Params> params(webview::Go::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  guest->Go(params->relative_index);
  return true;
}

WebviewReloadFunction::WebviewReloadFunction() {
}

WebviewReloadFunction::~WebviewReloadFunction() {
}

bool WebviewReloadFunction::RunImplSafe(WebViewGuest* guest) {
  guest->Reload();
  return true;
}

WebviewSetPermissionFunction::WebviewSetPermissionFunction() {
}

WebviewSetPermissionFunction::~WebviewSetPermissionFunction() {
}

bool WebviewSetPermissionFunction::RunImplSafe(WebViewGuest* guest) {
  scoped_ptr<webview::SetPermission::Params> params(
      webview::SetPermission::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewGuest::PermissionResponseAction action = WebViewGuest::DEFAULT;
  switch (params->action) {
    case Params::ACTION_ALLOW:
      action = WebViewGuest::ALLOW;
      break;
    case Params::ACTION_DENY:
      action = WebViewGuest::DENY;
      break;
    case Params::ACTION_DEFAULT:
      break;
    default:
      NOTREACHED();
  }

  std::string user_input;
  if (params->user_input)
    user_input = *params->user_input;

  WebViewGuest::SetPermissionResult result =
      guest->SetPermission(params->request_id, action, user_input);

  EXTENSION_FUNCTION_VALIDATE(result != WebViewGuest::SET_PERMISSION_INVALID);

  SetResult(base::Value::CreateBooleanValue(
      result == WebViewGuest::SET_PERMISSION_ALLOWED));
  SendResponse(true);
  return true;
}

WebviewOverrideUserAgentFunction::WebviewOverrideUserAgentFunction() {
}

WebviewOverrideUserAgentFunction::~WebviewOverrideUserAgentFunction() {
}

bool WebviewOverrideUserAgentFunction::RunImplSafe(WebViewGuest* guest) {
  scoped_ptr<extensions::api::webview::OverrideUserAgent::Params> params(
      extensions::api::webview::OverrideUserAgent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  guest->SetUserAgentOverride(params->user_agent_override);
  return true;
}

WebviewStopFunction::WebviewStopFunction() {
}

WebviewStopFunction::~WebviewStopFunction() {
}

bool WebviewStopFunction::RunImplSafe(WebViewGuest* guest) {
  guest->Stop();
  return true;
}

WebviewTerminateFunction::WebviewTerminateFunction() {
}

WebviewTerminateFunction::~WebviewTerminateFunction() {
}

bool WebviewTerminateFunction::RunImplSafe(WebViewGuest* guest) {
  guest->Terminate();
  return true;
}

}  // namespace extensions
