// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_event_router.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

namespace events = extensions::event_names;
namespace tab_keys = extensions::tabs_constants;
namespace page_action_keys = extension_page_actions_api_constants;

using content::NavigationController;
using content::WebContents;

namespace extensions {

BrowserEventRouter::TabEntry::TabEntry()
    : complete_waiting_on_load_(false),
      url_() {
}

DictionaryValue* BrowserEventRouter::TabEntry::UpdateLoadState(
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

DictionaryValue* BrowserEventRouter::TabEntry::DidNavigate(
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

BrowserEventRouter::BrowserEventRouter(Profile* profile)
    : profile_(profile) {
  DCHECK(!profile->IsOffTheRecord());

  BrowserList::AddObserver(this);

  // Init() can happen after the browser is running, so catch up with any
  // windows that already exist.
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    RegisterForBrowserNotifications(*iter);

    // Also catch up our internal bookkeeping of tab entries.
    Browser* browser = *iter;
    if (browser->tab_strip_model()) {
      for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
        WebContents* contents = chrome::GetWebContentsAt(browser, i);
        int tab_id = ExtensionTabUtil::GetTabId(contents);
        tab_entries_[tab_id] = TabEntry();
      }
    }
  }
}

BrowserEventRouter::~BrowserEventRouter() {
  BrowserList::RemoveObserver(this);
}

void BrowserEventRouter::OnBrowserAdded(Browser* browser) {
  RegisterForBrowserNotifications(browser);
}

void BrowserEventRouter::RegisterForBrowserNotifications(Browser* browser) {
  if (!profile_->IsSameProfile(browser->profile()))
    return;
  // Start listening to TabStripModel events for this browser.
  browser->tab_strip_model()->AddObserver(this);

  for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
    RegisterForTabNotifications(chrome::GetWebContentsAt(browser, i));
  }
}

void BrowserEventRouter::RegisterForTabNotifications(WebContents* contents) {
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

void BrowserEventRouter::UnregisterForTabNotifications(WebContents* contents) {
  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&contents->GetController()));
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<WebContents>(contents));
}

void BrowserEventRouter::OnBrowserRemoved(Browser* browser) {
  if (!profile_->IsSameProfile(browser->profile()))
    return;

  // Stop listening to TabStripModel events for this browser.
  browser->tab_strip_model()->RemoveObserver(this);
}

void BrowserEventRouter::OnBrowserSetLastActive(Browser* browser) {
  TabsWindowsAPI* tabs_window_api = TabsWindowsAPI::Get(profile_);
  if (tabs_window_api) {
    tabs_window_api->windows_event_router()->OnActiveWindowChanged(
        browser ? browser->extension_window_controller() : NULL);
  }
}

static void WillDispatchTabCreatedEvent(WebContents* contents,
                                        bool active,
                                        Profile* profile,
                                        const Extension* extension,
                                        ListValue* event_args) {
  DictionaryValue* tab_value = ExtensionTabUtil::CreateTabValue(
      contents, extension);
  event_args->Clear();
  event_args->Append(tab_value);
  tab_value->SetBoolean(tab_keys::kSelectedKey, active);
}

void BrowserEventRouter::TabCreatedAt(WebContents* contents,
                                      int index,
                                      bool active) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  scoped_ptr<ListValue> args(new ListValue());
  scoped_ptr<Event> event(new Event(events::kOnTabCreated, args.Pass()));
  event->restrict_to_profile = profile;
  event->user_gesture = EventRouter::USER_GESTURE_NOT_ENABLED;
  event->will_dispatch_callback =
      base::Bind(&WillDispatchTabCreatedEvent, contents, active);
  ExtensionSystem::Get(profile)->event_router()->BroadcastEvent(event.Pass());

  RegisterForTabNotifications(contents);
}

void BrowserEventRouter::TabInsertedAt(WebContents* contents,
                                       int index,
                                       bool active) {
  // If tab is new, send created event.
  int tab_id = ExtensionTabUtil::GetTabId(contents);
  if (!GetTabEntry(contents)) {
    tab_entries_[tab_id] = TabEntry();

    TabCreatedAt(contents, index, active);
    return;
  }

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kNewWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(tab_keys::kNewPositionKey, Value::CreateIntegerValue(
      index));
  args->Append(object_args);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::kOnTabAttached, args.Pass(),
                EventRouter::USER_GESTURE_UNKNOWN);
}

void BrowserEventRouter::TabDetachedAt(WebContents* contents, int index) {
  if (!GetTabEntry(contents)) {
    // The tab was removed. Don't send detach event.
    return;
  }

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateIntegerValue(ExtensionTabUtil::GetTabId(contents)));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kOldWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(tab_keys::kOldPositionKey, Value::CreateIntegerValue(
      index));
  args->Append(object_args);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::kOnTabDetached, args.Pass(),
                EventRouter::USER_GESTURE_UNKNOWN);
}

