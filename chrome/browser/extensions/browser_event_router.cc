// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_event_router.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
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

void BrowserEventRouter::Init() {
  if (initialized_)
    return;
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
        WebContents* contents =
            chrome::GetTabContentsAt(browser, i)->web_contents();
        int tab_id = ExtensionTabUtil::GetTabId(contents);
        tab_entries_[tab_id] = TabEntry();
      }
    }
  }

  initialized_ = true;
}

BrowserEventRouter::BrowserEventRouter(Profile* profile)
    : initialized_(false),
      profile_(profile) {
  DCHECK(!profile->IsOffTheRecord());
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
    RegisterForTabNotifications(
        chrome::GetTabContentsAt(browser, i)->web_contents());
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
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (service) {
    service->window_event_router()->OnActiveWindowChanged(
        browser ? browser->extension_window_controller() : NULL);
  }
}

void BrowserEventRouter::TabCreatedAt(WebContents* contents,
                                      int index,
                                      bool active) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEventWithTab(profile, "", events::kOnTabCreated, contents, active,
                       EventRouter::USER_GESTURE_NOT_ENABLED);

  RegisterForTabNotifications(contents);
}

void BrowserEventRouter::TabInsertedAt(TabContents* contents,
                                       int index,
                                       bool active) {
  // If tab is new, send created event.
  int tab_id = ExtensionTabUtil::GetTabId(contents->web_contents());
  if (!GetTabEntry(contents->web_contents())) {
    tab_entries_[tab_id] = TabEntry();

    TabCreatedAt(contents->web_contents(), index, active);
    return;
  }

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kNewWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents->web_contents())));
  object_args->Set(tab_keys::kNewPositionKey, Value::CreateIntegerValue(
      index));
  args->Append(object_args);

  DispatchEvent(contents->profile(), events::kOnTabAttached, args.Pass(),
                EventRouter::USER_GESTURE_UNKNOWN);
}

void BrowserEventRouter::TabDetachedAt(TabContents* contents, int index) {
  if (!GetTabEntry(contents->web_contents())) {
    // The tab was removed. Don't send detach event.
    return;
  }

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateIntegerValue(
      ExtensionTabUtil::GetTabId(contents->web_contents())));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kOldWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents->web_contents())));
  object_args->Set(tab_keys::kOldPositionKey, Value::CreateIntegerValue(
      index));
  args->Append(object_args);

  DispatchEvent(contents->profile(), events::kOnTabDetached, args.Pass(),
                EventRouter::USER_GESTURE_UNKNOWN);
}

void BrowserEventRouter::TabClosingAt(TabStripModel* tab_strip_model,
                                      TabContents* contents,
                                      int index) {
  int tab_id = ExtensionTabUtil::GetTabId(contents->web_contents());

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetBoolean(tab_keys::kWindowClosing,
                          tab_strip_model->closing_all());
  args->Append(object_args);

  DispatchEvent(contents->profile(), events::kOnTabRemoved, args.Pass(),
                EventRouter::USER_GESTURE_UNKNOWN);

  int removed_count = tab_entries_.erase(tab_id);
  DCHECK_GT(removed_count, 0);

  UnregisterForTabNotifications(contents->web_contents());
}

