// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browser_event_router.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api_constants.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/extensions/extension_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

#if defined(TOOLKIT_GTK)
#include "ui/base/x/active_window_watcher_x.h"
#endif

namespace events = extension_event_names;
namespace tab_keys = extension_tabs_module_constants;
namespace page_action_keys = extension_page_actions_api_constants;

using content::NavigationController;
using content::WebContents;

ExtensionBrowserEventRouter::TabEntry::TabEntry()
    : complete_waiting_on_load_(false),
      url_() {
}

DictionaryValue* ExtensionBrowserEventRouter::TabEntry::UpdateLoadState(
    const WebContents* contents) {
  // The tab may go in & out of loading (for instance if iframes navigate).
  // We only want to respond to the first change from loading to !loading after
  // the NAV_ENTRY_COMMITTED was fired.
  if (!complete_waiting_on_load_ || contents->IsLoading())
    return NULL;

  // Send "complete" state change.
  complete_waiting_on_load_ = false;
  DictionaryValue* changed_properties = new DictionaryValue();
  changed_properties->SetString(tab_keys::kStatusKey,
      tab_keys::kStatusValueComplete);
  return changed_properties;
}

DictionaryValue* ExtensionBrowserEventRouter::TabEntry::DidNavigate(
    const WebContents* contents) {
  // Send "loading" state change.
  complete_waiting_on_load_ = true;
  DictionaryValue* changed_properties = new DictionaryValue();
  changed_properties->SetString(tab_keys::kStatusKey,
      tab_keys::kStatusValueLoading);

  if (contents->GetURL() != url_) {
    url_ = contents->GetURL();
    changed_properties->SetString(tab_keys::kUrlKey, url_.spec());
  }

  return changed_properties;
}

void ExtensionBrowserEventRouter::Init(ExtensionToolbarModel* model) {
  if (initialized_)
    return;
  model->AddObserver(this);
  BrowserList::AddObserver(this);
#if defined(TOOLKIT_VIEWS)
  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
#elif defined(TOOLKIT_GTK)
  ui::ActiveWindowWatcherX::AddObserver(this);
#elif defined(OS_MACOSX)
  // Needed for when no suitable window can be passed to an extension as the
  // currently focused window.
  registrar_.Add(this, content::NOTIFICATION_NO_KEY_WINDOW,
                 content::NotificationService::AllSources());
#endif

  // Init() can happen after the browser is running, so catch up with any
  // windows that already exist.
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    RegisterForBrowserNotifications(*iter);

    // Also catch up our internal bookkeeping of tab entries.
    Browser* browser = *iter;
    if (browser->tabstrip_model()) {
      for (int i = 0; i < browser->tabstrip_model()->count(); ++i) {
        WebContents* contents =
            browser->GetTabContentsWrapperAt(i)->web_contents();
        int tab_id = ExtensionTabUtil::GetTabId(contents);
        tab_entries_[tab_id] = TabEntry();
      }
    }
  }

  initialized_ = true;
}

ExtensionBrowserEventRouter::ExtensionBrowserEventRouter(Profile* profile)
    : initialized_(false),
      profile_(profile),
      focused_profile_(NULL),
      focused_window_id_(extension_misc::kUnknownWindowId) {
  DCHECK(!profile->IsOffTheRecord());
}

ExtensionBrowserEventRouter::~ExtensionBrowserEventRouter() {
  BrowserList::RemoveObserver(this);
#if defined(TOOLKIT_VIEWS)
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
#elif defined(TOOLKIT_GTK)
  ui::ActiveWindowWatcherX::RemoveObserver(this);
#endif
}

void ExtensionBrowserEventRouter::OnBrowserAdded(const Browser* browser) {
  RegisterForBrowserNotifications(browser);
}

void ExtensionBrowserEventRouter::RegisterForBrowserNotifications(
    const Browser* browser) {
  if (!profile_->IsSameProfile(browser->profile()))
    return;
  // Start listening to TabStripModel events for this browser.
  browser->tabstrip_model()->AddObserver(this);

  // If this is a new window, it isn't ready at this point, so we register to be
  // notified when it is. If this is an existing window, this is a no-op that we
  // just do to reduce code complexity.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      content::Source<const Browser>(browser));

  for (int i = 0; i < browser->tabstrip_model()->count(); ++i) {
    RegisterForTabNotifications(
        browser->GetTabContentsWrapperAt(i)->web_contents());
  }
}

