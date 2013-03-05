// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"

#include <string>

#include "base/base64.h"
#include "base/lazy_instance.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api_constants.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/api/extension_action/browser_action_handler.h"
#include "chrome/common/extensions/api/extension_action/page_action_handler.h"
#include "chrome/common/extensions/api/extension_action/script_badge_handler.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/error_utils.h"
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

// Errors.
const char kNoExtensionActionError[] =
    "This extension has no action specified.";
const char kNoTabError[] = "No tab with id: *.";
const char kNoPageActionError[] =
    "This extension has no page action specified.";
const char kUrlNotActiveError[] = "This url is no longer active: *.";

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
  // Clear the header values so they don't vary in serialization.
  bitmap_pickle.SetHeaderValues(0, 0, 0);
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

//
// ExtensionActionAPI
//

static base::LazyInstance<ProfileKeyedAPIFactory<ExtensionActionAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

ExtensionActionAPI::ExtensionActionAPI(Profile* profile) {
  (new BrowserActionHandler)->Register();
  (new PageActionHandler)->Register();
  (new ScriptBadgeHandler)->Register();

  ExtensionFunctionRegistry* registry =
      ExtensionFunctionRegistry::GetInstance();

  // Browser Actions
  registry->RegisterFunction<BrowserActionSetIconFunction>();
  registry->RegisterFunction<BrowserActionSetTitleFunction>();
  registry->RegisterFunction<BrowserActionSetBadgeTextFunction>();
  registry->RegisterFunction<BrowserActionSetBadgeBackgroundColorFunction>();
  registry->RegisterFunction<BrowserActionSetPopupFunction>();
  registry->RegisterFunction<BrowserActionGetTitleFunction>();
  registry->RegisterFunction<BrowserActionGetBadgeTextFunction>();
  registry->RegisterFunction<BrowserActionGetBadgeBackgroundColorFunction>();
  registry->RegisterFunction<BrowserActionGetPopupFunction>();
  registry->RegisterFunction<BrowserActionEnableFunction>();
  registry->RegisterFunction<BrowserActionDisableFunction>();

  // Page Actions
  registry->RegisterFunction<EnablePageActionsFunction>();
  registry->RegisterFunction<DisablePageActionsFunction>();
  registry->RegisterFunction<PageActionShowFunction>();
  registry->RegisterFunction<PageActionHideFunction>();
  registry->RegisterFunction<PageActionSetIconFunction>();
  registry->RegisterFunction<PageActionSetTitleFunction>();
  registry->RegisterFunction<PageActionSetPopupFunction>();
  registry->RegisterFunction<PageActionGetTitleFunction>();
  registry->RegisterFunction<PageActionGetPopupFunction>();

  // Script Badges
  registry->RegisterFunction<ScriptBadgeGetAttentionFunction>();
  registry->RegisterFunction<ScriptBadgeGetPopupFunction>();
  registry->RegisterFunction<ScriptBadgeSetPopupFunction>();
}

ExtensionActionAPI::~ExtensionActionAPI() {
}

