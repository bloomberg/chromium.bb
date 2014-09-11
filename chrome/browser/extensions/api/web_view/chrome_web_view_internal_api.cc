// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_view/chrome_web_view_internal_api.h"

#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"
#include "chrome/browser/extensions/api/context_menus/context_menus_api.h"
#include "chrome/browser/extensions/api/context_menus/context_menus_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/chrome_web_view_internal.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/error_utils.h"

namespace helpers = extensions::context_menus_api_helpers;
namespace webview = extensions::api::chrome_web_view_internal;

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

// TODO(lazyboy): Add checks similar to
// WebViewInternalExtensionFunction::RunAsyncSafe(WebViewGuest*).
bool ChromeWebViewInternalContextMenusCreateFunction::RunAsync() {
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
      extension(),
      id,
      &error_);

  SendResponse(success);
  return success;
}

bool ChromeWebViewInternalContextMenusUpdateFunction::RunAsync() {
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
      params->update_properties, profile, extension(), item_id, &error_);
  SendResponse(success);
  return success;
}

bool ChromeWebViewInternalContextMenusRemoveFunction::RunAsync() {
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

bool ChromeWebViewInternalContextMenusRemoveAllFunction::RunAsync() {
  scoped_ptr<webview::ContextMenusRemoveAll::Params> params(
      webview::ContextMenusRemoveAll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MenuManager* menu_manager =
      MenuManager::Get(Profile::FromBrowserContext(browser_context()));

  int webview_instance_id = params->instance_id;
  menu_manager->RemoveAllContextItems(
      MenuItem::ExtensionKey(extension()->id(), webview_instance_id));
  SendResponse(true);
  return true;
}

ChromeWebViewInternalClearDataFunction::ChromeWebViewInternalClearDataFunction()
    : remove_mask_(0), bad_message_(false) {
}

ChromeWebViewInternalClearDataFunction::
    ~ChromeWebViewInternalClearDataFunction() {
}

// Parses the |dataToRemove| argument to generate the remove mask. Sets
// |bad_message_| (like EXTENSION_FUNCTION_VALIDATE would if this were a bool
// method) if 'dataToRemove' is not present.
uint32 ChromeWebViewInternalClearDataFunction::GetRemovalMask() {
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
// TODO(lf): Move ClearDataFunction to extensions
bool ChromeWebViewInternalClearDataFunction::RunAsyncSafe(WebViewGuest* guest) {
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
        base::Bind(&ChromeWebViewInternalClearDataFunction::ClearDataDone,
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

void ChromeWebViewInternalClearDataFunction::ClearDataDone() {
  Release();  // Balanced in RunAsync().
  SendResponse(true);
}

ChromeWebViewInternalShowContextMenuFunction::
    ChromeWebViewInternalShowContextMenuFunction() {
}

ChromeWebViewInternalShowContextMenuFunction::
    ~ChromeWebViewInternalShowContextMenuFunction() {
}

bool ChromeWebViewInternalShowContextMenuFunction::RunAsyncSafe(
    WebViewGuest* guest) {
  scoped_ptr<webview::ShowContextMenu::Params> params(
      webview::ShowContextMenu::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(lazyboy): Actually implement filtering menu items, we pass NULL for
  // now.
  guest->ShowContextMenu(params->request_id, NULL);

  SendResponse(true);
  return true;
}

}  // namespace extensions