void BrowserEventRouter::TabClosingAt(TabStripModel* tab_strip_model,
                                      WebContents* contents,
                                      int index) {
  int tab_id = ExtensionTabUtil::GetTabId(contents);

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetInteger(tab_keys::kWindowIdKey,
                          ExtensionTabUtil::GetWindowIdOfTab(contents));
  object_args->SetBoolean(tab_keys::kWindowClosing,
                          tab_strip_model->closing_all());
  args->Append(object_args);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::kOnTabRemoved, args.Pass(),
                EventRouter::USER_GESTURE_UNKNOWN);

  int removed_count = tab_entries_.erase(tab_id);
  DCHECK_GT(removed_count, 0);

  UnregisterForTabNotifications(contents);
}

void BrowserEventRouter::ActiveTabChanged(WebContents* old_contents,
                                          WebContents* new_contents,
                                          int index,
                                          bool user_gesture) {
  scoped_ptr<ListValue> args(new ListValue());
  int tab_id = ExtensionTabUtil::GetTabId(new_contents);
  args->Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(new_contents)));
  args->Append(object_args);

  // The onActivated event replaced onActiveChanged and onSelectionChanged. The
  // deprecated events take two arguments: tabId, {windowId}.
  Profile* profile =
      Profile::FromBrowserContext(new_contents->GetBrowserContext());
  EventRouter::UserGestureState gesture = user_gesture ?
      EventRouter::USER_GESTURE_ENABLED : EventRouter::USER_GESTURE_NOT_ENABLED;
  DispatchEvent(profile, events::kOnTabSelectionChanged,
                scoped_ptr<ListValue>(args->DeepCopy()), gesture);
  DispatchEvent(profile, events::kOnTabActiveChanged,
                scoped_ptr<ListValue>(args->DeepCopy()), gesture);

  // The onActivated event takes one argument: {windowId, tabId}.
  args->Remove(0, NULL);
  object_args->Set(tab_keys::kTabIdKey, Value::CreateIntegerValue(tab_id));
  DispatchEvent(profile, events::kOnTabActivated, args.Pass(), gesture);
}

void BrowserEventRouter::TabSelectionChanged(
    TabStripModel* tab_strip_model,
    const ui::ListSelectionModel& old_model) {
  ui::ListSelectionModel::SelectedIndices new_selection =
      tab_strip_model->selection_model().selected_indices();
  ListValue* all = new ListValue();

  for (size_t i = 0; i < new_selection.size(); ++i) {
    int index = new_selection[i];
    WebContents* contents = tab_strip_model->GetWebContentsAt(index);
    if (!contents)
      break;
    int tab_id = ExtensionTabUtil::GetTabId(contents);
    all->Append(Value::CreateIntegerValue(tab_id));
  }

  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* select_info = new DictionaryValue();

  select_info->Set(tab_keys::kWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTabStripModel(tab_strip_model)));

  select_info->Set(tab_keys::kTabIdsKey, all);
  args->Append(select_info);

  // The onHighlighted event replaced onHighlightChanged.
  Profile* profile = tab_strip_model->profile();
  DispatchEvent(profile, events::kOnTabHighlightChanged,
                scoped_ptr<ListValue>(args->DeepCopy()),
                EventRouter::USER_GESTURE_UNKNOWN);
  DispatchEvent(profile, events::kOnTabHighlighted, args.Pass(),
                EventRouter::USER_GESTURE_UNKNOWN);
}

void BrowserEventRouter::TabMoved(WebContents* contents,
                                  int from_index,
                                  int to_index) {
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateIntegerValue(ExtensionTabUtil::GetTabId(contents)));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(tab_keys::kFromIndexKey, Value::CreateIntegerValue(
      from_index));
  object_args->Set(tab_keys::kToIndexKey, Value::CreateIntegerValue(
      to_index));
  args->Append(object_args);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::kOnTabMoved, args.Pass(),
                EventRouter::USER_GESTURE_UNKNOWN);
}

void BrowserEventRouter::TabUpdated(WebContents* contents, bool did_navigate) {
  TabEntry* entry = GetTabEntry(contents);
  scoped_ptr<DictionaryValue> changed_properties;

  DCHECK(entry);

  if (did_navigate)
    changed_properties.reset(entry->DidNavigate(contents));
  else
    changed_properties.reset(entry->UpdateLoadState(contents));

  if (changed_properties)
    DispatchTabUpdatedEvent(contents, changed_properties.Pass());
}

