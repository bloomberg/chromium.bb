// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_actions_api.h"

#include <string>

#include "base/base64.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"

namespace {

const char kBrowserActionStorageKey[] = "browser_action";
const char kPopupUrlStorageKey[] = "poupup_url";
const char kTitleStorageKey[] = "title";
const char kIconStorageKey[] = "icon";
const char kBadgeTextStorageKey[] = "badge_text";
const char kBadgeBackgroundColorStorageKey[] = "badge_background_color";
const char kBadgeTextColorStorageKey[] = "badge_text_color";
const char kAppearanceStorageKey[] = "appearance";

// Errors.
const char kNoExtensionActionError[] =
    "This extension has no action specified.";
const char kNoTabError[] = "No tab with id: *.";

struct IconRepresentationInfo {
  // Size as a string that will be used to retrieve representation value from
  // SetIcon function arguments.
  const char* size_string;
  // Scale factor for which the represantion should be used.
  ui::ScaleFactor scale;
};


const IconRepresentationInfo kIconSizes[] = {
    { "19", ui::SCALE_FACTOR_100P },
    { "38", ui::SCALE_FACTOR_200P }
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
  IPC::Message bitmap_pickle(raw_str.data(), raw_str.size());
  PickleIterator iter(bitmap_pickle);
  return IPC::ReadParam(&bitmap_pickle, &iter, bitmap);
}

// Conversion function for reading/writing to storage.
std::string RepresentationToString(const gfx::ImageSkia& image,
                                   ui::ScaleFactor scale) {
  SkBitmap bitmap = image.GetRepresentation(scale).sk_bitmap();
  IPC::Message bitmap_pickle;
  IPC::WriteParam(&bitmap_pickle, bitmap);
  std::string raw_str(static_cast<const char*>(bitmap_pickle.data()),
                      bitmap_pickle.size());
  std::string base64_str;
  if (!base::Base64Encode(raw_str, &base64_str))
    return std::string();
  return base64_str;
}

// Set |action|'s default values to those specified in |dict|.
void SetDefaultsFromValue(const base::DictionaryValue* dict,
                          ExtensionAction* action) {
  const int kTabId = ExtensionAction::kDefaultTabId;
  std::string str_value;
  int int_value;
  SkBitmap bitmap;
  gfx::ImageSkia icon;

  if (dict->GetString(kPopupUrlStorageKey, &str_value))
    action->SetPopupUrl(kTabId, GURL(str_value));
  if (dict->GetString(kTitleStorageKey, &str_value))
    action->SetTitle(kTabId, str_value);
  if (dict->GetString(kBadgeTextStorageKey, &str_value))
    action->SetBadgeText(kTabId, str_value);
  if (dict->GetString(kBadgeBackgroundColorStorageKey, &str_value))
    action->SetBadgeBackgroundColor(kTabId, RawStringToSkColor(str_value));
  if (dict->GetString(kBadgeTextColorStorageKey, &str_value))
    action->SetBadgeTextColor(kTabId, RawStringToSkColor(str_value));
  if (dict->GetInteger(kAppearanceStorageKey, &int_value))
    action->SetAppearance(kTabId,
                          static_cast<ExtensionAction::Appearance>(int_value));

  const base::DictionaryValue* icon_value = NULL;
  if (dict->GetDictionary(kIconStorageKey, &icon_value)) {
    for (size_t i = 0; i < arraysize(kIconSizes); i++) {
      if (icon_value->GetString(kIconSizes[i].size_string, &str_value) &&
          StringToSkBitmap(str_value, &bitmap)) {
        CHECK(!bitmap.isNull());
        icon.AddRepresentation(gfx::ImageSkiaRep(bitmap, kIconSizes[i].scale));
      }
    }
    action->SetIcon(kTabId, gfx::Image(icon));
  }
}

// Store |action|'s default values in a DictionaryValue for use in storing to
// disk.
scoped_ptr<base::DictionaryValue> DefaultsToValue(ExtensionAction* action) {
  const int kTabId = ExtensionAction::kDefaultTabId;
  scoped_ptr<base::DictionaryValue> dict(new DictionaryValue());

  dict->SetString(kPopupUrlStorageKey, action->GetPopupUrl(kTabId).spec());
  dict->SetString(kTitleStorageKey, action->GetTitle(kTabId));
  dict->SetString(kBadgeTextStorageKey, action->GetBadgeText(kTabId));
  dict->SetString(kBadgeBackgroundColorStorageKey,
                  SkColorToRawString(action->GetBadgeBackgroundColor(kTabId)));
  dict->SetString(kBadgeTextColorStorageKey,
                  SkColorToRawString(action->GetBadgeTextColor(kTabId)));
  dict->SetInteger(kAppearanceStorageKey,
                   action->GetIsVisible(kTabId) ?
                       ExtensionAction::ACTIVE : ExtensionAction::INVISIBLE);

  gfx::ImageSkia icon = action->GetExplicitlySetIcon(kTabId);
  if (!icon.isNull()) {
    base::DictionaryValue* icon_value = new base::DictionaryValue();
    for (size_t i = 0; i < arraysize(kIconSizes); i++) {
      if (icon.HasRepresentation(kIconSizes[i].scale)) {
        icon_value->SetString(
            kIconSizes[i].size_string,
            RepresentationToString(icon, kIconSizes[i].scale));
      }
    }
    dict->Set(kIconStorageKey, icon_value);
  }
  return dict.Pass();
}

}  // namespace