void ExtensionBrowserEventRouter::RegisterForTabNotifications(
    WebContents* contents) {
  registrar_.Add(
      this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&contents->GetController()));

  // Observing NOTIFICATION_WEB_CONTENTS_DESTROYED is necessary because it's
  // possible for tabs to be created, detached and then destroyed without
  // ever having been re-attached and closed. This happens in the case of
  // a devtools WebContents that is opened in window, docked, then closed.
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(contents));
}

void ExtensionBrowserEventRouter::UnregisterForTabNotifications(
    WebContents* contents) {
  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&contents->GetController()));
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<WebContents>(contents));
}

void ExtensionBrowserEventRouter::OnBrowserWindowReady(const Browser* browser) {
  ListValue args;

  DCHECK(browser->extension_window_controller());
  DictionaryValue* window_dictionary =
      browser->extension_window_controller()->CreateWindowValue();
  args.Append(window_dictionary);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  DispatchEvent(browser->profile(), events::kOnWindowCreated, json_args);
}

void ExtensionBrowserEventRouter::OnBrowserRemoved(const Browser* browser) {
  if (!profile_->IsSameProfile(browser->profile()))
    return;

  // Stop listening to TabStripModel events for this browser.
  browser->tabstrip_model()->RemoveObserver(this);

  registrar_.Remove(this, chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      content::Source<const Browser>(browser));

  DispatchSimpleBrowserEvent(browser->profile(),
                             ExtensionTabUtil::GetWindowId(browser),
                             events::kOnWindowRemoved);
}

#if defined(TOOLKIT_VIEWS)
void ExtensionBrowserEventRouter::OnNativeFocusChange(
    gfx::NativeView focused_before,
    gfx::NativeView focused_now) {
  if (!focused_now)
    OnBrowserSetLastActive(NULL);
}
#elif defined(TOOLKIT_GTK)
void ExtensionBrowserEventRouter::ActiveWindowChanged(
    GdkWindow* active_window) {
  if (!active_window)
    OnBrowserSetLastActive(NULL);
}
#endif

void ExtensionBrowserEventRouter::OnBrowserSetLastActive(
    const Browser* browser) {
  Profile* window_profile = NULL;
  int window_id = extension_misc::kUnknownWindowId;
  if (browser && profile_->IsSameProfile(browser->profile())) {
    window_profile = browser->profile();
    window_id = ExtensionTabUtil::GetWindowId(browser);
  }

  if (focused_window_id_ == window_id)
    return;

  // window_profile is either this profile's default profile, its
  // incognito profile, or NULL iff this profile is losing focus.
  Profile* previous_focused_profile = focused_profile_;
  focused_profile_ = window_profile;
  focused_window_id_ = window_id;

  ListValue real_args;
  real_args.Append(Value::CreateIntegerValue(window_id));
  std::string real_json_args;
  base::JSONWriter::Write(&real_args, &real_json_args);

  // When switching between windows in the default and incognitoi profiles,
  // dispatch WINDOW_ID_NONE to extensions whose profile lost focus that
  // can't see the new focused window across the incognito boundary.
  // See crbug.com/46610.
  std::string none_json_args;
  if (focused_profile_ != NULL && previous_focused_profile != NULL &&
      focused_profile_ != previous_focused_profile) {
    ListValue none_args;
    none_args.Append(
        Value::CreateIntegerValue(extension_misc::kUnknownWindowId));
    base::JSONWriter::Write(&none_args, &none_json_args);
  }

  DispatchEventsAcrossIncognito((focused_profile_ ? focused_profile_ :
                                 previous_focused_profile),
                                events::kOnWindowFocusedChanged,
                                real_json_args,
                                none_json_args);
}

void ExtensionBrowserEventRouter::TabCreatedAt(WebContents* contents,
                                               int index,
                                               bool active) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEventWithTab(profile, "", events::kOnTabCreated, contents, active);

  RegisterForTabNotifications(contents);
}

void ExtensionBrowserEventRouter::TabInsertedAt(TabContentsWrapper* contents,
                                                int index,
                                                bool active) {
  // If tab is new, send created event.
  int tab_id = ExtensionTabUtil::GetTabId(contents->web_contents());
  if (!GetTabEntry(contents->web_contents())) {
    tab_entries_[tab_id] = TabEntry();

    TabCreatedAt(contents->web_contents(), index, active);
    return;
  }

  ListValue args;
  args.Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kNewWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents->web_contents())));
  object_args->Set(tab_keys::kNewPositionKey, Value::CreateIntegerValue(
      index));
  args.Append(object_args);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  DispatchEvent(contents->profile(), events::kOnTabAttached, json_args);
}

