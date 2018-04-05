// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/image_util.h"
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
const char kInvalidColorError[] =
    "The color specification could not be parsed.";

}  // namespace

//
// ExtensionActionAPI::Observer
//

void ExtensionActionAPI::Observer::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
}

void ExtensionActionAPI::Observer::OnExtensionActionVisibilityChanged(
    const std::string& extension_id,
    bool is_now_visible) {
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

static base::LazyInstance<BrowserContextKeyedAPIFactory<ExtensionActionAPI>>::
    DestructorAtExit g_extension_action_api_factory = LAZY_INSTANCE_INITIALIZER;

ExtensionActionAPI::ExtensionActionAPI(content::BrowserContext* context)
    : browser_context_(context),
      extension_prefs_(nullptr) {
  ExtensionFunctionRegistry& registry =
      ExtensionFunctionRegistry::GetInstance();

  // Browser Actions
  registry.RegisterFunction<BrowserActionSetIconFunction>();
  registry.RegisterFunction<BrowserActionSetTitleFunction>();
  registry.RegisterFunction<BrowserActionSetBadgeTextFunction>();
  registry.RegisterFunction<BrowserActionSetBadgeBackgroundColorFunction>();
  registry.RegisterFunction<BrowserActionSetPopupFunction>();
  registry.RegisterFunction<BrowserActionGetTitleFunction>();
  registry.RegisterFunction<BrowserActionGetBadgeTextFunction>();
  registry.RegisterFunction<BrowserActionGetBadgeBackgroundColorFunction>();
  registry.RegisterFunction<BrowserActionGetPopupFunction>();
  registry.RegisterFunction<BrowserActionEnableFunction>();
  registry.RegisterFunction<BrowserActionDisableFunction>();
  registry.RegisterFunction<BrowserActionOpenPopupFunction>();

  // Page Actions
  registry.RegisterFunction<PageActionShowFunction>();
  registry.RegisterFunction<PageActionHideFunction>();
  registry.RegisterFunction<PageActionSetIconFunction>();
  registry.RegisterFunction<PageActionSetTitleFunction>();
  registry.RegisterFunction<PageActionSetPopupFunction>();
  registry.RegisterFunction<PageActionGetTitleFunction>();
  registry.RegisterFunction<PageActionGetPopupFunction>();
}

ExtensionActionAPI::~ExtensionActionAPI() {
}

// static
BrowserContextKeyedAPIFactory<ExtensionActionAPI>*
ExtensionActionAPI::GetFactoryInstance() {
  return g_extension_action_api_factory.Pointer();
}

// static
ExtensionActionAPI* ExtensionActionAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<ExtensionActionAPI>::Get(context);
}

void ExtensionActionAPI::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionActionAPI::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ExtensionActionAPI::GetBrowserActionVisibility(
    const std::string& extension_id) {
  bool visible = false;
  ExtensionPrefs* prefs = GetExtensionPrefs();
  if (!prefs || !prefs->ReadPrefAsBoolean(extension_id,
                                          kBrowserActionVisible,
                                          &visible)) {
    return true;
  }
  return visible;
}

void ExtensionActionAPI::SetBrowserActionVisibility(
    const std::string& extension_id,
    bool visible) {
  if (GetBrowserActionVisibility(extension_id) == visible)
    return;

  GetExtensionPrefs()->UpdateExtensionPref(
      extension_id, kBrowserActionVisible,
      std::make_unique<base::Value>(visible));
  for (auto& observer : observers_)
    observer.OnExtensionActionVisibilityChanged(extension_id, visible);
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

  // Don't support showing action popups in a popup window.
  if (!browser->SupportsWindowFeature(Browser::FEATURE_TOOLBAR))
    return false;

  ToolbarActionsBar* toolbar_actions_bar =
      browser->window()->GetToolbarActionsBar();
  // ToolbarActionsBar could be null if, e.g., this is a popup window with no
  // toolbar.
  return toolbar_actions_bar &&
         toolbar_actions_bar->ShowToolbarActionPopup(
             extension->id(), grant_active_tab_permissions);
}

void ExtensionActionAPI::NotifyChange(ExtensionAction* extension_action,
                                      content::WebContents* web_contents,
                                      content::BrowserContext* context) {
  for (auto& observer : observers_)
    observer.OnExtensionActionUpdated(extension_action, web_contents, context);

  if (extension_action->action_type() == ActionInfo::TYPE_PAGE)
    NotifyPageActionsChanged(web_contents);
}

