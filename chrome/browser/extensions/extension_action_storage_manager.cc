// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_storage_manager.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/constants.h"
#include "ui/base/layout.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

namespace {

const char kBrowserActionStorageKey[] = "browser_action";
const char kPopupUrlStorageKey[] = "poupup_url";
const char kTitleStorageKey[] = "title";
const char kIconStorageKey[] = "icon";
const char kBadgeTextStorageKey[] = "badge_text";
const char kBadgeBackgroundColorStorageKey[] = "badge_background_color";
const char kBadgeTextColorStorageKey[] = "badge_text_color";
const char kAppearanceStorageKey[] = "appearance";

// Only add values to the end of this enum, since it's stored in the user's
// Extension State, under the kAppearanceStorageKey.  It represents the
// ExtensionAction's default visibility.
enum StoredAppearance {
  // The action icon is hidden.
  INVISIBLE = 0,
  // The action is trying to get the user's attention but isn't yet
  // running on the page.  Was only used for script badges.
  OBSOLETE_WANTS_ATTENTION = 1,
  // The action icon is visible with its normal appearance.
  ACTIVE = 2,
};

// Conversion function for reading/writing to storage.
SkColor RawStringToSkColor(const std::string& str) {
  uint64 value = 0;
  base::StringToUint64(str, &value);
  SkColor color = static_cast<SkColor>(value);
  DCHECK(value == color);  // ensure value fits into color's 32 bits
  return color;
}

// Conversion function for reading/writing to storage.
std::string SkColorToRawString(SkColor color) {
  return base::Uint64ToString(color);
}

// Conversion function for reading/writing to storage.
bool StringToSkBitmap(const std::string& str, SkBitmap* bitmap) {
  // TODO(mpcomplete): Remove the base64 encode/decode step when
  // http://crbug.com/140546 is fixed.
  std::string raw_str;
  if (!base::Base64Decode(str, &raw_str))
    return false;

  bool success = gfx::PNGCodec::Decode(
      reinterpret_cast<unsigned const char*>(raw_str.data()), raw_str.size(),
      bitmap);
  return success;
}

// Conversion function for reading/writing to storage.
std::string RepresentationToString(const gfx::ImageSkia& image, float scale) {
  SkBitmap bitmap = image.GetRepresentation(scale).sk_bitmap();
  SkAutoLockPixels lock_image(bitmap);
  std::vector<unsigned char> data;
  bool success = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &data);
  if (!success)
    return std::string();

  base::StringPiece raw_str(
      reinterpret_cast<const char*>(&data[0]), data.size());
  std::string base64_str;
  base::Base64Encode(raw_str, &base64_str);
  return base64_str;
}

// Set |action|'s default values to those specified in |dict|.
void SetDefaultsFromValue(const base::DictionaryValue* dict,
                          ExtensionAction* action) {
  const int kDefaultTabId = ExtensionAction::kDefaultTabId;
  std::string str_value;
  int int_value;
  SkBitmap bitmap;
  gfx::ImageSkia icon;

  // For each value, don't set it if it has been modified already.
  if (dict->GetString(kPopupUrlStorageKey, &str_value) &&
      !action->HasPopupUrl(kDefaultTabId)) {
    action->SetPopupUrl(kDefaultTabId, GURL(str_value));
  }
  if (dict->GetString(kTitleStorageKey, &str_value) &&
      !action->HasTitle(kDefaultTabId)) {
    action->SetTitle(kDefaultTabId, str_value);
  }
  if (dict->GetString(kBadgeTextStorageKey, &str_value) &&
      !action->HasBadgeText(kDefaultTabId)) {
    action->SetBadgeText(kDefaultTabId, str_value);
  }
  if (dict->GetString(kBadgeBackgroundColorStorageKey, &str_value) &&
      !action->HasBadgeBackgroundColor(kDefaultTabId)) {
    action->SetBadgeBackgroundColor(kDefaultTabId,
                                    RawStringToSkColor(str_value));
  }
  if (dict->GetString(kBadgeTextColorStorageKey, &str_value) &&
      !action->HasBadgeTextColor(kDefaultTabId)) {
    action->SetBadgeTextColor(kDefaultTabId, RawStringToSkColor(str_value));
  }
  if (dict->GetInteger(kAppearanceStorageKey, &int_value) &&
      !action->HasIsVisible(kDefaultTabId)) {
    switch (int_value) {
      case INVISIBLE:
      case OBSOLETE_WANTS_ATTENTION:
        action->SetIsVisible(kDefaultTabId, false);
        break;
      case ACTIVE:
        action->SetIsVisible(kDefaultTabId, true);
        break;
    }
  }

  const base::DictionaryValue* icon_value = NULL;
  if (dict->GetDictionary(kIconStorageKey, &icon_value) &&
      !action->HasIcon(kDefaultTabId)) {
    for (size_t i = 0; i < extension_misc::kNumExtensionActionIconSizes; i++) {
      const extension_misc::IconRepresentationInfo& icon_info =
          extension_misc::kExtensionActionIconSizes[i];
      if (icon_value->GetString(icon_info.size_string, &str_value) &&
          StringToSkBitmap(str_value, &bitmap)) {
        CHECK(!bitmap.isNull());
        float scale = ui::GetScaleForScaleFactor(icon_info.scale);
        icon.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
      }
    }
    action->SetIcon(kDefaultTabId, gfx::Image(icon));
  }
}