namespace extensions {

//
// ExtensionActionStorageManager
//

ExtensionActionStorageManager::ExtensionActionStorageManager(Profile* profile)
    : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
                 content::NotificationService::AllBrowserContextsAndSources());

  StateStore* storage = ExtensionSystem::Get(profile_)->state_store();
  if (storage)
    storage->RegisterKey(kBrowserActionStorageKey);
}

ExtensionActionStorageManager::~ExtensionActionStorageManager() {
}

void ExtensionActionStorageManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (!extension->browser_action())
        break;

      StateStore* storage = ExtensionSystem::Get(profile_)->state_store();
      if (storage) {
        storage->GetExtensionValue(extension->id(), kBrowserActionStorageKey,
            base::Bind(&ExtensionActionStorageManager::ReadFromStorage,
                       AsWeakPtr(), extension->id()));
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED: {
      ExtensionAction* extension_action =
          content::Source<ExtensionAction>(source).ptr();
      Profile* profile = content::Details<Profile>(details).ptr();
      if (profile != profile_)
        break;

      extension_action->set_has_changed(true);
      WriteToStorage(extension_action);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void ExtensionActionStorageManager::WriteToStorage(
    ExtensionAction* extension_action) {
  StateStore* storage = ExtensionSystem::Get(profile_)->state_store();
  if (!storage)
    return;

  scoped_ptr<base::DictionaryValue> defaults =
      DefaultsToValue(extension_action);
  storage->SetExtensionValue(extension_action->extension_id(),
                             kBrowserActionStorageKey,
                             defaults.PassAs<base::Value>());
}

void ExtensionActionStorageManager::ReadFromStorage(
    const std::string& extension_id, scoped_ptr<base::Value> value) {
  const Extension* extension =
      ExtensionSystem::Get(profile_)->extension_service()->
          GetExtensionById(extension_id, true);
  if (!extension)
    return;

  CHECK(extension->browser_action());

  // Don't load values from storage if the extension has updated a value
  // already. The extension may have only updated some of the values, but
  // this is a good first approximation. If the extension is doing stuff
  // to the browser action, we can assume it is ready to take over.
  if (extension->browser_action()->has_changed())
    return;

  const base::DictionaryValue* dict = NULL;
  if (!value.get() || !value->GetAsDictionary(&dict))
    return;

  SetDefaultsFromValue(dict, extension->browser_action());
}

}  // namespace extensions


//
// ExtensionActionFunction
//

ExtensionActionFunction::ExtensionActionFunction()
    : details_(NULL),
      tab_id_(ExtensionAction::kDefaultTabId),
      contents_(NULL),
      extension_action_(NULL) {
}

ExtensionActionFunction::~ExtensionActionFunction() {
}

bool ExtensionActionFunction::RunImpl() {
  if (base::StringPiece(name()).starts_with("scriptBadge.")) {
    extension_action_ = GetExtension()->script_badge();
  } else {
    extension_action_ = GetExtension()->browser_action();
    if (!extension_action_)
      extension_action_ = GetExtension()->page_action();
  }
  if (!extension_action_) {
    // TODO(kalman): ideally the browserAction/pageAction APIs wouldn't event
    // exist for extensions that don't have one declared. This should come as
    // part of the Feature system.
    error_ = kNoExtensionActionError;
    return false;
  }

  // There may or may not be details (depends on the function).
  // The tabId might appear in details (if it exists) or as the first
  // argument besides the action type (depends on the function).
  {
    base::Value* first_arg = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &first_arg));

    switch (first_arg->GetType()) {
      case Value::TYPE_INTEGER:
        CHECK(first_arg->GetAsInteger(&tab_id_));
        break;

      case Value::TYPE_DICTIONARY: {
        details_ = static_cast<base::DictionaryValue*>(first_arg);
        base::Value* tab_id_value = NULL;
        if (details_->Get("tabId", &tab_id_value)) {
          switch (tab_id_value->GetType()) {
            case Value::TYPE_NULL:
              // Fine, equivalent to it being not-there, and tabId is optional.
              break;
            case Value::TYPE_INTEGER:
              CHECK(tab_id_value->GetAsInteger(&tab_id_));
              break;
            default:
              // Boom.
              EXTENSION_FUNCTION_VALIDATE(false);
          }
        }
        break;
      }

      case Value::TYPE_NULL:
        // The tabId might be an optional argument.
        break;

      default:
        EXTENSION_FUNCTION_VALIDATE(false);
    }
  }

  // Find the TabContents that contains this tab id if one is required.
  if (tab_id_ == ExtensionAction::kDefaultTabId) {
    EXTENSION_FUNCTION_VALIDATE(GetExtension()->browser_action());
  } else {
    ExtensionTabUtil::GetTabById(
        tab_id_, profile(), include_incognito(), NULL, NULL, &contents_, NULL);
    if (!contents_) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(
          kNoTabError, base::IntToString(tab_id_));
      return false;
    }
  }

  return RunExtensionAction();
}