// static
ProfileKeyedAPIFactory<ExtensionActionAPI>*
ExtensionActionAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

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
      if (!ExtensionActionManager::Get(profile_)->
          GetBrowserAction(*extension)) {
        break;
      }

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
      extensions()->GetByID(extension_id);
  if (!extension)
    return;

  ExtensionAction* browser_action =
      ExtensionActionManager::Get(profile_)->GetBrowserAction(*extension);
  CHECK(browser_action);

  // Don't load values from storage if the extension has updated a value
  // already. The extension may have only updated some of the values, but
  // this is a good first approximation. If the extension is doing stuff
  // to the browser action, we can assume it is ready to take over.
  if (browser_action->has_changed())
    return;

  const base::DictionaryValue* dict = NULL;
  if (!value.get() || !value->GetAsDictionary(&dict))
    return;

  SetDefaultsFromValue(dict, browser_action);
}

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
  ExtensionActionManager* manager = ExtensionActionManager::Get(profile_);
  const Extension* extension = GetExtension();
  if (StartsWithASCII(name(), "scriptBadge.", false)) {
    extension_action_ = manager->GetScriptBadge(*extension);
  } else if (StartsWithASCII(name(), "systemIndicator.", false)) {
    extension_action_ = manager->GetSystemIndicator(*extension);
  } else {
    extension_action_ = manager->GetBrowserAction(*extension);
    if (!extension_action_) {
      extension_action_ = manager->GetPageAction(*extension);
    }
  }
  if (!extension_action_) {
    // TODO(kalman): ideally the browserAction/pageAction APIs wouldn't event
    // exist for extensions that don't have one declared. This should come as
    // part of the Feature system.
    error_ = kNoExtensionActionError;
    return false;
  }

  // Populates the tab_id_ and details_ members.
  EXTENSION_FUNCTION_VALIDATE(ExtractDataFromArguments());

  // Find the WebContents that contains this tab id if one is required.
  if (tab_id_ != ExtensionAction::kDefaultTabId) {
    ExtensionTabUtil::GetTabById(
        tab_id_, profile(), include_incognito(), NULL, NULL, &contents_, NULL);
    if (!contents_) {
      error_ = ErrorUtils::FormatErrorMessage(
          kNoTabError, base::IntToString(tab_id_));
      return false;
    }
  } else {
    // Only browser actions and system indicators have a default tabId.
    ActionInfo::Type action_type = extension_action_->action_type();
    EXTENSION_FUNCTION_VALIDATE(
        action_type == ActionInfo::TYPE_BROWSER ||
        action_type == ActionInfo::TYPE_SYSTEM_INDICATOR);
  }
  return RunExtensionAction();
}

bool ExtensionActionFunction::ExtractDataFromArguments() {
  // There may or may not be details (depends on the function).
  // The tabId might appear in details (if it exists), as the first
  // argument besides the action type (depends on the function), or be omitted
  // entirely.
  base::Value* first_arg = NULL;
  if (!args_->Get(0, &first_arg))
    return true;

  switch (first_arg->GetType()) {
    case Value::TYPE_INTEGER:
      CHECK(first_arg->GetAsInteger(&tab_id_));
      break;

    case Value::TYPE_DICTIONARY: {
      // Found the details argument.
      details_ = static_cast<base::DictionaryValue*>(first_arg);
      // Still need to check for the tabId within details.
      base::Value* tab_id_value = NULL;
      if (details_->Get("tabId", &tab_id_value)) {
        switch (tab_id_value->GetType()) {
          case Value::TYPE_NULL:
            // OK; tabId is optional, leave it default.
            return true;
          case Value::TYPE_INTEGER:
            CHECK(tab_id_value->GetAsInteger(&tab_id_));
            return true;
          default:
            // Boom.
            return false;
        }
      }
      // Not found; tabId is optional, leave it default.
      break;
    }

    case Value::TYPE_NULL:
      // The tabId might be an optional argument.
      break;

    default:
      return false;
  }

  return true;
}

void ExtensionActionFunction::NotifyChange() {
  switch (extension_action_->action_type()) {
    case ActionInfo::TYPE_BROWSER:
    case ActionInfo::TYPE_PAGE:
      if (ExtensionActionManager::Get(profile_)->
          GetBrowserAction(*extension_)) {
        NotifyBrowserActionChange();
      } else if (ExtensionActionManager::Get(profile_)->
                     GetPageAction(*extension_)) {
        NotifyLocationBarChange();
      }
      return;
    case ActionInfo::TYPE_SCRIPT_BADGE:
      NotifyLocationBarChange();
      return;
    case ActionInfo::TYPE_SYSTEM_INDICATOR:
      NotifySystemIndicatorChange();
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
  TabHelper::FromWebContents(contents_)->
      location_bar_controller()->NotifyChange();
}

void ExtensionActionFunction::NotifySystemIndicatorChange() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_SYSTEM_INDICATOR_UPDATED,
      content::Source<Profile>(profile()),
      content::Details<ExtensionAction>(extension_action_));
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
  if (extension_action_->GetIsVisible(tab_id_) == visible)
    return true;
  extension_action_->SetAppearance(
      tab_id_, visible ? ExtensionAction::ACTIVE : ExtensionAction::INVISIBLE);
  NotifyChange();
  return true;
}

