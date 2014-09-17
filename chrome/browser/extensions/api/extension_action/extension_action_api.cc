// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"

#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/image_util.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/feature_switch.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

using content::WebContents;

namespace extensions {

namespace {

// Whether the browser action is visible in the toolbar.
const char kBrowserActionVisible[] = "browser_action_visible";

// Errors.
const char kNoExtensionActionError[] =
    "This extension has no action specified.";
const char kNoTabError[] = "No tab with id: *.";
const char kOpenPopupError[] =
    "Failed to show popup either because there is an existing popup or another "
    "error occurred.";

}  // namespace

//
// ExtensionActionAPI::Observer
//

void ExtensionActionAPI::Observer::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
}

void ExtensionActionAPI::Observer::OnPageActionsUpdated(
    content::WebContents* web_contents) {
}

void ExtensionActionAPI::Observer::OnExtensionActionAPIShuttingDown() {
}

ExtensionActionAPI::Observer::~Observer() {
}

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

void ExtensionActionAPI::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionActionAPI::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ExtensionAction::ShowAction ExtensionActionAPI::ExecuteExtensionAction(
    const Extension* extension,
    Browser* browser,
    bool grant_active_tab_permissions) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return ExtensionAction::ACTION_NONE;

  int tab_id = SessionTabHelper::IdForTab(web_contents);

  ActiveScriptController* active_script_controller =
      ActiveScriptController::GetForWebContents(web_contents);
  bool has_pending_scripts = false;
  if (active_script_controller &&
      active_script_controller->WantsToRun(extension)) {
    has_pending_scripts = true;
  }

  // Grant active tab if appropriate.
  if (grant_active_tab_permissions) {
    TabHelper::FromWebContents(web_contents)->active_tab_permission_granter()->
        GrantIfRequested(extension);
  }

  // If this was a request to run a script, it will have been run once active
  // tab was granted. Return without executing the action, since we should only
  // run pending scripts OR the extension action, not both.
  if (has_pending_scripts)
    return ExtensionAction::ACTION_NONE;

  ExtensionAction* extension_action =
      ExtensionActionManager::Get(browser_context_)->GetExtensionAction(
          *extension);

  // Anything that gets here should have a page or browser action.
  DCHECK(extension_action);
  if (!extension_action->GetIsVisible(tab_id))
    return ExtensionAction::ACTION_NONE;

  if (extension_action->HasPopup(tab_id))
    return ExtensionAction::ACTION_SHOW_POPUP;

  ExtensionActionExecuted(*extension_action, web_contents);
  return ExtensionAction::ACTION_NONE;
}

bool ExtensionActionAPI::ShowExtensionActionPopup(
    const Extension* extension,
    Browser* browser,
    bool grant_active_tab_permissions) {
  ExtensionAction* extension_action =
      ExtensionActionManager::Get(browser_context_)->GetExtensionAction(
          *extension);
  if (!extension_action)
    return false;

  if (extension_action->action_type() == ActionInfo::TYPE_PAGE &&
      !FeatureSwitch::extension_action_redesign()->IsEnabled()) {
    // We show page actions in the location bar unless the new toolbar is
    // enabled.
    return browser->window()->GetLocationBar()->ShowPageActionPopup(
        extension, grant_active_tab_permissions);
  } else {
    return ExtensionToolbarModel::Get(browser->profile())->
        ShowExtensionActionPopup(
            extension, browser, grant_active_tab_permissions);
  }
}

void ExtensionActionAPI::NotifyChange(ExtensionAction* extension_action,
                                      content::WebContents* web_contents,
                                      content::BrowserContext* context) {
  FOR_EACH_OBSERVER(
      Observer,
      observers_,
      OnExtensionActionUpdated(extension_action, web_contents, context));

  if (extension_action->action_type() == ActionInfo::TYPE_PAGE)
    NotifyPageActionsChanged(web_contents);
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

void ExtensionActionAPI::ExtensionActionExecuted(
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
        web_contents->GetBrowserContext(),
        extension_action.extension_id(),
        event_name,
        args.Pass());
  }
}

void ExtensionActionAPI::NotifyPageActionsChanged(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;
  LocationBar* location_bar =
      browser->window() ? browser->window()->GetLocationBar() : NULL;
  if (!location_bar)
    return;
  location_bar->UpdatePageActions();

  FOR_EACH_OBSERVER(Observer, observers_, OnPageActionsUpdated(web_contents));
}

void ExtensionActionAPI::Shutdown() {
  FOR_EACH_OBSERVER(Observer, observers_, OnExtensionActionAPIShuttingDown());
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

    EXTENSION_FUNCTION_VALIDATE(
        ExtensionAction::ParseIconFromCanvasDictionary(*canvas_set, &icon));

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
  // We only allow the popup in the active window.
  Browser* browser = chrome::FindLastActiveWithProfile(
                         GetProfile(), chrome::GetActiveDesktop());

  // If there's no active browser, or the Toolbar isn't visible, abort.
  // Otherwise, try to open a popup in the active browser.
  // TODO(justinlin): Remove toolbar check when http://crbug.com/308645 is
  // fixed.
  if (!browser ||
      !browser->window()->IsActive() ||
      !browser->window()->IsToolbarVisible() ||
      !ExtensionActionAPI::Get(GetProfile())->ShowExtensionActionPopup(
          extension_.get(), browser, false)) {
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