void ExtensionBrowserEventRouter::TabDetachedAt(TabContentsWrapper* contents,
                                                int index) {
  if (!GetTabEntry(contents->web_contents())) {
    // The tab was removed. Don't send detach event.
    return;
  }

  ListValue args;
  args.Append(Value::CreateIntegerValue(
      ExtensionTabUtil::GetTabId(contents->web_contents())));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kOldWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents->web_contents())));
  object_args->Set(tab_keys::kOldPositionKey, Value::CreateIntegerValue(
      index));
  args.Append(object_args);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  DispatchEvent(contents->profile(), events::kOnTabDetached, json_args);
}

void ExtensionBrowserEventRouter::TabClosingAt(TabStripModel* tab_strip_model,
                                               TabContentsWrapper* contents,
                                               int index) {
  int tab_id = ExtensionTabUtil::GetTabId(contents->web_contents());

  ListValue args;
  args.Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetBoolean(tab_keys::kWindowClosing,
                          tab_strip_model->closing_all());
  args.Append(object_args);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  DispatchEvent(contents->profile(), events::kOnTabRemoved, json_args);

  int removed_count = tab_entries_.erase(tab_id);
  DCHECK_GT(removed_count, 0);

  UnregisterForTabNotifications(contents->web_contents());
}

void ExtensionBrowserEventRouter::ActiveTabChanged(
    TabContentsWrapper* old_contents,
    TabContentsWrapper* new_contents,
    int index,
    bool user_gesture) {
  ListValue args;
  int tab_id = ExtensionTabUtil::GetTabId(new_contents->web_contents());
  args.Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(new_contents->web_contents())));
  args.Append(object_args);

  // The onActivated event replaced onActiveChanged and onSelectionChanged. The
  // deprecated events take two arguments: tabId, {windowId}.
  std::string old_json_args;
  base::JSONWriter::Write(&args, &old_json_args);

  // The onActivated event takes one argument: {windowId, tabId}.
  std::string new_json_args;
  args.Remove(0, NULL);
  object_args->Set(tab_keys::kTabIdKey, Value::CreateIntegerValue(tab_id));
  base::JSONWriter::Write(&args, &new_json_args);

  Profile* profile = new_contents->profile();
  DispatchEvent(profile, events::kOnTabSelectionChanged, old_json_args);
  DispatchEvent(profile, events::kOnTabActiveChanged, old_json_args);
  DispatchEvent(profile, events::kOnTabActivated, new_json_args);
}

void ExtensionBrowserEventRouter::TabSelectionChanged(
    TabStripModel* tab_strip_model,
    const TabStripSelectionModel& old_model) {
  TabStripSelectionModel::SelectedIndices new_selection =
      tab_strip_model->selection_model().selected_indices();
  ListValue* all = new ListValue();

  for (size_t i = 0; i < new_selection.size(); ++i) {
    int index = new_selection[i];
    TabContentsWrapper* contents = tab_strip_model->GetTabContentsAt(index);
    if (!contents)
      break;
    int tab_id = ExtensionTabUtil::GetTabId(contents->web_contents());
    all->Append(Value::CreateIntegerValue(tab_id));
  }

  ListValue args;
  DictionaryValue* select_info = new DictionaryValue();

  select_info->Set(tab_keys::kWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTabStripModel(tab_strip_model)));

  select_info->Set(tab_keys::kTabIdsKey, all);
  args.Append(select_info);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  // The onHighlighted event replaced onHighlightChanged.
  Profile* profile = tab_strip_model->profile();
  DispatchEvent(profile, events::kOnTabHighlightChanged, json_args);
  DispatchEvent(profile, events::kOnTabHighlighted, json_args);
}

void ExtensionBrowserEventRouter::TabMoved(TabContentsWrapper* contents,
                                           int from_index,
                                           int to_index) {
  ListValue args;
  args.Append(Value::CreateIntegerValue(
      ExtensionTabUtil::GetTabId(contents->web_contents())));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents->web_contents())));
  object_args->Set(tab_keys::kFromIndexKey, Value::CreateIntegerValue(
      from_index));
  object_args->Set(tab_keys::kToIndexKey, Value::CreateIntegerValue(
      to_index));
  args.Append(object_args);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  DispatchEvent(contents->profile(), events::kOnTabMoved, json_args);
}