void BrowserEventRouter::ActiveTabChanged(TabContents* old_contents,
                                          TabContents* new_contents,
                                          int index,
                                          bool user_gesture) {
  scoped_ptr<ListValue> args(new ListValue());
  int tab_id = ExtensionTabUtil::GetTabId(new_contents->web_contents());
  args->Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(new_contents->web_contents())));
  args->Append(object_args);

  // The onActivated event replaced onActiveChanged and onSelectionChanged. The
  // deprecated events take two arguments: tabId, {windowId}.
  Profile* profile = new_contents->profile();
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
    const TabStripSelectionModel& old_model) {
  TabStripSelectionModel::SelectedIndices new_selection =
      tab_strip_model->selection_model().selected_indices();
  ListValue* all = new ListValue();

  for (size_t i = 0; i < new_selection.size(); ++i) {
    int index = new_selection[i];
    TabContents* contents = tab_strip_model->GetTabContentsAt(index);
    if (!contents)
      break;
    int tab_id = ExtensionTabUtil::GetTabId(contents->web_contents());
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

void BrowserEventRouter::TabMoved(TabContents* contents,
                                  int from_index,
                                  int to_index) {
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateIntegerValue(
      ExtensionTabUtil::GetTabId(contents->web_contents())));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents->web_contents())));
  object_args->Set(tab_keys::kFromIndexKey, Value::CreateIntegerValue(
      from_index));
  object_args->Set(tab_keys::kToIndexKey, Value::CreateIntegerValue(
      to_index));
  args->Append(object_args);

  DispatchEvent(contents->profile(), events::kOnTabMoved, args.Pass(),
                EventRouter::USER_GESTURE_UNKNOWN);
}

void BrowserEventRouter::TabUpdated(WebContents* contents, bool did_navigate) {
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

void BrowserEventRouter::DispatchEvent(
    Profile* profile,
    const char* event_name,
    scoped_ptr<ListValue> args,
    EventRouter::UserGestureState user_gesture) {
  if (!profile_->IsSameProfile(profile) || !profile->GetExtensionEventRouter())
    return;

  profile->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name, args.Pass(), profile, GURL(), user_gesture);
}

void BrowserEventRouter::DispatchEventToExtension(
    Profile* profile,
    const std::string& extension_id,
    const char* event_name,
    scoped_ptr<ListValue> event_args,
    EventRouter::UserGestureState user_gesture) {
  if (!profile_->IsSameProfile(profile) || !profile->GetExtensionEventRouter())
    return;

  profile->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id, event_name, event_args.Pass(), profile, GURL(),
      user_gesture);
}

void BrowserEventRouter::DispatchEventsAcrossIncognito(
    Profile* profile,
    const char* event_name,
    scoped_ptr<ListValue> event_args,
    scoped_ptr<ListValue> cross_incognito_args) {
  if (!profile_->IsSameProfile(profile) || !profile->GetExtensionEventRouter())
    return;

  profile->GetExtensionEventRouter()->DispatchEventsToRenderersAcrossIncognito(
      event_name, event_args.Pass(), profile, cross_incognito_args.Pass(),
      GURL());
}