void BrowserEventRouter::DispatchEvent(
    Profile* profile,
    const char* event_name,
    scoped_ptr<ListValue> args,
    EventRouter::UserGestureState user_gesture) {
  if (!profile_->IsSameProfile(profile) ||
      !extensions::ExtensionSystem::Get(profile)->event_router())
    return;

  scoped_ptr<Event> event(new Event(event_name, args.Pass()));
  event->restrict_to_profile = profile;
  event->user_gesture = user_gesture;
  ExtensionSystem::Get(profile)->event_router()->BroadcastEvent(event.Pass());
}

void BrowserEventRouter::DispatchEventToExtension(
    Profile* profile,
    const std::string& extension_id,
    const char* event_name,
    scoped_ptr<ListValue> event_args,
    EventRouter::UserGestureState user_gesture) {
  if (!profile_->IsSameProfile(profile) ||
      !extensions::ExtensionSystem::Get(profile)->event_router())
    return;

  scoped_ptr<Event> event(new Event(event_name, event_args.Pass()));
  event->restrict_to_profile = profile;
  event->user_gesture = user_gesture;
  ExtensionSystem::Get(profile)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());
}

void BrowserEventRouter::DispatchSimpleBrowserEvent(
    Profile* profile, const int window_id, const char* event_name) {
  if (!profile_->IsSameProfile(profile))
    return;

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateIntegerValue(window_id));

  DispatchEvent(profile, event_name, args.Pass(),
                EventRouter::USER_GESTURE_UNKNOWN);
}

static void WillDispatchTabUpdatedEvent(
    WebContents* contents,
    const DictionaryValue* changed_properties,
    Profile* profile,
    const Extension* extension,
    ListValue* event_args) {
  // Overwrite the second argument with the appropriate properties dictionary,
  // depending on extension permissions.
  DictionaryValue* properties_value = changed_properties->DeepCopy();
  ExtensionTabUtil::ScrubTabValueForExtension(contents, extension,
                                              properties_value);
  event_args->Set(1, properties_value);

  // Overwrite the third arg with our tab value as seen by this extension.
  DictionaryValue* tab_value = ExtensionTabUtil::CreateTabValue(
      contents, extension);
  event_args->Set(2, tab_value);
}

void BrowserEventRouter::DispatchTabUpdatedEvent(
    WebContents* contents, scoped_ptr<DictionaryValue> changed_properties) {
  DCHECK(changed_properties);
  DCHECK(contents);

  // The state of the tab (as seen from the extension point of view) has
  // changed.  Send a notification to the extension.
  scoped_ptr<ListValue> args_base(new ListValue());

  // First arg: The id of the tab that changed.
  args_base->AppendInteger(ExtensionTabUtil::GetTabId(contents));

  // Second arg: An object containing the changes to the tab state.  Filled in
  // by WillDispatchTabUpdatedEvent as a copy of changed_properties, if the
  // extension has the tabs permission.

  // Third arg: An object containing the state of the tab. Filled in by
  // WillDispatchTabUpdatedEvent.
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  scoped_ptr<Event> event(new Event(events::kOnTabUpdated, args_base.Pass()));
  event->restrict_to_profile = profile;
  event->user_gesture = EventRouter::USER_GESTURE_NOT_ENABLED;
  event->will_dispatch_callback =
      base::Bind(&WillDispatchTabUpdatedEvent,
                 contents, changed_properties.get());
  ExtensionSystem::Get(profile)->event_router()->BroadcastEvent(event.Pass());
}

BrowserEventRouter::TabEntry* BrowserEventRouter::GetTabEntry(
    const WebContents* contents) {
  int tab_id = ExtensionTabUtil::GetTabId(contents);
  std::map<int, TabEntry>::iterator i = tab_entries_.find(tab_id);
  if (tab_entries_.end() == i)
    return NULL;
  return &i->second;
}

void BrowserEventRouter::Observe(int type,
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
  } else {
    NOTREACHED();
  }
}

void BrowserEventRouter::TabChangedAt(WebContents* contents,
                                      int index,
                                      TabChangeType change_type) {
  TabUpdated(contents, false);
}