void ExtensionActionAPI::DispatchExtensionActionClicked(
    const ExtensionAction& extension_action,
    WebContents* web_contents,
    const Extension* extension) {
  events::HistogramValue histogram_value = events::UNKNOWN;
  const char* event_name = NULL;
  switch (extension_action.action_type()) {
    case ActionInfo::TYPE_BROWSER:
      histogram_value = events::BROWSER_ACTION_ON_CLICKED;
      event_name = "browserAction.onClicked";
      break;
    case ActionInfo::TYPE_PAGE:
      histogram_value = events::PAGE_ACTION_ON_CLICKED;
      event_name = "pageAction.onClicked";
      break;
    case ActionInfo::TYPE_SYSTEM_INDICATOR:
      // The System Indicator handles its own clicks.
      NOTREACHED();
      break;
  }

  if (event_name) {
    std::unique_ptr<base::ListValue> args(new base::ListValue());
    args->Append(ExtensionTabUtil::CreateTabObject(
                     web_contents, ExtensionTabUtil::kScrubTab, extension)
                     ->ToValue());

    DispatchEventToExtension(web_contents->GetBrowserContext(),
                             extension_action.extension_id(), histogram_value,
                             event_name, std::move(args));
  }
}

void ExtensionActionAPI::ClearAllValuesForTab(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  const SessionID tab_id = SessionTabHelper::IdForTab(web_contents);
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();
  ExtensionActionManager* action_manager =
      ExtensionActionManager::Get(browser_context_);

  for (ExtensionSet::const_iterator iter = enabled_extensions.begin();
       iter != enabled_extensions.end(); ++iter) {
    ExtensionAction* extension_action =
        action_manager->GetExtensionAction(**iter);
    if (extension_action) {
      extension_action->ClearAllValuesForTab(tab_id.id());
      NotifyChange(extension_action, web_contents, browser_context);
    }
  }
}

ExtensionPrefs* ExtensionActionAPI::GetExtensionPrefs() {
  // This lazy initialization is more than just an optimization, because it
  // allows tests to associate a new ExtensionPrefs with the browser context
  // before we access it.
  if (!extension_prefs_)
    extension_prefs_ = ExtensionPrefs::Get(browser_context_);
  return extension_prefs_;
}

void ExtensionActionAPI::DispatchEventToExtension(
    content::BrowserContext* context,
    const std::string& extension_id,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  if (!EventRouter::Get(context))
    return;

  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(event_args), context);
  event->user_gesture = EventRouter::USER_GESTURE_ENABLED;
  EventRouter::Get(context)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

void ExtensionActionAPI::NotifyPageActionsChanged(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  for (auto& observer : observers_)
    observer.OnPageActionsUpdated(web_contents);
}

