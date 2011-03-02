// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browser_event_router.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_page_actions_module_constants.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace events = extension_event_names;
namespace tab_keys = extension_tabs_module_constants;
namespace page_action_keys = extension_page_actions_module_constants;

ExtensionBrowserEventRouter::TabEntry::TabEntry()
    : complete_waiting_on_load_(false),
      url_() {
}

DictionaryValue* ExtensionBrowserEventRouter::TabEntry::UpdateLoadState(
    const TabContents* contents) {
  // The tab may go in & out of loading (for instance if iframes navigate).
  // We only want to respond to the first change from loading to !loading after
  // the NAV_ENTRY_COMMITTED was fired.
  if (!complete_waiting_on_load_ || contents->is_loading())
    return NULL;

  // Send "complete" state change.
  complete_waiting_on_load_ = false;
  DictionaryValue* changed_properties = new DictionaryValue();
  changed_properties->SetString(tab_keys::kStatusKey,
      tab_keys::kStatusValueComplete);
  return changed_properties;
}

DictionaryValue* ExtensionBrowserEventRouter::TabEntry::DidNavigate(
    const TabContents* contents) {
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

static void DispatchEvent(Profile* profile,
                          const char* event_name,
                          const std::string& json_args) {
  if (profile->GetExtensionEventRouter()) {
    profile->GetExtensionEventRouter()->DispatchEventToRenderers(
        event_name, json_args, profile, GURL());
  }
}

static void DispatchEventToExtension(Profile* profile,
                                     const std::string& extension_id,
                                     const char* event_name,
                                     const std::string& json_args) {
  if (profile->GetExtensionEventRouter()) {
    profile->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id, event_name, json_args, profile, GURL());
  }
}

static void DispatchEventWithTab(Profile* profile,
                                 const std::string& extension_id,
                                 const char* event_name,
                                 const TabContents* tab_contents) {
  ListValue args;
  args.Append(ExtensionTabUtil::CreateTabValue(tab_contents));
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  if (!extension_id.empty()) {
    DispatchEventToExtension(profile, extension_id, event_name, json_args);
  } else {
    DispatchEvent(profile, event_name, json_args);
  }
}

static void DispatchSimpleBrowserEvent(Profile* profile,
                                       const int window_id,
                                       const char* event_name) {
  ListValue args;
  args.Append(Value::CreateIntegerValue(window_id));

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(profile, event_name, json_args);
}

void ExtensionBrowserEventRouter::Init() {
  if (initialized_)
    return;
  BrowserList::AddObserver(this);
#if defined(TOOLKIT_VIEWS)
  views::FocusManager::GetWidgetFocusManager()->AddFocusChangeListener(this);
#elif defined(TOOLKIT_GTK)
  ui::ActiveWindowWatcherX::AddObserver(this);
#elif defined(OS_MACOSX)
  // Needed for when no suitable window can be passed to an extension as the
  // currently focused window.
  registrar_.Add(this, NotificationType::NO_KEY_WINDOW,
                 NotificationService::AllSources());
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
        TabContents* contents = browser->GetTabContentsAt(i);
        int tab_id = ExtensionTabUtil::GetTabId(contents);
        tab_entries_[tab_id] = TabEntry();
      }
    }
  }

  initialized_ = true;
}

ExtensionBrowserEventRouter::ExtensionBrowserEventRouter(Profile* profile)
    : initialized_(false),
      focused_window_id_(extension_misc::kUnknownWindowId),
      profile_(profile) {
  DCHECK(!profile->IsOffTheRecord());
}

ExtensionBrowserEventRouter::~ExtensionBrowserEventRouter() {
  BrowserList::RemoveObserver(this);
#if defined(TOOLKIT_VIEWS)
  views::FocusManager::GetWidgetFocusManager()->RemoveFocusChangeListener(this);
#elif defined(TOOLKIT_GTK)
  ui::ActiveWindowWatcherX::RemoveObserver(this);
#endif
}

void ExtensionBrowserEventRouter::OnBrowserAdded(const Browser* browser) {
  RegisterForBrowserNotifications(browser);
}

void ExtensionBrowserEventRouter::RegisterForBrowserNotifications(
    const Browser* browser) {
  // Start listening to TabStripModel events for this browser.
  browser->tabstrip_model()->AddObserver(this);

  // If this is a new window, it isn't ready at this point, so we register to be
  // notified when it is. If this is an existing window, this is a no-op that we
  // just do to reduce code complexity.
  registrar_.Add(this, NotificationType::BROWSER_WINDOW_READY,
      Source<const Browser>(browser));

  if (browser->tabstrip_model()) {
    for (int i = 0; i < browser->tabstrip_model()->count(); ++i)
      RegisterForTabNotifications(browser->GetTabContentsAt(i));
  }
}