void BrowserEventRouter::DispatchEventWithTab(
    Profile* profile,
    const std::string& extension_id,
    const char* event_name,
    const WebContents* web_contents,
    bool active,
    EventRouter::UserGestureState user_gesture,
    scoped_ptr<ListValue> event_args) {
  if (!profile_->IsSameProfile(profile))
    return;

  if (!extension_id.empty()) {
    event_args->Append(ExtensionTabUtil::CreateTabValueActive(
        web_contents,
        active,
        profile->GetExtensionService()->extensions()->GetByID(extension_id)));
    DispatchEventToExtension(profile, extension_id, event_name,
                             event_args.Pass(), user_gesture);
  } else {
    const EventListenerMap::ListenerList& listeners(
        ExtensionSystem::Get(profile)->event_router()->
        listeners().GetEventListenersByName(event_name));

    for (EventListenerMap::ListenerList::const_iterator it = listeners.begin();
         it != listeners.end();
         ++it) {
      scoped_ptr<ListValue> args(event_args->DeepCopy());
      args->Append(ExtensionTabUtil::CreateTabValueActive(
          web_contents,
          active,
          profile->GetExtensionService()->extensions()->GetByID(
              (*it)->extension_id)));
      DispatchEventToExtension(profile, (*it)->extension_id, event_name,
                               args.Pass(), user_gesture);
    }
  }
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

void BrowserEventRouter::DispatchTabUpdatedEvent(
    WebContents* contents, DictionaryValue* changed_properties) {
  DCHECK(changed_properties);
  DCHECK(contents);

  // The state of the tab (as seen from the extension point of view) has
  // changed.  Send a notification to the extension.
  scoped_ptr<ListValue> args(new ListValue());

  // First arg: The id of the tab that changed.
  args->Append(Value::CreateIntegerValue(ExtensionTabUtil::GetTabId(contents)));

  // Second arg: An object containing the changes to the tab state.
  args->Append(changed_properties);

  // Third arg: An object containing the state of the tab.
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  DispatchEventWithTab(profile, "", events::kOnTabUpdated, contents, true,
                       EventRouter::USER_GESTURE_UNKNOWN, args.Pass());
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

void BrowserEventRouter::TabChangedAt(TabContents* contents,
                                      int index,
                                      TabChangeType change_type) {
  TabUpdated(contents->web_contents(), false);
}

void BrowserEventRouter::TabReplacedAt(TabStripModel* tab_strip_model,
                                       TabContents* old_contents,
                                       TabContents* new_contents,
                                       int index) {
  TabClosingAt(tab_strip_model, old_contents, index);
  TabInsertedAt(new_contents, index, tab_strip_model->active_index() == index);
}

void BrowserEventRouter::TabPinnedStateChanged(TabContents* contents,
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
  TabContents* tab_contents = NULL;
  int tab_id = 0;
  if (!ExtensionTabUtil::GetDefaultTab(browser, &tab_contents, &tab_id))
    return;
  ExtensionActionExecuted(profile, browser_action, tab_contents);
}

void BrowserEventRouter::PageActionExecuted(Profile* profile,
                                            const ExtensionAction& page_action,
                                            int tab_id,
                                            const std::string& url,
                                            int button) {
  DispatchOldPageActionEvent(profile, page_action.extension_id(),
                             page_action.id(), tab_id, url, button);
  TabContents* tab_contents = NULL;
  if (!ExtensionTabUtil::GetTabById(tab_id, profile, profile->IsOffTheRecord(),
                                    NULL, NULL, &tab_contents, NULL)) {
    return;
  }
  ExtensionActionExecuted(profile, page_action, tab_contents);
}

void BrowserEventRouter::ScriptBadgeExecuted(
    Profile* profile,
    const ExtensionAction& script_badge,
    int tab_id) {
  TabContents* tab_contents = NULL;
  if (!ExtensionTabUtil::GetTabById(tab_id, profile, profile->IsOffTheRecord(),
                                    NULL, NULL, &tab_contents, NULL)) {
    return;
  }
  ExtensionActionExecuted(profile, script_badge, tab_contents);
}

void BrowserEventRouter::CommandExecuted(Profile* profile,
                                         const std::string& extension_id,
                                         const std::string& command) {
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateStringValue(command));

  DispatchEventToExtension(profile,
                           extension_id,
                           "commands.onCommand",
                           args.Pass(),
                           EventRouter::USER_GESTURE_ENABLED);
}

void BrowserEventRouter::ExtensionActionExecuted(
    Profile* profile,
    const ExtensionAction& extension_action,
    TabContents* tab_contents) {
  const char* event_name = NULL;
  switch (extension_action.action_type()) {
    case ExtensionAction::TYPE_BROWSER:
      event_name = "browserAction.onClicked";
      break;
    case ExtensionAction::TYPE_PAGE:
      event_name = "pageAction.onClicked";
      break;
    case ExtensionAction::TYPE_SCRIPT_BADGE:
      event_name = "scriptBadge.onClicked";
      break;
  }

  if (event_name) {
    DispatchEventWithTab(profile,
                         extension_action.extension_id(),
                         event_name,
                         tab_contents->web_contents(),
                         true,
                         EventRouter::USER_GESTURE_ENABLED);
  }
}

}  // namespace extensions