void ExtensionActionAPI::Shutdown() {
  for (auto& observer : observers_)
    observer.OnExtensionActionAPIShuttingDown();
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

ExtensionFunction::ResponseAction ExtensionActionFunction::Run() {
  ExtensionActionManager* manager =
      ExtensionActionManager::Get(browser_context());
  if (base::StartsWith(name(), "systemIndicator.",
                       base::CompareCase::INSENSITIVE_ASCII)) {
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
    return RespondNow(Error(kNoExtensionActionError));
  }

  // Populates the tab_id_ and details_ members.
  EXTENSION_FUNCTION_VALIDATE(ExtractDataFromArguments());

  // Find the WebContents that contains this tab id if one is required.
  if (tab_id_ != ExtensionAction::kDefaultTabId) {
    ExtensionTabUtil::GetTabById(tab_id_, browser_context(),
                                 include_incognito(), nullptr, nullptr,
                                 &contents_, nullptr);
    if (!contents_)
      return RespondNow(Error(kNoTabError, base::IntToString(tab_id_)));
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

  switch (first_arg->type()) {
    case base::Value::Type::INTEGER:
      CHECK(first_arg->GetAsInteger(&tab_id_));
      break;

    case base::Value::Type::DICTIONARY: {
      // Found the details argument.
      details_ = static_cast<base::DictionaryValue*>(first_arg);
      // Still need to check for the tabId within details.
      base::Value* tab_id_value = NULL;
      if (details_->Get("tabId", &tab_id_value)) {
        switch (tab_id_value->type()) {
          case base::Value::Type::NONE:
            // OK; tabId is optional, leave it default.
            return true;
          case base::Value::Type::INTEGER:
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

    case base::Value::Type::NONE:
      // The tabId might be an optional argument.
      break;

    default:
      return false;
  }

  return true;
}

void ExtensionActionFunction::NotifyChange() {
  ExtensionActionAPI::Get(browser_context())
      ->NotifyChange(extension_action_, contents_, browser_context());
}

void ExtensionActionFunction::SetVisible(bool visible) {
  if (extension_action_->GetIsVisible(tab_id_) == visible)
    return;
  extension_action_->SetIsVisible(tab_id_, visible);
  NotifyChange();
}

ExtensionFunction::ResponseAction
ExtensionActionShowFunction::RunExtensionAction() {
  SetVisible(true);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionHideFunction::RunExtensionAction() {
  SetVisible(false);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetIconFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);

  // setIcon can take a variant argument: either a dictionary of canvas
  // ImageData, or an icon index.
  base::DictionaryValue* canvas_set = NULL;
  int icon_index;
  if (details_->GetDictionary("imageData", &canvas_set)) {
    gfx::ImageSkia icon;

    EXTENSION_FUNCTION_VALIDATE(
        ExtensionAction::ParseIconFromCanvasDictionary(*canvas_set, &icon));

    if (icon.isNull())
      return RespondNow(Error("Icon invalid."));

    extension_action_->SetIcon(tab_id_, gfx::Image(icon));
  } else if (details_->GetInteger("iconIndex", &icon_index)) {
    // Obsolete argument: ignore it.
    return RespondNow(NoArguments());
  } else {
    EXTENSION_FUNCTION_VALIDATE(false);
  }
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetTitleFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  std::string title;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("title", &title));
  extension_action_->SetTitle(tab_id_, title);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetPopupFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  std::string popup_string;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("popup", &popup_string));

  GURL popup_url;
  if (!popup_string.empty())
    popup_url = extension()->GetResourceURL(popup_string);

  extension_action_->SetPopupUrl(tab_id_, popup_url);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetBadgeTextFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  std::string badge_text;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("text", &badge_text));
  extension_action_->SetBadgeText(tab_id_, badge_text);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetBadgeBackgroundColorFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  base::Value* color_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(details_->Get("color", &color_value));
  SkColor color = 0;
  if (color_value->is_list()) {
    base::ListValue* list = NULL;
    EXTENSION_FUNCTION_VALIDATE(details_->GetList("color", &list));
    EXTENSION_FUNCTION_VALIDATE(list->GetSize() == 4);

    int color_array[4] = {0};
    for (size_t i = 0; i < arraysize(color_array); ++i) {
      EXTENSION_FUNCTION_VALIDATE(list->GetInteger(i, &color_array[i]));
    }

    color = SkColorSetARGB(color_array[3], color_array[0],
                           color_array[1], color_array[2]);
  } else if (color_value->is_string()) {
    std::string color_string;
    EXTENSION_FUNCTION_VALIDATE(details_->GetString("color", &color_string));
    if (!image_util::ParseCssColorString(color_string, &color))
      return RespondNow(Error(kInvalidColorError));
  }

  extension_action_->SetBadgeBackgroundColor(tab_id_, color);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionGetTitleFunction::RunExtensionAction() {
  return RespondNow(OneArgument(
      std::make_unique<base::Value>(extension_action_->GetTitle(tab_id_))));
}

ExtensionFunction::ResponseAction
ExtensionActionGetPopupFunction::RunExtensionAction() {
  return RespondNow(OneArgument(std::make_unique<base::Value>(
      extension_action_->GetPopupUrl(tab_id_).spec())));
}

ExtensionFunction::ResponseAction
ExtensionActionGetBadgeTextFunction::RunExtensionAction() {
  return RespondNow(OneArgument(
      std::make_unique<base::Value>(extension_action_->GetBadgeText(tab_id_))));
}

ExtensionFunction::ResponseAction
ExtensionActionGetBadgeBackgroundColorFunction::RunExtensionAction() {
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  SkColor color = extension_action_->GetBadgeBackgroundColor(tab_id_);
  list->AppendInteger(static_cast<int>(SkColorGetR(color)));
  list->AppendInteger(static_cast<int>(SkColorGetG(color)));
  list->AppendInteger(static_cast<int>(SkColorGetB(color)));
  list->AppendInteger(static_cast<int>(SkColorGetA(color)));
  return RespondNow(OneArgument(std::move(list)));
}

BrowserActionOpenPopupFunction::BrowserActionOpenPopupFunction()
    : response_sent_(false) {
}

bool BrowserActionOpenPopupFunction::RunAsync() {
  // We only allow the popup in the active window.
  Profile* profile = GetProfile();
  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  // It's possible that the last active browser actually corresponds to the
  // associated incognito profile, and this won't be returned by
  // FindLastActiveWithProfile. If the browser we found isn't active and the
  // extension can operate incognito, then check the last active incognito, too.
  if ((!browser || !browser->window()->IsActive()) &&
      util::IsIncognitoEnabled(extension()->id(), profile) &&
      profile->HasOffTheRecordProfile()) {
    browser =
        chrome::FindLastActiveWithProfile(profile->GetOffTheRecordProfile());
  }

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

  // Even if this is for an incognito window, we want to use the normal profile.
  // If the extension is spanning, then extension hosts are created with the
  // original profile, and if it's split, then we know the api call came from
  // the right profile.
  registrar_.Add(this, NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
                 content::Source<Profile>(profile));

  // Set a timeout for waiting for the notification that the popup is loaded.
  // Waiting is required so that the popup view can be retrieved by the custom
  // bindings for the response callback. It's also needed to keep this function
  // instance around until a notification is observed.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BrowserActionOpenPopupFunction::OpenPopupTimedOut, this),
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
  DCHECK_EQ(NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD, type);
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
