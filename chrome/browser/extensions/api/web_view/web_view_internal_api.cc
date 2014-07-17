// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_view/web_view_internal_api.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"
#include "chrome/browser/extensions/api/context_menus/context_menus_api.h"
#include "chrome/browser/extensions/api/context_menus/context_menus_api_helpers.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/guest_view/web_view/web_view_permission_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/web_view_internal.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/stop_find_action.h"
#include "extensions/common/error_utils.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"

using content::WebContents;
using extensions::api::tabs::InjectDetails;
using extensions::api::web_view_internal::SetPermission::Params;
namespace helpers = extensions::context_menus_api_helpers;
namespace webview = extensions::api::web_view_internal;

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

bool WebViewInternalExtensionFunction::RunAsync() {
  int instance_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &instance_id));
  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), instance_id);
  if (!guest)
    return false;

  return RunAsyncSafe(guest);
}

// TODO(lazyboy): Add checks similar to
// WebViewInternalExtensionFunction::RunAsyncSafe(WebViewGuest*).
bool WebViewInternalContextMenusCreateFunction::RunAsync() {
  scoped_ptr<webview::ContextMenusCreate::Params> params(
      webview::ContextMenusCreate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MenuItem::Id id(
      Profile::FromBrowserContext(browser_context())->IsOffTheRecord(),
      MenuItem::ExtensionKey(extension_id(), params->instance_id));

  if (params->create_properties.id.get()) {
    id.string_uid = *params->create_properties.id;
  } else {
    // The Generated Id is added by web_view_internal_custom_bindings.js.
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

bool WebViewInternalNavigateFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::Navigate::Params> params(
      webview::Navigate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  std::string src = params->src;
  guest->NavigateGuest(src);
  return true;
}

bool WebViewInternalContextMenusUpdateFunction::RunAsync() {
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

bool WebViewInternalContextMenusRemoveFunction::RunAsync() {
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

bool WebViewInternalContextMenusRemoveAllFunction::RunAsync() {
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

WebViewInternalClearDataFunction::WebViewInternalClearDataFunction()
    : remove_mask_(0), bad_message_(false) {
}

WebViewInternalClearDataFunction::~WebViewInternalClearDataFunction() {
}

// Parses the |dataToRemove| argument to generate the remove mask. Sets
// |bad_message_| (like EXTENSION_FUNCTION_VALIDATE would if this were a bool
// method) if 'dataToRemove' is not present.
uint32 WebViewInternalClearDataFunction::GetRemovalMask() {
  base::DictionaryValue* data_to_remove;
  if (!args_->GetDictionary(2, &data_to_remove)) {
    bad_message_ = true;
    return 0;
  }

  uint32 remove_mask = 0;
  for (base::DictionaryValue::Iterator i(*data_to_remove); !i.IsAtEnd();
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
bool WebViewInternalClearDataFunction::RunAsyncSafe(WebViewGuest* guest) {
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
  remove_since_ = (ms_since_epoch == 0)
                      ? base::Time::UnixEpoch()
                      : base::Time::FromDoubleT(ms_since_epoch / 1000.0);

  remove_mask_ = GetRemovalMask();
  if (bad_message_)
    return false;

  AddRef();  // Balanced below or in WebViewInternalClearDataFunction::Done().

  bool scheduled = false;
  if (remove_mask_) {
    scheduled = guest->ClearData(
        remove_since_,
        remove_mask_,
        base::Bind(&WebViewInternalClearDataFunction::ClearDataDone, this));
  }
  if (!remove_mask_ || !scheduled) {
    SendResponse(false);
    Release();  // Balanced above.
    return false;
  }

  // Will finish asynchronously.
  return true;
}

void WebViewInternalClearDataFunction::ClearDataDone() {
  Release();  // Balanced in RunAsync().
  SendResponse(true);
}

WebViewInternalExecuteCodeFunction::WebViewInternalExecuteCodeFunction()
    : guest_instance_id_(0), guest_src_(GURL::EmptyGURL()) {
}

WebViewInternalExecuteCodeFunction::~WebViewInternalExecuteCodeFunction() {
}

bool WebViewInternalExecuteCodeFunction::Init() {
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

bool WebViewInternalExecuteCodeFunction::ShouldInsertCSS() const {
  return false;
}

bool WebViewInternalExecuteCodeFunction::CanExecuteScriptOnPage() {
  return true;
}

extensions::ScriptExecutor*
WebViewInternalExecuteCodeFunction::GetScriptExecutor() {
  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), guest_instance_id_);
  if (!guest)
    return NULL;

  return guest->script_executor();
}

bool WebViewInternalExecuteCodeFunction::IsWebView() const {
  return true;
}

const GURL& WebViewInternalExecuteCodeFunction::GetWebViewSrc() const {
  return guest_src_;
}

WebViewInternalExecuteScriptFunction::WebViewInternalExecuteScriptFunction() {
}

void WebViewInternalExecuteScriptFunction::OnExecuteCodeFinished(
    const std::string& error,
    const GURL& on_url,
    const base::ListValue& result) {
  if (error.empty())
    SetResult(result.DeepCopy());
  WebViewInternalExecuteCodeFunction::OnExecuteCodeFinished(
      error, on_url, result);
}

WebViewInternalInsertCSSFunction::WebViewInternalInsertCSSFunction() {
}

bool WebViewInternalInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

WebViewInternalCaptureVisibleRegionFunction::
    WebViewInternalCaptureVisibleRegionFunction() {
}

WebViewInternalCaptureVisibleRegionFunction::
    ~WebViewInternalCaptureVisibleRegionFunction() {
}

bool WebViewInternalCaptureVisibleRegionFunction::IsScreenshotEnabled() {
  return true;
}

WebContents* WebViewInternalCaptureVisibleRegionFunction::GetWebContentsForID(
    int instance_id) {
  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), instance_id);
  return guest ? guest->guest_web_contents() : NULL;
}

void WebViewInternalCaptureVisibleRegionFunction::OnCaptureFailure(
    FailureReason reason) {
  SendResponse(false);
}

WebViewInternalSetNameFunction::WebViewInternalSetNameFunction() {
}

WebViewInternalSetNameFunction::~WebViewInternalSetNameFunction() {
}

WebViewInternalSetZoomFunction::WebViewInternalSetZoomFunction() {
}

WebViewInternalSetZoomFunction::~WebViewInternalSetZoomFunction() {
}

bool WebViewInternalSetNameFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::SetName::Params> params(
      webview::SetName::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest->SetName(params->frame_name);
  SendResponse(true);
  return true;
}

bool WebViewInternalSetZoomFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::SetZoom::Params> params(
      webview::SetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest->SetZoom(params->zoom_factor);

  SendResponse(true);
  return true;
}

WebViewInternalGetZoomFunction::WebViewInternalGetZoomFunction() {
}

WebViewInternalGetZoomFunction::~WebViewInternalGetZoomFunction() {
}

bool WebViewInternalGetZoomFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::GetZoom::Params> params(
      webview::GetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  double zoom_factor = guest->GetZoom();
  SetResult(new base::FundamentalValue(zoom_factor));
  SendResponse(true);
  return true;
}

WebViewInternalFindFunction::WebViewInternalFindFunction() {
}

WebViewInternalFindFunction::~WebViewInternalFindFunction() {
}

bool WebViewInternalFindFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::Find::Params> params(
      webview::Find::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Convert the std::string search_text to string16.
  base::string16 search_text;
  base::UTF8ToUTF16(
      params->search_text.c_str(), params->search_text.length(), &search_text);

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

WebViewInternalStopFindingFunction::WebViewInternalStopFindingFunction() {
}

WebViewInternalStopFindingFunction::~WebViewInternalStopFindingFunction() {
}

bool WebViewInternalStopFindingFunction::RunAsyncSafe(WebViewGuest* guest) {
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

WebViewInternalGoFunction::WebViewInternalGoFunction() {
}

WebViewInternalGoFunction::~WebViewInternalGoFunction() {
}

bool WebViewInternalGoFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::Go::Params> params(webview::Go::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  guest->Go(params->relative_index);
  return true;
}

WebViewInternalReloadFunction::WebViewInternalReloadFunction() {
}

WebViewInternalReloadFunction::~WebViewInternalReloadFunction() {
}

bool WebViewInternalReloadFunction::RunAsyncSafe(WebViewGuest* guest) {
  guest->Reload();
  return true;
}

WebViewInternalSetPermissionFunction::WebViewInternalSetPermissionFunction() {
}

WebViewInternalSetPermissionFunction::~WebViewInternalSetPermissionFunction() {
}

bool WebViewInternalSetPermissionFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::SetPermission::Params> params(
      webview::SetPermission::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewPermissionHelper::PermissionResponseAction action =
      WebViewPermissionHelper::DEFAULT;
  switch (params->action) {
    case Params::ACTION_ALLOW:
      action = WebViewPermissionHelper::ALLOW;
      break;
    case Params::ACTION_DENY:
      action = WebViewPermissionHelper::DENY;
      break;
    case Params::ACTION_DEFAULT:
      break;
    default:
      NOTREACHED();
  }

  std::string user_input;
  if (params->user_input)
    user_input = *params->user_input;

  WebViewPermissionHelper* web_view_permission_helper =
      WebViewPermissionHelper:: FromWebContents(guest->guest_web_contents());

  WebViewPermissionHelper::SetPermissionResult result =
      web_view_permission_helper->SetPermission(
          params->request_id, action, user_input);

  EXTENSION_FUNCTION_VALIDATE(
      result != WebViewPermissionHelper::SET_PERMISSION_INVALID);

  SetResult(new base::FundamentalValue(
      result == WebViewPermissionHelper::SET_PERMISSION_ALLOWED));
  SendResponse(true);
  return true;
}

WebViewInternalShowContextMenuFunction::
    WebViewInternalShowContextMenuFunction() {
}

WebViewInternalShowContextMenuFunction::
    ~WebViewInternalShowContextMenuFunction() {
}

bool WebViewInternalShowContextMenuFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::ShowContextMenu::Params> params(
      webview::ShowContextMenu::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(lazyboy): Actually implement filtering menu items, we pass NULL for
  // now.
  guest->ShowContextMenu(params->request_id, NULL);

  SendResponse(true);
  return true;
}

WebViewInternalOverrideUserAgentFunction::
    WebViewInternalOverrideUserAgentFunction() {
}

WebViewInternalOverrideUserAgentFunction::
    ~WebViewInternalOverrideUserAgentFunction() {
}

bool WebViewInternalOverrideUserAgentFunction::RunAsyncSafe(
    WebViewGuest* guest) {
  scoped_ptr<webview::OverrideUserAgent::Params> params(
      webview::OverrideUserAgent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  guest->SetUserAgentOverride(params->user_agent_override);
  return true;
}

WebViewInternalStopFunction::WebViewInternalStopFunction() {
}

WebViewInternalStopFunction::~WebViewInternalStopFunction() {
}

bool WebViewInternalStopFunction::RunAsyncSafe(WebViewGuest* guest) {
  guest->Stop();
  return true;
}

WebViewInternalTerminateFunction::WebViewInternalTerminateFunction() {
}

WebViewInternalTerminateFunction::~WebViewInternalTerminateFunction() {
}

bool WebViewInternalTerminateFunction::RunAsyncSafe(WebViewGuest* guest) {
  guest->Terminate();
  return true;
}

}  // namespace extensions