void ExtensionBrowserEventRouter::RegisterForTabNotifications(
    TabContents* contents) {
  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(&contents->controller()));

  // Observing TAB_CONTENTS_DESTROYED is necessary because it's
  // possible for tabs to be created, detached and then destroyed without
  // ever having been re-attached and closed. This happens in the case of
  // a devtools TabContents that is opened in window, docked, then closed.
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(contents));
}

void ExtensionBrowserEventRouter::UnregisterForTabNotifications(
    TabContents* contents) {
  registrar_.Remove(this, NotificationType::NAV_ENTRY_COMMITTED,
      Source<NavigationController>(&contents->controller()));
  registrar_.Remove(this, NotificationType::TAB_CONTENTS_DESTROYED,
      Source<TabContents>(contents));
}

void ExtensionBrowserEventRouter::OnBrowserWindowReady(const Browser* browser) {
  ListValue args;

  DictionaryValue* window_dictionary = ExtensionTabUtil::CreateWindowValue(
      browser, false);
  args.Append(window_dictionary);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(browser->profile(), events::kOnWindowCreated, json_args);
}

void ExtensionBrowserEventRouter::OnBrowserRemoved(const Browser* browser) {
  // Stop listening to TabStripModel events for this browser.
  browser->tabstrip_model()->RemoveObserver(this);

  registrar_.Remove(this, NotificationType::BROWSER_WINDOW_READY,
      Source<const Browser>(browser));

  DispatchSimpleBrowserEvent(browser->profile(),
                             ExtensionTabUtil::GetWindowId(browser),
                             events::kOnWindowRemoved);
}

#if defined(TOOLKIT_VIEWS)
void ExtensionBrowserEventRouter::NativeFocusWillChange(
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
  int window_id = extension_misc::kUnknownWindowId;
  if (browser)
    window_id = ExtensionTabUtil::GetWindowId(browser);

  if (focused_window_id_ == window_id)
    return;

  focused_window_id_ = window_id;
  // Note: because we use the default profile when |browser| is NULL, it means
  // that all extensions hear about the event regardless of whether the browser
  // that lost focus was OTR or if the extension is OTR-enabled.
  // See crbug.com/46610.
  DispatchSimpleBrowserEvent(browser ? browser->profile() : profile_,
                             focused_window_id_,
                             events::kOnWindowFocusedChanged);
}

void ExtensionBrowserEventRouter::TabCreatedAt(TabContents* contents,
                                               int index,
                                               bool foreground) {
  DispatchEventWithTab(contents->profile(), "", events::kOnTabCreated,
                       contents);

  RegisterForTabNotifications(contents);
}

void ExtensionBrowserEventRouter::TabInsertedAt(TabContentsWrapper* contents,
                                                int index,
                                                bool foreground) {
  // If tab is new, send created event.
  int tab_id = ExtensionTabUtil::GetTabId(contents->tab_contents());
  if (!GetTabEntry(contents->tab_contents())) {
    tab_entries_[tab_id] = TabEntry();

    TabCreatedAt(contents->tab_contents(), index, foreground);
    return;
  }

  ListValue args;
  args.Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kNewWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents->tab_contents())));
  object_args->Set(tab_keys::kNewPositionKey, Value::CreateIntegerValue(
      index));
  args.Append(object_args);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(contents->profile(), events::kOnTabAttached, json_args);
}

void ExtensionBrowserEventRouter::TabDetachedAt(TabContentsWrapper* contents,
                                                int index) {
  if (!GetTabEntry(contents->tab_contents())) {
    // The tab was removed. Don't send detach event.
    return;
  }

  ListValue args;
  args.Append(Value::CreateIntegerValue(
      ExtensionTabUtil::GetTabId(contents->tab_contents())));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kOldWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents->tab_contents())));
  object_args->Set(tab_keys::kOldPositionKey, Value::CreateIntegerValue(
      index));
  args.Append(object_args);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(contents->profile(), events::kOnTabDetached, json_args);
}

void ExtensionBrowserEventRouter::TabClosingAt(TabStripModel* tab_strip_model,
                                               TabContentsWrapper* contents,
                                               int index) {
  int tab_id = ExtensionTabUtil::GetTabId(contents->tab_contents());

  ListValue args;
  args.Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetBoolean(tab_keys::kWindowClosing,
                          tab_strip_model->closing_all());
  args.Append(object_args);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(contents->profile(), events::kOnTabRemoved, json_args);

  int removed_count = tab_entries_.erase(tab_id);
  DCHECK_GT(removed_count, 0);

  UnregisterForTabNotifications(contents->tab_contents());
}

