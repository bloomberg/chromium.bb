// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"

#include "base/base64.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api_constants.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_util.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/error_utils.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

using content::WebContents;

namespace page_actions_keys = extension_page_actions_api_constants;

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

// Whether the browser action is visible in the toolbar.
const char kBrowserActionVisible[] = "browser_action_visible";

// Errors.
const char kNoExtensionActionError[] =
    "This extension has no action specified.";
const char kNoTabError[] = "No tab with id: *.";
const char kOpenPopupError[] =
    "Failed to show popup either because there is an existing popup or another "
    "error occurred.";
const char kInternalError[] = "Internal error.";

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
    for (size_t i = 0; i < arraysize(kIconSizes); i++) {
      if (icon_value->GetString(kIconSizes[i].size_string, &str_value) &&
          StringToSkBitmap(str_value, &bitmap)) {
        CHECK(!bitmap.isNull());
        float scale = ui::GetScaleForScaleFactor(kIconSizes[i].scale);
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
    base::DictionaryValue* icon_value = new base::DictionaryValue();
    for (size_t i = 0; i < arraysize(kIconSizes); i++) {
      float scale = ui::GetScaleForScaleFactor(kIconSizes[i].scale);
      if (icon.HasRepresentation(scale)) {
        icon_value->SetString(
            kIconSizes[i].size_string,
            RepresentationToString(icon, scale));
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

static base::LazyInstance<BrowserContextKeyedAPIFactory<ExtensionActionAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

ExtensionActionAPI::ExtensionActionAPI(content::BrowserContext* context)
    : browser_context_(context) {
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
  registry->RegisterFunction<BrowserActionOpenPopupFunction>();

  // Page Actions
  registry->RegisterFunction<PageActionShowFunction>();
  registry->RegisterFunction<PageActionHideFunction>();
  registry->RegisterFunction<PageActionSetIconFunction>();
  registry->RegisterFunction<PageActionSetTitleFunction>();
  registry->RegisterFunction<PageActionSetPopupFunction>();
  registry->RegisterFunction<PageActionGetTitleFunction>();
  registry->RegisterFunction<PageActionGetPopupFunction>();
}

ExtensionActionAPI::~ExtensionActionAPI() {
}

// static
BrowserContextKeyedAPIFactory<ExtensionActionAPI>*
ExtensionActionAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
ExtensionActionAPI* ExtensionActionAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<ExtensionActionAPI>::Get(context);
}

// static
bool ExtensionActionAPI::GetBrowserActionVisibility(
    const ExtensionPrefs* prefs,
    const std::string& extension_id) {
  bool visible = false;
  if (!prefs || !prefs->ReadPrefAsBoolean(extension_id,
                                          kBrowserActionVisible,
                                          &visible)) {
    return true;
  }
  return visible;
}

// static
void ExtensionActionAPI::SetBrowserActionVisibility(
    ExtensionPrefs* prefs,
    const std::string& extension_id,
    bool visible) {
  if (GetBrowserActionVisibility(prefs, extension_id) == visible)
    return;

  prefs->UpdateExtensionPref(extension_id,
                             kBrowserActionVisible,
                             new base::FundamentalValue(visible));
  content::NotificationService::current()->Notify(
      NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
      content::Source<ExtensionPrefs>(prefs),
      content::Details<const std::string>(&extension_id));
}

// static
void ExtensionActionAPI::BrowserActionExecuted(
    content::BrowserContext* context,
    const ExtensionAction& browser_action,
    WebContents* web_contents) {
  ExtensionActionExecuted(context, browser_action, web_contents);
}

// static
void ExtensionActionAPI::PageActionExecuted(content::BrowserContext* context,
                                            const ExtensionAction& page_action,
                                            int tab_id,
                                            const std::string& url,
                                            int button) {
  WebContents* web_contents = NULL;
  if (!ExtensionTabUtil::GetTabById(
           tab_id,
           Profile::FromBrowserContext(context),
           context->IsOffTheRecord(),
           NULL,
           NULL,
           &web_contents,
           NULL)) {
    return;
  }
  ExtensionActionExecuted(context, page_action, web_contents);
}

// static
void ExtensionActionAPI::DispatchEventToExtension(
    content::BrowserContext* context,
    const std::string& extension_id,
    const std::string& event_name,
    scoped_ptr<base::ListValue> event_args) {
  if (!EventRouter::Get(context))
    return;

  scoped_ptr<Event> event(new Event(event_name, event_args.Pass()));
  event->restrict_to_browser_context = context;
  event->user_gesture = EventRouter::USER_GESTURE_ENABLED;
  EventRouter::Get(context)
      ->DispatchEventToExtension(extension_id, event.Pass());
}

// static
void ExtensionActionAPI::ExtensionActionExecuted(
    content::BrowserContext* context,
    const ExtensionAction& extension_action,
    WebContents* web_contents) {
  const char* event_name = NULL;
  switch (extension_action.action_type()) {
    case ActionInfo::TYPE_BROWSER:
      event_name = "browserAction.onClicked";
      break;
    case ActionInfo::TYPE_PAGE:
      event_name = "pageAction.onClicked";
      break;
    case ActionInfo::TYPE_SYSTEM_INDICATOR:
      // The System Indicator handles its own clicks.
      break;
  }

  if (event_name) {
    scoped_ptr<base::ListValue> args(new base::ListValue());
    base::DictionaryValue* tab_value =
        ExtensionTabUtil::CreateTabValue(web_contents);
    args->Append(tab_value);

    DispatchEventToExtension(
        context, extension_action.extension_id(), event_name, args.Pass());
  }
}

void ExtensionActionAPI::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionActionAPI::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionActionAPI::NotifyChange(ExtensionAction* extension_action,
                                      content::WebContents* web_contents,
                                      content::BrowserContext* context) {
  FOR_EACH_OBSERVER(
      Observer,
      observers_,
      OnExtensionActionUpdated(extension_action, web_contents, context));
}

void ExtensionActionAPI::ClearAllValuesForTab(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();
  ExtensionActionManager* action_manager =
      ExtensionActionManager::Get(browser_context_);

  for (ExtensionSet::const_iterator iter = enabled_extensions.begin();
       iter != enabled_extensions.end(); ++iter) {
    ExtensionAction* extension_action =
        action_manager->GetBrowserAction(*iter->get());
    if (!extension_action)
      extension_action = action_manager->GetPageAction(*iter->get());
    if (extension_action) {
      extension_action->ClearAllValuesForTab(tab_id);
      NotifyChange(extension_action, web_contents, browser_context);
    }
  }
}

void ExtensionActionAPI::Shutdown() {
  FOR_EACH_OBSERVER(Observer, observers_, OnExtensionActionAPIShuttingDown());
}

//
// ExtensionActionStorageManager
//

ExtensionActionStorageManager::ExtensionActionStorageManager(Profile* profile)
    : profile_(profile),
      extension_action_observer_(this),
      extension_registry_observer_(this) {
  extension_action_observer_.Add(ExtensionActionAPI::Get(profile_));
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));

  StateStore* storage = ExtensionSystem::Get(profile_)->state_store();
  if (storage)
    storage->RegisterKey(kBrowserActionStorageKey);
}

ExtensionActionStorageManager::~ExtensionActionStorageManager() {
}

void ExtensionActionStorageManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (!ExtensionActionManager::Get(profile_)->GetBrowserAction(*extension))
    return;

  StateStore* storage = ExtensionSystem::Get(profile_)->state_store();
  if (storage) {
    storage->GetExtensionValue(
        extension->id(),
        kBrowserActionStorageKey,
        base::Bind(&ExtensionActionStorageManager::ReadFromStorage,
                   AsWeakPtr(),
                   extension->id()));
  }
}