void ExtensionActionFunction::NotifyChange() {
  switch (extension_action_->action_type()) {
    case ExtensionAction::TYPE_BROWSER:
    case ExtensionAction::TYPE_PAGE:
      if (extension_->browser_action()) {
        NotifyBrowserActionChange();
      } else if (extension_->page_action()) {
        NotifyLocationBarChange();
      }
      return;
    case ExtensionAction::TYPE_SCRIPT_BADGE:
      NotifyLocationBarChange();
      return;
  }
  NOTREACHED();
}

void ExtensionActionFunction::NotifyBrowserActionChange() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
      content::Source<ExtensionAction>(extension_action_),
      content::Details<Profile>(profile()));
}

void ExtensionActionFunction::NotifyLocationBarChange() {
  extensions::TabHelper::FromWebContents(contents_->web_contents())->
      location_bar_controller()->NotifyChange();
}

// static
bool ExtensionActionFunction::ParseCSSColorString(
    const std::string& color_string,
    SkColor* result) {
  std::string formatted_color = "#";
  // Check the string for incorrect formatting.
  if (color_string[0] != '#')
    return false;

  // Convert the string from #FFF format to #FFFFFF format.
  if (color_string.length() == 4) {
    for (size_t i = 1; i < color_string.length(); i++) {
      formatted_color += color_string[i];
      formatted_color += color_string[i];
    }
  } else {
    formatted_color = color_string;
  }

  if (formatted_color.length() != 7)
    return false;

  // Convert the string to an integer and make sure it is in the correct value
  // range.
  int color_ints[3] = {0};
  for (int i = 0; i < 3; i++) {
    if (!base::HexStringToInt(formatted_color.substr(1 + (2 * i), 2),
                              color_ints + i))
      return false;
    if (color_ints[i] > 255 || color_ints[i] < 0)
      return false;
  }

  *result = SkColorSetARGB(255, color_ints[0], color_ints[1], color_ints[2]);
  return true;
}

bool ExtensionActionFunction::SetVisible(bool visible) {
  extension_action_->SetAppearance(
      tab_id_, visible ? ExtensionAction::ACTIVE : ExtensionAction::INVISIBLE);
  NotifyChange();
  return true;
}

extensions::TabHelper& ExtensionActionFunction::tab_helper() const {
  CHECK(contents_);
  return *extensions::TabHelper::FromWebContents(contents_->web_contents());
}

bool ExtensionActionShowFunction::RunExtensionAction() {
  return SetVisible(true);
}

bool ExtensionActionHideFunction::RunExtensionAction() {
  return SetVisible(false);
}