// Store |action|'s default values in a DictionaryValue for use in storing to
// disk.
scoped_ptr<base::DictionaryValue> DefaultsToValue(ExtensionAction* action) {
  const int kDefaultTabId = ExtensionAction::kDefaultTabId;
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString(kPopupUrlStorageKey,
                  action->GetPopupUrl(kDefaultTabId).spec());
  dict->SetString(kTitleStorageKey, action->GetTitle(kDefaultTabId));
  dict->SetString(kBadgeTextStorageKey, action->GetBadgeText(kDefaultTabId));
  dict->SetString(
      kBadgeBackgroundColorStorageKey,
      SkColorToRawString(action->GetBadgeBackgroundColor(kDefaultTabId)));
  dict->SetString(kBadgeTextColorStorageKey,
                  SkColorToRawString(action->GetBadgeTextColor(kDefaultTabId)));
  dict->SetInteger(kAppearanceStorageKey,
                   action->GetIsVisible(kDefaultTabId) ? ACTIVE : INVISIBLE);

  gfx::ImageSkia icon = action->GetExplicitlySetIcon(kDefaultTabId);
  if (!icon.isNull()) {
    scoped_ptr<base::DictionaryValue> icon_value(new base::DictionaryValue());
    for (size_t i = 0; i < extension_misc::kNumExtensionActionIconSizes; i++) {
      const extension_misc::IconRepresentationInfo& icon_info =
          extension_misc::kExtensionActionIconSizes[i];
      float scale = ui::GetScaleForScaleFactor(icon_info.scale);
      if (icon.HasRepresentation(scale)) {
        icon_value->SetString(icon_info.size_string,
                              RepresentationToString(icon, scale));
      }
    }
    dict->Set(kIconStorageKey, icon_value.release());
  }
  return dict.Pass();
}

}  // namespace

ExtensionActionStorageManager::ExtensionActionStorageManager(
    content::BrowserContext* context)
    : browser_context_(context),
      extension_action_observer_(this),
      extension_registry_observer_(this),
      weak_factory_(this) {
  extension_action_observer_.Add(ExtensionActionAPI::Get(browser_context_));
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));

  StateStore* store = GetStateStore();
  if (store)
    store->RegisterKey(kBrowserActionStorageKey);
}

ExtensionActionStorageManager::~ExtensionActionStorageManager() {
}

void ExtensionActionStorageManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (!ExtensionActionManager::Get(browser_context_)->GetBrowserAction(
          *extension))
    return;

  StateStore* store = GetStateStore();
  if (store) {
    store->GetExtensionValue(
        extension->id(),
        kBrowserActionStorageKey,
        base::Bind(&ExtensionActionStorageManager::ReadFromStorage,
                   weak_factory_.GetWeakPtr(),
                   extension->id()));
  }
}

void ExtensionActionStorageManager::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  if (browser_context_ == browser_context &&
      extension_action->action_type() == ActionInfo::TYPE_BROWSER)
    WriteToStorage(extension_action);
}

void ExtensionActionStorageManager::OnExtensionActionAPIShuttingDown() {
  extension_action_observer_.RemoveAll();
}

void ExtensionActionStorageManager::WriteToStorage(
    ExtensionAction* extension_action) {
  StateStore* store = GetStateStore();
  if (store) {
    scoped_ptr<base::DictionaryValue> defaults =
        DefaultsToValue(extension_action);
    store->SetExtensionValue(extension_action->extension_id(),
                             kBrowserActionStorageKey,
                             defaults.PassAs<base::Value>());
  }
}

void ExtensionActionStorageManager::ReadFromStorage(
    const std::string& extension_id, scoped_ptr<base::Value> value) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)->
      enabled_extensions().GetByID(extension_id);
  if (!extension)
    return;

  ExtensionAction* browser_action =
      ExtensionActionManager::Get(browser_context_)->GetBrowserAction(
          *extension);
  if (!browser_action) {
    // This can happen if the extension is updated between startup and when the
    // storage read comes back, and the update removes the browser action.
    // http://crbug.com/349371
    return;
  }

  const base::DictionaryValue* dict = NULL;
  if (!value.get() || !value->GetAsDictionary(&dict))
    return;

  SetDefaultsFromValue(dict, browser_action);
}

StateStore* ExtensionActionStorageManager::GetStateStore() {
  return ExtensionSystem::Get(browser_context_)->state_store();
}

}  // namespace extensions