TabHelper& ExtensionActionFunction::tab_helper() const {
  CHECK(contents_);
  return *TabHelper::FromWebContents(contents_);
}

bool ExtensionActionShowFunction::RunExtensionAction() {
  return SetVisible(true);
}

bool ExtensionActionHideFunction::RunExtensionAction() {
  return SetVisible(false);
}

bool ExtensionActionSetIconFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);

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
  EXTENSION_FUNCTION_VALIDATE(details_);
  std::string title;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("title", &title));
  extension_action_->SetTitle(tab_id_, title);
  NotifyChange();
  return true;
}

bool ExtensionActionSetPopupFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
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
  EXTENSION_FUNCTION_VALIDATE(details_);
  std::string badge_text;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("text", &badge_text));
  extension_action_->SetBadgeText(tab_id_, badge_text);
  NotifyChange();
  return true;
}

bool ExtensionActionSetBadgeBackgroundColorFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  Value* color_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(details_->Get("color", &color_value));
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

//
// ScriptBadgeGetAttentionFunction
//

ScriptBadgeGetAttentionFunction::~ScriptBadgeGetAttentionFunction() {}

bool ScriptBadgeGetAttentionFunction::RunExtensionAction() {
  tab_helper().location_bar_controller()->GetAttentionFor(extension_id());
  return true;
}

}  // namespace extensions

//
// PageActionsFunction (deprecated)
//

namespace keys = extension_page_actions_api_constants;

PageActionsFunction::PageActionsFunction() {
}

PageActionsFunction::~PageActionsFunction() {
}

bool PageActionsFunction::SetPageActionEnabled(bool enable) {
  std::string extension_action_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_action_id));
  DictionaryValue* action = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &action));

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(action->GetInteger(keys::kTabIdKey, &tab_id));
  std::string url;
  EXTENSION_FUNCTION_VALIDATE(action->GetString(keys::kUrlKey, &url));

  std::string title;
  if (enable) {
    if (action->HasKey(keys::kTitleKey))
      EXTENSION_FUNCTION_VALIDATE(action->GetString(keys::kTitleKey, &title));
  }

  ExtensionAction* page_action =
      extensions::ExtensionActionManager::Get(profile())->
      GetPageAction(*GetExtension());
  if (!page_action) {
    error_ = extensions::kNoPageActionError;
    return false;
  }

  // Find the WebContents that contains this tab id.
  content::WebContents* contents = NULL;
  bool result = ExtensionTabUtil::GetTabById(
      tab_id, profile(), include_incognito(), NULL, NULL, &contents, NULL);
  if (!result || !contents) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        extensions::kNoTabError, base::IntToString(tab_id));
    return false;
  }

  // Make sure the URL hasn't changed.
  content::NavigationEntry* entry = contents->GetController().GetActiveEntry();
  if (!entry || url != entry->GetURL().spec()) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        extensions::kUrlNotActiveError, url);
    return false;
  }

  // Set visibility and broadcast notifications that the UI should be updated.
  page_action->SetAppearance(
      tab_id, enable ? ExtensionAction::ACTIVE : ExtensionAction::INVISIBLE);
  page_action->SetTitle(tab_id, title);
  extensions::TabHelper::FromWebContents(contents)->
      location_bar_controller()->NotifyChange();

  return true;
}

bool EnablePageActionsFunction::RunImpl() {
  return SetPageActionEnabled(true);
}

bool DisablePageActionsFunction::RunImpl() {
  return SetPageActionEnabled(false);
}