void ExtensionBrowserEventRouter::TabUpdated(WebContents* contents,
                                             bool did_navigate) {
  TabEntry* entry = GetTabEntry(contents);
  DictionaryValue* changed_properties = NULL;

  DCHECK(entry);

  if (did_navigate)
    changed_properties = entry->DidNavigate(contents);
  else
    changed_properties = entry->UpdateLoadState(contents);

  if (changed_properties)
    DispatchTabUpdatedEvent(contents, changed_properties);
}

void ExtensionBrowserEventRouter::DispatchEvent(Profile* profile,
                                                const char* event_name,
                                                const std::string& json_args) {
  if (!profile_->IsSameProfile(profile) || !profile->GetExtensionEventRouter())
    return;

  profile->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name, json_args, profile, GURL());
}

void ExtensionBrowserEventRouter::DispatchEventToExtension(
    Profile* profile,
    const std::string& extension_id,
    const char* event_name,
    const std::string& json_args) {
  if (!profile_->IsSameProfile(profile) || !profile->GetExtensionEventRouter())
    return;

  profile->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id, event_name, json_args, profile, GURL());
}

void ExtensionBrowserEventRouter::DispatchEventsAcrossIncognito(
    Profile* profile,
    const char* event_name,
    const std::string& json_args,
    const std::string& cross_incognito_args) {
  if (!profile_->IsSameProfile(profile) || !profile->GetExtensionEventRouter())
    return;

  profile->GetExtensionEventRouter()->DispatchEventsToRenderersAcrossIncognito(
      event_name, json_args, profile, cross_incognito_args, GURL());
}

void ExtensionBrowserEventRouter::DispatchEventWithTab(
    Profile* profile,
    const std::string& extension_id,
    const char* event_name,
    const WebContents* web_contents,
    bool active) {
  if (!profile_->IsSameProfile(profile))
    return;

  ListValue args;
  args.Append(ExtensionTabUtil::CreateTabValueActive(
      web_contents, active));
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  if (!extension_id.empty()) {
    DispatchEventToExtension(profile, extension_id, event_name, json_args);
  } else {
    DispatchEvent(profile, event_name, json_args);
  }
}

void ExtensionBrowserEventRouter::DispatchSimpleBrowserEvent(
    Profile* profile, const int window_id, const char* event_name) {
  if (!profile_->IsSameProfile(profile))
    return;

  ListValue args;
  args.Append(Value::CreateIntegerValue(window_id));

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  DispatchEvent(profile, event_name, json_args);
}

void ExtensionBrowserEventRouter::DispatchTabUpdatedEvent(
    WebContents* contents, DictionaryValue* changed_properties) {
  DCHECK(changed_properties);
  DCHECK(contents);

  // The state of the tab (as seen from the extension point of view) has
  // changed.  Send a notification to the extension.
  ListValue args;

  // First arg: The id of the tab that changed.
  args.Append(Value::CreateIntegerValue(ExtensionTabUtil::GetTabId(contents)));

  // Second arg: An object containing the changes to the tab state.
  args.Append(changed_properties);

  // Third arg: An object containing the state of the tab.
  args.Append(ExtensionTabUtil::CreateTabValue(contents));

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::kOnTabUpdated, json_args);
}

ExtensionBrowserEventRouter::TabEntry* ExtensionBrowserEventRouter::GetTabEntry(
    const WebContents* contents) {
  int tab_id = ExtensionTabUtil::GetTabId(contents);
  std::map<int, TabEntry>::iterator i = tab_entries_.find(tab_id);
  if (tab_entries_.end() == i)
    return NULL;
  return &i->second;
}

void ExtensionBrowserEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    NavigationController* source_controller =
        content::Source<NavigationController>(source).ptr();
    TabUpdated(source_controller->GetWebContents(), true);
  } else if (type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED) {
    // Tab was destroyed after being detached (without being re-attached).
    WebContents* contents = content::Source<WebContents>(source).ptr();
    registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<NavigationController>(&contents->GetController()));
    registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
        content::Source<WebContents>(contents));
  } else if (type == chrome::NOTIFICATION_BROWSER_WINDOW_READY) {
    const Browser* browser = content::Source<const Browser>(source).ptr();
    OnBrowserWindowReady(browser);
#if defined(OS_MACOSX)
  } else if (type == content::NOTIFICATION_NO_KEY_WINDOW) {
    OnBrowserSetLastActive(NULL);
#endif
  } else {
    NOTREACHED();
  }
}