void ExtensionBrowserEventRouter::TabSelectedAt(
    TabContentsWrapper* old_contents,
    TabContentsWrapper* new_contents,
    int index,
    bool user_gesture) {
  if (old_contents == new_contents)
    return;

  ListValue args;
  args.Append(Value::CreateIntegerValue(
      ExtensionTabUtil::GetTabId(new_contents->tab_contents())));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(new_contents->tab_contents())));
  args.Append(object_args);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(new_contents->profile(), events::kOnTabSelectionChanged,
                json_args);
}

void ExtensionBrowserEventRouter::TabMoved(TabContentsWrapper* contents,
                                           int from_index,
                                           int to_index) {
  ListValue args;
  args.Append(Value::CreateIntegerValue(
      ExtensionTabUtil::GetTabId(contents->tab_contents())));

  DictionaryValue* object_args = new DictionaryValue();
  object_args->Set(tab_keys::kWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents->tab_contents())));
  object_args->Set(tab_keys::kFromIndexKey, Value::CreateIntegerValue(
      from_index));
  object_args->Set(tab_keys::kToIndexKey, Value::CreateIntegerValue(
      to_index));
  args.Append(object_args);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(contents->profile(), events::kOnTabMoved, json_args);
}

void ExtensionBrowserEventRouter::TabUpdated(TabContents* contents,
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

void ExtensionBrowserEventRouter::DispatchTabUpdatedEvent(
    TabContents* contents, DictionaryValue* changed_properties) {
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
  base::JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(contents->profile(), events::kOnTabUpdated, json_args);
}

ExtensionBrowserEventRouter::TabEntry* ExtensionBrowserEventRouter::GetTabEntry(
    const TabContents* contents) {
  int tab_id = ExtensionTabUtil::GetTabId(contents);
  std::map<int, TabEntry>::iterator i = tab_entries_.find(tab_id);
  if (tab_entries_.end() == i)
    return NULL;
  return &i->second;
}

void ExtensionBrowserEventRouter::Observe(NotificationType type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  if (type == NotificationType::NAV_ENTRY_COMMITTED) {
    NavigationController* source_controller =
        Source<NavigationController>(source).ptr();
    TabUpdated(source_controller->tab_contents(), true);
  } else if (type == NotificationType::TAB_CONTENTS_DESTROYED) {
    // Tab was destroyed after being detached (without being re-attached).
    TabContents* contents = Source<TabContents>(source).ptr();
    registrar_.Remove(this, NotificationType::NAV_ENTRY_COMMITTED,
        Source<NavigationController>(&contents->controller()));
    registrar_.Remove(this, NotificationType::TAB_CONTENTS_DESTROYED,
        Source<TabContents>(contents));
  } else if (type == NotificationType::BROWSER_WINDOW_READY) {
    const Browser* browser = Source<const Browser>(source).ptr();
    OnBrowserWindowReady(browser);
#if defined(OS_MACOSX)
  } else if (type == NotificationType::NO_KEY_WINDOW) {
    OnBrowserSetLastActive(NULL);
#endif
  } else {
    NOTREACHED();
  }
}

void ExtensionBrowserEventRouter::TabChangedAt(TabContentsWrapper* contents,
                                               int index,
                                               TabChangeType change_type) {
  TabUpdated(contents->tab_contents(), false);
}

void ExtensionBrowserEventRouter::TabReplacedAt(
    TabStripModel* tab_strip_model,
    TabContentsWrapper* old_contents,
    TabContentsWrapper* new_contents,
    int index) {
  TabClosingAt(tab_strip_model, old_contents, index);
  TabInsertedAt(new_contents, index,
                tab_strip_model->selected_index() == index);
}

void ExtensionBrowserEventRouter::TabPinnedStateChanged(
    TabContentsWrapper* contents,
    int index) {
  TabStripModel* tab_strip = NULL;
  int tab_index;

  if (ExtensionTabUtil::GetTabStripModel(
        contents->tab_contents(), &tab_strip, &tab_index)) {
    DictionaryValue* changed_properties = new DictionaryValue();
    changed_properties->SetBoolean(tab_keys::kPinnedKey,
                                   tab_strip->IsTabPinned(tab_index));
    DispatchTabUpdatedEvent(contents->tab_contents(), changed_properties);
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
  base::JSONWriter::Write(&args, false, &json_args);

  DispatchEventToExtension(profile, extension_id, "pageActions", json_args);
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
  DispatchEventWithTab(profile, extension_id, "pageAction.onClicked",
                       tab_contents->tab_contents());
}

void ExtensionBrowserEventRouter::BrowserActionExecuted(
    Profile* profile, const std::string& extension_id, Browser* browser) {
  TabContentsWrapper* tab_contents = NULL;
  int tab_id = 0;
  if (!ExtensionTabUtil::GetDefaultTab(browser, &tab_contents, &tab_id))
    return;
  DispatchEventWithTab(profile, extension_id, "browserAction.onClicked",
                       tab_contents->tab_contents());
}