bool ExtensionActionSetIconFunction::RunExtensionAction() {
  // setIcon can take a variant argument: either a dictionary of canvas
  // ImageData, or an icon index.
  base::DictionaryValue* canvas_set = NULL;
  int icon_index;
  if (details_->GetDictionary("imageData", &canvas_set)) {
    gfx::ImageSkia icon;
    // Extract icon representations from the ImageDataSet dictionary.
    for (size_t i = 0; i < arraysize(kIconSizes); i++) {
      base::BinaryValue* binary;
      if (canvas_set->GetBinary(kIconSizes[i].size_string, &binary)) {
        IPC::Message pickle(binary->GetBuffer(), binary->GetSize());
        PickleIterator iter(pickle);
        SkBitmap bitmap;
        EXTENSION_FUNCTION_VALIDATE(IPC::ReadParam(&pickle, &iter, &bitmap));
        CHECK(!bitmap.isNull());
        icon.AddRepresentation(gfx::ImageSkiaRep(bitmap, kIconSizes[i].scale));
      }
    }

    extension_action_->SetIcon(tab_id_, gfx::Image(icon));
  } else if (details_->GetInteger("iconIndex", &icon_index)) {
    // Obsolete argument: ignore it.
    return true;
  } else {
    EXTENSION_FUNCTION_VALIDATE(false);
  }
  NotifyChange();
  return true;
}

bool ExtensionActionSetTitleFunction::RunExtensionAction() {
  std::string title;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("title", &title));
  extension_action_->SetTitle(tab_id_, title);
  NotifyChange();
  return true;
}

bool ExtensionActionSetPopupFunction::RunExtensionAction() {
  std::string popup_string;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("popup", &popup_string));

  GURL popup_url;
  if (!popup_string.empty())
    popup_url = GetExtension()->GetResourceURL(popup_string);

  extension_action_->SetPopupUrl(tab_id_, popup_url);
  NotifyChange();
  return true;
}

bool ExtensionActionSetBadgeTextFunction::RunExtensionAction() {
  std::string badge_text;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("text", &badge_text));
  extension_action_->SetBadgeText(tab_id_, badge_text);
  NotifyChange();
  return true;
}

bool ExtensionActionSetBadgeBackgroundColorFunction::RunExtensionAction() {
  Value* color_value = NULL;
  details_->Get("color", &color_value);
  SkColor color = 0;
  if (color_value->IsType(Value::TYPE_LIST)) {
    ListValue* list = NULL;
    EXTENSION_FUNCTION_VALIDATE(details_->GetList("color", &list));
    EXTENSION_FUNCTION_VALIDATE(list->GetSize() == 4);

    int color_array[4] = {0};
    for (size_t i = 0; i < arraysize(color_array); ++i) {
      EXTENSION_FUNCTION_VALIDATE(list->GetInteger(i, &color_array[i]));
    }

    color = SkColorSetARGB(color_array[3], color_array[0],
                           color_array[1], color_array[2]);
  } else if (color_value->IsType(Value::TYPE_STRING)) {
    std::string color_string;
    EXTENSION_FUNCTION_VALIDATE(details_->GetString("color", &color_string));
    if (!ParseCSSColorString(color_string, &color))
      return false;
  }

  extension_action_->SetBadgeBackgroundColor(tab_id_, color);
  NotifyChange();
  return true;
}

bool ExtensionActionGetTitleFunction::RunExtensionAction() {
  SetResult(Value::CreateStringValue(extension_action_->GetTitle(tab_id_)));
  return true;
}

bool ExtensionActionGetPopupFunction::RunExtensionAction() {
  SetResult(
      Value::CreateStringValue(extension_action_->GetPopupUrl(tab_id_).spec()));
  return true;
}

bool ExtensionActionGetBadgeTextFunction::RunExtensionAction() {
  SetResult(Value::CreateStringValue(extension_action_->GetBadgeText(tab_id_)));
  return true;
}

bool ExtensionActionGetBadgeBackgroundColorFunction::RunExtensionAction() {
  ListValue* list = new ListValue();
  SkColor color = extension_action_->GetBadgeBackgroundColor(tab_id_);
  list->Append(Value::CreateIntegerValue(SkColorGetR(color)));
  list->Append(Value::CreateIntegerValue(SkColorGetG(color)));
  list->Append(Value::CreateIntegerValue(SkColorGetB(color)));
  list->Append(Value::CreateIntegerValue(SkColorGetA(color)));
  SetResult(list);
  return true;
}