void ExtensionBrowserEventRouter::TabChangedAt(TabContentsWrapper* contents,
                                               int index,
                                               TabChangeType change_type) {
  TabUpdated(contents->web_contents(), false);
}

void ExtensionBrowserEventRouter::TabReplacedAt(
    TabStripModel* tab_strip_model,
    TabContentsWrapper* old_contents,
    TabContentsWrapper* new_contents,
    int index) {
  TabClosingAt(tab_strip_model, old_contents, index);
  TabInsertedAt(new_contents, index, tab_strip_model->active_index() == index);
}

void ExtensionBrowserEventRouter::TabPinnedStateChanged(
    TabContentsWrapper* contents,
    int index) {
  TabStripModel* tab_strip = NULL;
  int tab_index;

  if (ExtensionTabUtil::GetTabStripModel(
        contents->web_contents(), &tab_strip, &tab_index)) {
    DictionaryValue* changed_properties = new DictionaryValue();
    changed_properties->SetBoolean(tab_keys::kPinnedKey,
                                   tab_strip->IsTabPinned(tab_index));
    DispatchTabUpdatedEvent(contents->web_contents(), changed_properties);
  }
}

void ExtensionBrowserEventRouter::TabStripEmpty() {}

void ExtensionBrowserEventRouter::DispatchOldPageActionEvent(
    Profile* profile,
    const std::string& extension_id,
    const std::string& page_action_id,
    int tab_id,
    const std::string& url,
    int button) {
  ListValue args;
  args.Append(Value::CreateStringValue(page_action_id));

  DictionaryValue* data = new DictionaryValue();
  data->Set(tab_keys::kTabIdKey, Value::CreateIntegerValue(tab_id));
  data->Set(tab_keys::kTabUrlKey, Value::CreateStringValue(url));
  data->Set(page_action_keys::kButtonKey, Value::CreateIntegerValue(button));
  args.Append(data);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  DispatchEventToExtension(profile, extension_id, "pageActions", json_args);
}

void ExtensionBrowserEventRouter::BrowserActionExecuted(
    const std::string& extension_id, Browser* browser) {
  Profile* profile = browser->profile();
  TabContentsWrapper* tab_contents = NULL;
  int tab_id = 0;
  if (!ExtensionTabUtil::GetDefaultTab(browser, &tab_contents, &tab_id))
    return;
  ExtensionActionExecuted(profile, extension_id, tab_contents);
}

void ExtensionBrowserEventRouter::PageActionExecuted(
    Profile* profile,
    const std::string& extension_id,
    const std::string& page_action_id,
    int tab_id,
    const std::string& url,
    int button) {
  DispatchOldPageActionEvent(profile, extension_id, page_action_id, tab_id, url,
                             button);
  TabContentsWrapper* tab_contents = NULL;
  if (!ExtensionTabUtil::GetTabById(tab_id, profile, profile->IsOffTheRecord(),
                                    NULL, NULL, &tab_contents, NULL)) {
    return;
  }
  ExtensionActionExecuted(profile, extension_id, tab_contents);
}

void ExtensionBrowserEventRouter::CommandExecuted(
    Profile* profile,
    const std::string& extension_id,
    const std::string& command) {
  ListValue args;
  args.Append(Value::CreateStringValue(command));
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  DispatchEventToExtension(profile,
                           extension_id,
                           "experimental.keybinding.onCommand",
                           json_args);
}

void ExtensionBrowserEventRouter::ExtensionActionExecuted(
    Profile* profile,
    const std::string& extension_id,
    TabContentsWrapper* tab_contents) {
  const Extension* extension =
      profile->GetExtensionService()->GetExtensionById(extension_id, false);
  if (!extension)
    return;

  const char* event_name = NULL;
  switch (extension->declared_action_type()) {
    case ExtensionAction::TYPE_NONE:
      break;
    case ExtensionAction::TYPE_BROWSER:
      event_name = "browserAction.onClicked";
      break;
    case ExtensionAction::TYPE_PAGE:
      event_name = "pageAction.onClicked";
      break;
  }

  if (event_name) {
    DispatchEventWithTab(profile,
                         extension_id,
                         event_name,
                         tab_contents->web_contents(),
                         true);
  }
}