void BrowserEventRouter::TabReplacedAt(TabStripModel* tab_strip_model,
                                       WebContents* old_contents,
                                       WebContents* new_contents,
                                       int index) {
  // Notify listeners that the next tabs closing or being added are due to
  // WebContents being swapped.
  const int new_tab_id = ExtensionTabUtil::GetTabId(new_contents);
  const int old_tab_id = ExtensionTabUtil::GetTabId(old_contents);
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateIntegerValue(new_tab_id));
  args->Append(Value::CreateIntegerValue(old_tab_id));

  DispatchEvent(Profile::FromBrowserContext(new_contents->GetBrowserContext()),
                events::kOnTabReplaced,
                args.Pass(),
                EventRouter::USER_GESTURE_UNKNOWN);

  // Update tab_entries_.
  const int removed_count = tab_entries_.erase(old_tab_id);
  DCHECK_GT(removed_count, 0);
  UnregisterForTabNotifications(old_contents);

  if (!GetTabEntry(new_contents)) {
    tab_entries_[new_tab_id] = TabEntry();
    RegisterForTabNotifications(new_contents);
  }
}

void BrowserEventRouter::TabPinnedStateChanged(WebContents* contents,
                                               int index) {
  TabStripModel* tab_strip = NULL;
  int tab_index;

  if (ExtensionTabUtil::GetTabStripModel(contents, &tab_strip, &tab_index)) {
    scoped_ptr<DictionaryValue> changed_properties(new DictionaryValue());
    changed_properties->SetBoolean(tab_keys::kPinnedKey,
                                   tab_strip->IsTabPinned(tab_index));
    DispatchTabUpdatedEvent(contents, changed_properties.Pass());
  }
}

void BrowserEventRouter::TabStripEmpty() {}

void BrowserEventRouter::DispatchOldPageActionEvent(
    Profile* profile,
    const std::string& extension_id,
    const std::string& page_action_id,
    int tab_id,
    const std::string& url,
    int button) {
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateStringValue(page_action_id));

  DictionaryValue* data = new DictionaryValue();
  data->Set(tab_keys::kTabIdKey, Value::CreateIntegerValue(tab_id));
  data->Set(tab_keys::kTabUrlKey, Value::CreateStringValue(url));
  data->Set(page_action_keys::kButtonKey, Value::CreateIntegerValue(button));
  args->Append(data);

  DispatchEventToExtension(profile, extension_id, "pageActions", args.Pass(),
                           EventRouter::USER_GESTURE_ENABLED);
}

void BrowserEventRouter::BrowserActionExecuted(
    const ExtensionAction& browser_action,
    Browser* browser) {
  Profile* profile = browser->profile();
  WebContents* web_contents = NULL;
  int tab_id = 0;
  if (!ExtensionTabUtil::GetDefaultTab(browser, &web_contents, &tab_id))
    return;
  ExtensionActionExecuted(profile, browser_action, web_contents);
}

void BrowserEventRouter::PageActionExecuted(Profile* profile,
                                            const ExtensionAction& page_action,
                                            int tab_id,
                                            const std::string& url,
                                            int button) {
  DispatchOldPageActionEvent(profile, page_action.extension_id(),
                             page_action.id(), tab_id, url, button);
  WebContents* web_contents = NULL;
  if (!ExtensionTabUtil::GetTabById(tab_id, profile, profile->IsOffTheRecord(),
                                    NULL, NULL, &web_contents, NULL)) {
    return;
  }
  ExtensionActionExecuted(profile, page_action, web_contents);
}

void BrowserEventRouter::ScriptBadgeExecuted(
    Profile* profile,
    const ExtensionAction& script_badge,
    int tab_id) {
  WebContents* web_contents = NULL;
  if (!ExtensionTabUtil::GetTabById(tab_id, profile, profile->IsOffTheRecord(),
                                    NULL, NULL, &web_contents, NULL)) {
    return;
  }
  ExtensionActionExecuted(profile, script_badge, web_contents);
}

void BrowserEventRouter::ExtensionActionExecuted(
    Profile* profile,
    const ExtensionAction& extension_action,
    WebContents* web_contents) {
  const char* event_name = NULL;
  switch (extension_action.action_type()) {
    case Extension::ActionInfo::TYPE_BROWSER:
      event_name = "browserAction.onClicked";
      break;
    case Extension::ActionInfo::TYPE_PAGE:
      event_name = "pageAction.onClicked";
      break;
    case Extension::ActionInfo::TYPE_SCRIPT_BADGE:
      event_name = "scriptBadge.onClicked";
      break;
    case Extension::ActionInfo::TYPE_SYSTEM_INDICATOR:
      // The System Indicator handles its own clicks.
      break;
  }

  if (event_name) {
    scoped_ptr<ListValue> args(new ListValue());
    DictionaryValue* tab_value = ExtensionTabUtil::CreateTabValue(
        web_contents);
    args->Append(tab_value);

    DispatchEventToExtension(profile,
                             extension_action.extension_id(),
                             event_name,
                             args.Pass(),
                             EventRouter::USER_GESTURE_ENABLED);
  }
}

}  // namespace extensions