void ExtensionActionStorageManager::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  if (profile_ == browser_context &&
      extension_action->action_type() == ActionInfo::TYPE_BROWSER)
    WriteToStorage(extension_action);
}

void ExtensionActionStorageManager::OnExtensionActionAPIShuttingDown() {
  extension_action_observer_.RemoveAll();
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

bool ExtensionActionFunction::RunSync() {
  ExtensionActionManager* manager = ExtensionActionManager::Get(GetProfile());
  if (StartsWithASCII(name(), "systemIndicator.", false)) {
    extension_action_ = manager->GetSystemIndicator(*extension());
  } else {
    extension_action_ = manager->GetBrowserAction(*extension());
    if (!extension_action_) {
      extension_action_ = manager->GetPageAction(*extension());
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
    ExtensionTabUtil::GetTabById(tab_id_,
                                 GetProfile(),
                                 include_incognito(),
                                 NULL,
                                 NULL,
                                 &contents_,
                                 NULL);
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
    case base::Value::TYPE_INTEGER:
      CHECK(first_arg->GetAsInteger(&tab_id_));
      break;

    case base::Value::TYPE_DICTIONARY: {
      // Found the details argument.
      details_ = static_cast<base::DictionaryValue*>(first_arg);
      // Still need to check for the tabId within details.
      base::Value* tab_id_value = NULL;
      if (details_->Get("tabId", &tab_id_value)) {
        switch (tab_id_value->GetType()) {
          case base::Value::TYPE_NULL:
            // OK; tabId is optional, leave it default.
            return true;
          case base::Value::TYPE_INTEGER:
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

    case base::Value::TYPE_NULL:
      // The tabId might be an optional argument.
      break;

    default:
      return false;
  }

  return true;
}

void ExtensionActionFunction::NotifyChange() {
  ExtensionActionAPI::Get(GetProfile())->NotifyChange(
      extension_action_, contents_, GetProfile());
}

bool ExtensionActionFunction::SetVisible(bool visible) {
  if (extension_action_->GetIsVisible(tab_id_) == visible)
    return true;
  extension_action_->SetIsVisible(tab_id_, visible);
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
        float scale = ui::GetScaleForScaleFactor(kIconSizes[i].scale);
        icon.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
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
    popup_url = extension()->GetResourceURL(popup_string);

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
  base::Value* color_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(details_->Get("color", &color_value));
  SkColor color = 0;
  if (color_value->IsType(base::Value::TYPE_LIST)) {
    base::ListValue* list = NULL;
    EXTENSION_FUNCTION_VALIDATE(details_->GetList("color", &list));
    EXTENSION_FUNCTION_VALIDATE(list->GetSize() == 4);

    int color_array[4] = {0};
    for (size_t i = 0; i < arraysize(color_array); ++i) {
      EXTENSION_FUNCTION_VALIDATE(list->GetInteger(i, &color_array[i]));
    }

    color = SkColorSetARGB(color_array[3], color_array[0],
                           color_array[1], color_array[2]);
  } else if (color_value->IsType(base::Value::TYPE_STRING)) {
    std::string color_string;
    EXTENSION_FUNCTION_VALIDATE(details_->GetString("color", &color_string));
    if (!image_util::ParseCSSColorString(color_string, &color))
      return false;
  }

  extension_action_->SetBadgeBackgroundColor(tab_id_, color);
  NotifyChange();
  return true;
}

bool ExtensionActionGetTitleFunction::RunExtensionAction() {
  SetResult(new base::StringValue(extension_action_->GetTitle(tab_id_)));
  return true;
}

bool ExtensionActionGetPopupFunction::RunExtensionAction() {
  SetResult(
      new base::StringValue(extension_action_->GetPopupUrl(tab_id_).spec()));
  return true;
}

bool ExtensionActionGetBadgeTextFunction::RunExtensionAction() {
  SetResult(new base::StringValue(extension_action_->GetBadgeText(tab_id_)));
  return true;
}

bool ExtensionActionGetBadgeBackgroundColorFunction::RunExtensionAction() {
  base::ListValue* list = new base::ListValue();
  SkColor color = extension_action_->GetBadgeBackgroundColor(tab_id_);
  list->Append(
      new base::FundamentalValue(static_cast<int>(SkColorGetR(color))));
  list->Append(
      new base::FundamentalValue(static_cast<int>(SkColorGetG(color))));
  list->Append(
      new base::FundamentalValue(static_cast<int>(SkColorGetB(color))));
  list->Append(
      new base::FundamentalValue(static_cast<int>(SkColorGetA(color))));
  SetResult(list);
  return true;
}

BrowserActionOpenPopupFunction::BrowserActionOpenPopupFunction()
    : response_sent_(false) {
}

bool BrowserActionOpenPopupFunction::RunAsync() {
  ExtensionToolbarModel* model = ExtensionToolbarModel::Get(GetProfile());
  if (!model) {
    error_ = kInternalError;
    return false;
  }

  if (!model->ShowBrowserActionPopup(extension_)) {
    error_ = kOpenPopupError;
    return false;
  }

  registrar_.Add(this,
                 NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                 content::Source<Profile>(GetProfile()));

  // Set a timeout for waiting for the notification that the popup is loaded.
  // Waiting is required so that the popup view can be retrieved by the custom
  // bindings for the response callback. It's also needed to keep this function
  // instance around until a notification is observed.
  base::MessageLoopForUI::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BrowserActionOpenPopupFunction::OpenPopupTimedOut, this),
      base::TimeDelta::FromSeconds(10));
  return true;
}

void BrowserActionOpenPopupFunction::OpenPopupTimedOut() {
  if (response_sent_)
    return;

  DVLOG(1) << "chrome.browserAction.openPopup did not show a popup.";
  error_ = kOpenPopupError;
  SendResponse(false);
  response_sent_ = true;
}

void BrowserActionOpenPopupFunction::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING, type);
  if (response_sent_)
    return;

  ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
  if (host->extension_host_type() != VIEW_TYPE_EXTENSION_POPUP ||
      host->extension()->id() != extension_->id())
    return;

  SendResponse(true);
  response_sent_ = true;
  registrar_.RemoveAll();
}

}  // namespace extensions
