// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

using base::DictionaryValue;
using base::ListValue;
using base::FundamentalValue;
using content::NavigationController;
using content::WebContents;
using ui_zoom::ZoomController;

namespace extensions {

namespace {

namespace tabs = api::tabs;

bool WillDispatchTabUpdatedEvent(
    WebContents* contents,
    const base::DictionaryValue* changed_properties,
    content::BrowserContext* context,
    const Extension* extension,
    Event* event,
    const base::DictionaryValue* listener_filter) {
  // Overwrite the second argument with the appropriate properties dictionary,
  // depending on extension permissions.
  base::DictionaryValue* properties_value = changed_properties->DeepCopy();
  ExtensionTabUtil::ScrubTabValueForExtension(contents,
                                              extension,
                                              properties_value);
  event->event_args->Set(1, properties_value);

  // Overwrite the third arg with our tab value as seen by this extension.
  event->event_args->Set(2,
                         ExtensionTabUtil::CreateTabValue(contents, extension));
  return true;
}

}  // namespace

TabsEventRouter::TabEntry::TabEntry(content::WebContents* contents)
    : contents_(contents),
      complete_waiting_on_load_(false),
      was_audible_(contents->WasRecentlyAudible()),
      was_muted_(contents->IsAudioMuted()) {
}

scoped_ptr<base::DictionaryValue> TabsEventRouter::TabEntry::UpdateLoadState() {
  // The tab may go in & out of loading (for instance if iframes navigate).
  // We only want to respond to the first change from loading to !loading after
  // the NAV_ENTRY_COMMITTED was fired.
  scoped_ptr<base::DictionaryValue> changed_properties(
      new base::DictionaryValue());
  if (!complete_waiting_on_load_ || contents_->IsLoading()) {
    return changed_properties.Pass();
  }

  // Send "complete" state change.
  complete_waiting_on_load_ = false;
  changed_properties->SetString(tabs_constants::kStatusKey,
                                tabs_constants::kStatusValueComplete);
  return changed_properties.Pass();
}

scoped_ptr<base::DictionaryValue> TabsEventRouter::TabEntry::DidNavigate() {
  // Send "loading" state change.
  complete_waiting_on_load_ = true;
  scoped_ptr<base::DictionaryValue> changed_properties(
      new base::DictionaryValue());
  changed_properties->SetString(tabs_constants::kStatusKey,
                                tabs_constants::kStatusValueLoading);

  if (contents_->GetURL() != url_) {
    url_ = contents_->GetURL();
    changed_properties->SetString(tabs_constants::kUrlKey, url_.spec());
  }

  return changed_properties.Pass();
}

bool TabsEventRouter::TabEntry::SetAudible(bool new_val) {
  if (was_audible_ == new_val)
    return false;
  was_audible_ = new_val;
  return true;
}

bool TabsEventRouter::TabEntry::SetMuted(bool new_val) {
  if (was_muted_ == new_val)
    return false;
  was_muted_ = new_val;
  return true;
}

TabsEventRouter::TabsEventRouter(Profile* profile)
    : profile_(profile), favicon_scoped_observer_(this) {
  DCHECK(!profile->IsOffTheRecord());

  BrowserList::AddObserver(this);

  // Init() can happen after the browser is running, so catch up with any
  // windows that already exist.
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    RegisterForBrowserNotifications(*it);

    // Also catch up our internal bookkeeping of tab entries.
    Browser* browser = *it;
    if (ExtensionTabUtil::BrowserSupportsTabs(browser)) {
      for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
        WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(i);
        int tab_id = ExtensionTabUtil::GetTabId(contents);
        tab_entries_[tab_id] = make_linked_ptr(new TabEntry(contents));
      }
    }
  }
}

TabsEventRouter::~TabsEventRouter() {
  BrowserList::RemoveObserver(this);
}

void TabsEventRouter::OnBrowserAdded(Browser* browser) {
  RegisterForBrowserNotifications(browser);
}

void TabsEventRouter::RegisterForBrowserNotifications(Browser* browser) {
  if (!profile_->IsSameProfile(browser->profile()) ||
      !ExtensionTabUtil::BrowserSupportsTabs(browser))
    return;
  // Start listening to TabStripModel events for this browser.
  TabStripModel* tab_strip = browser->tab_strip_model();
  tab_strip->AddObserver(this);

  for (int i = 0; i < tab_strip->count(); ++i) {
    RegisterForTabNotifications(tab_strip->GetWebContentsAt(i));
  }
}

void TabsEventRouter::RegisterForTabNotifications(WebContents* contents) {
  registrar_.Add(
      this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&contents->GetController()));

  // Observing NOTIFICATION_WEB_CONTENTS_DESTROYED is necessary because it's
  // possible for tabs to be created, detached and then destroyed without
  // ever having been re-attached and closed. This happens in the case of
  // a devtools WebContents that is opened in window, docked, then closed.
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(contents));

  favicon_scoped_observer_.Add(
      favicon::ContentFaviconDriver::FromWebContents(contents));

  ZoomController::FromWebContents(contents)->AddObserver(this);
}

void TabsEventRouter::UnregisterForTabNotifications(WebContents* contents) {
  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&contents->GetController()));
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<WebContents>(contents));
  favicon_scoped_observer_.Remove(
      favicon::ContentFaviconDriver::FromWebContents(contents));

  ZoomController::FromWebContents(contents)->RemoveObserver(this);
}

void TabsEventRouter::OnBrowserRemoved(Browser* browser) {
  if (!profile_->IsSameProfile(browser->profile()))
    return;

  // Stop listening to TabStripModel events for this browser.
  browser->tab_strip_model()->RemoveObserver(this);
}

void TabsEventRouter::OnBrowserSetLastActive(Browser* browser) {
  TabsWindowsAPI* tabs_window_api = TabsWindowsAPI::Get(profile_);
  if (tabs_window_api) {
    tabs_window_api->windows_event_router()->OnActiveWindowChanged(
        browser ? browser->extension_window_controller() : NULL);
  }
}

static bool WillDispatchTabCreatedEvent(
    WebContents* contents,
    bool active,
    content::BrowserContext* context,
    const Extension* extension,
    Event* event,
    const base::DictionaryValue* listener_filter) {
  base::DictionaryValue* tab_value = ExtensionTabUtil::CreateTabValue(
      contents, extension);
  event->event_args->Clear();
  event->event_args->Append(tab_value);
  tab_value->SetBoolean(tabs_constants::kSelectedKey, active);
  return true;
}

void TabsEventRouter::TabCreatedAt(WebContents* contents,
                                   int index,
                                   bool active) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  scoped_ptr<base::ListValue> args(new base::ListValue);
  scoped_ptr<Event> event(new Event(events::TABS_ON_CREATED,
                                    tabs::OnCreated::kEventName, args.Pass()));
  event->restrict_to_browser_context = profile;
  event->user_gesture = EventRouter::USER_GESTURE_NOT_ENABLED;
  event->will_dispatch_callback =
      base::Bind(&WillDispatchTabCreatedEvent, contents, active);
  EventRouter::Get(profile)->BroadcastEvent(event.Pass());

  RegisterForTabNotifications(contents);
}

void TabsEventRouter::TabInsertedAt(WebContents* contents,
                                    int index,
                                    bool active) {
  // If tab is new, send created event.
  int tab_id = ExtensionTabUtil::GetTabId(contents);
  if (GetTabEntry(contents).get() == NULL) {
    tab_entries_[tab_id] = make_linked_ptr(new TabEntry(contents));

    TabCreatedAt(contents, index, active);
    return;
  }

  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(new FundamentalValue(tab_id));

  base::DictionaryValue* object_args = new base::DictionaryValue();
  object_args->Set(tabs_constants::kNewWindowIdKey,
                   new FundamentalValue(
                       ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(tabs_constants::kNewPositionKey,
                   new FundamentalValue(index));
  args->Append(object_args);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_ATTACHED, tabs::OnAttached::kEventName,
                args.Pass(), EventRouter::USER_GESTURE_UNKNOWN);
}

void TabsEventRouter::TabDetachedAt(WebContents* contents, int index) {
  if (GetTabEntry(contents).get() == NULL) {
    // The tab was removed. Don't send detach event.
    return;
  }

  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(
      new FundamentalValue(ExtensionTabUtil::GetTabId(contents)));

  base::DictionaryValue* object_args = new base::DictionaryValue();
  object_args->Set(tabs_constants::kOldWindowIdKey,
                   new FundamentalValue(
                       ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(tabs_constants::kOldPositionKey,
                   new FundamentalValue(index));
  args->Append(object_args);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_DETACHED, tabs::OnDetached::kEventName,
                args.Pass(), EventRouter::USER_GESTURE_UNKNOWN);
}

void TabsEventRouter::TabClosingAt(TabStripModel* tab_strip_model,
                                   WebContents* contents,
                                   int index) {
  int tab_id = ExtensionTabUtil::GetTabId(contents);

  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(new FundamentalValue(tab_id));

  base::DictionaryValue* object_args = new base::DictionaryValue();
  object_args->SetInteger(tabs_constants::kWindowIdKey,
                          ExtensionTabUtil::GetWindowIdOfTab(contents));
  object_args->SetBoolean(tabs_constants::kWindowClosing,
                          tab_strip_model->closing_all());
  args->Append(object_args);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_REMOVED, tabs::OnRemoved::kEventName,
                args.Pass(), EventRouter::USER_GESTURE_UNKNOWN);

  int removed_count = tab_entries_.erase(tab_id);
  DCHECK_GT(removed_count, 0);

  UnregisterForTabNotifications(contents);
}

void TabsEventRouter::ActiveTabChanged(WebContents* old_contents,
                                       WebContents* new_contents,
                                       int index,
                                       int reason) {
  scoped_ptr<base::ListValue> args(new base::ListValue);
  int tab_id = ExtensionTabUtil::GetTabId(new_contents);
  args->Append(new FundamentalValue(tab_id));

  base::DictionaryValue* object_args = new base::DictionaryValue();
  object_args->Set(tabs_constants::kWindowIdKey,
                   new FundamentalValue(
                       ExtensionTabUtil::GetWindowIdOfTab(new_contents)));
  args->Append(object_args);

  // The onActivated event replaced onActiveChanged and onSelectionChanged. The
  // deprecated events take two arguments: tabId, {windowId}.
  Profile* profile =
      Profile::FromBrowserContext(new_contents->GetBrowserContext());
  EventRouter::UserGestureState gesture =
      reason & CHANGE_REASON_USER_GESTURE
      ? EventRouter::USER_GESTURE_ENABLED
      : EventRouter::USER_GESTURE_NOT_ENABLED;
  DispatchEvent(profile, events::TABS_ON_SELECTION_CHANGED,
                tabs::OnSelectionChanged::kEventName,
                scoped_ptr<base::ListValue>(args->DeepCopy()), gesture);
  DispatchEvent(profile, events::TABS_ON_ACTIVE_CHANGED,
                tabs::OnActiveChanged::kEventName,
                scoped_ptr<base::ListValue>(args->DeepCopy()), gesture);

  // The onActivated event takes one argument: {windowId, tabId}.
  args->Remove(0, NULL);
  object_args->Set(tabs_constants::kTabIdKey,
                   new FundamentalValue(tab_id));
  DispatchEvent(profile, events::TABS_ON_ACTIVATED,
                tabs::OnActivated::kEventName, args.Pass(), gesture);
}

void TabsEventRouter::TabSelectionChanged(
    TabStripModel* tab_strip_model,
    const ui::ListSelectionModel& old_model) {
  ui::ListSelectionModel::SelectedIndices new_selection =
      tab_strip_model->selection_model().selected_indices();
  scoped_ptr<base::ListValue> all_tabs(new base::ListValue);

  for (size_t i = 0; i < new_selection.size(); ++i) {
    int index = new_selection[i];
    WebContents* contents = tab_strip_model->GetWebContentsAt(index);
    if (!contents)
      break;
    int tab_id = ExtensionTabUtil::GetTabId(contents);
    all_tabs->Append(new FundamentalValue(tab_id));
  }

  scoped_ptr<base::ListValue> args(new base::ListValue);
  scoped_ptr<base::DictionaryValue> select_info(new base::DictionaryValue);

  select_info->Set(
      tabs_constants::kWindowIdKey,
      new FundamentalValue(
          ExtensionTabUtil::GetWindowIdOfTabStripModel(tab_strip_model)));

  select_info->Set(tabs_constants::kTabIdsKey, all_tabs.release());
  args->Append(select_info.release());

  // The onHighlighted event replaced onHighlightChanged.
  Profile* profile = tab_strip_model->profile();
  DispatchEvent(profile, events::TABS_ON_HIGHLIGHT_CHANGED,
                tabs::OnHighlightChanged::kEventName,
                scoped_ptr<base::ListValue>(args->DeepCopy()),
                EventRouter::USER_GESTURE_UNKNOWN);
  DispatchEvent(profile, events::TABS_ON_HIGHLIGHTED,
                tabs::OnHighlighted::kEventName, args.Pass(),
                EventRouter::USER_GESTURE_UNKNOWN);
}

void TabsEventRouter::TabMoved(WebContents* contents,
                               int from_index,
                               int to_index) {
  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(
      new FundamentalValue(ExtensionTabUtil::GetTabId(contents)));

  base::DictionaryValue* object_args = new base::DictionaryValue();
  object_args->Set(tabs_constants::kWindowIdKey,
                   new FundamentalValue(
                       ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(tabs_constants::kFromIndexKey,
                   new FundamentalValue(from_index));
  object_args->Set(tabs_constants::kToIndexKey,
                   new FundamentalValue(to_index));
  args->Append(object_args);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_MOVED, tabs::OnMoved::kEventName,
                args.Pass(), EventRouter::USER_GESTURE_UNKNOWN);
}

void TabsEventRouter::TabUpdated(
    linked_ptr<TabEntry> entry,
    scoped_ptr<base::DictionaryValue> changed_properties) {
  CHECK(entry->web_contents());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableTabAudioMuting)) {
    bool audible = entry->web_contents()->WasRecentlyAudible();
    if (entry->SetAudible(audible)) {
      changed_properties->SetBoolean(tabs_constants::kAudibleKey, audible);
    }

    bool muted = entry->web_contents()->IsAudioMuted();
    if (entry->SetMuted(muted)) {
      changed_properties->SetBoolean(tabs_constants::kMutedKey, muted);
      changed_properties->SetString(
          tabs_constants::kMutedCauseKey,
          ExtensionTabUtil::GetTabAudioMutedReason(entry->web_contents()));
    }
  }

  if (!changed_properties->empty()) {
    DispatchTabUpdatedEvent(entry->web_contents(), changed_properties.Pass());
  }
}

void TabsEventRouter::FaviconUrlUpdated(WebContents* contents) {
    content::NavigationEntry* entry =
        contents->GetController().GetVisibleEntry();
    if (!entry || !entry->GetFavicon().valid)
      return;
    scoped_ptr<base::DictionaryValue> changed_properties(
        new base::DictionaryValue);
    changed_properties->SetString(
        tabs_constants::kFaviconUrlKey,
        entry->GetFavicon().url.possibly_invalid_spec());
    DispatchTabUpdatedEvent(contents, changed_properties.Pass());
}

void TabsEventRouter::DispatchEvent(
    Profile* profile,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    scoped_ptr<base::ListValue> args,
    EventRouter::UserGestureState user_gesture) {
  EventRouter* event_router = EventRouter::Get(profile);
  if (!profile_->IsSameProfile(profile) || !event_router)
    return;

  scoped_ptr<Event> event(new Event(histogram_value, event_name, args.Pass()));
  event->restrict_to_browser_context = profile;
  event->user_gesture = user_gesture;
  event_router->BroadcastEvent(event.Pass());
}

void TabsEventRouter::DispatchTabUpdatedEvent(
    WebContents* contents,
    scoped_ptr<base::DictionaryValue> changed_properties) {
  DCHECK(changed_properties);
  DCHECK(contents);

  // The state of the tab (as seen from the extension point of view) has
  // changed.  Send a notification to the extension.
  scoped_ptr<base::ListValue> args_base(new base::ListValue);

  // First arg: The id of the tab that changed.
  args_base->AppendInteger(ExtensionTabUtil::GetTabId(contents));

  // Second arg: An object containing the changes to the tab state.  Filled in
  // by WillDispatchTabUpdatedEvent as a copy of changed_properties, if the
  // extension has the tabs permission.

  // Third arg: An object containing the state of the tab. Filled in by
  // WillDispatchTabUpdatedEvent.
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  scoped_ptr<Event> event(new Event(
      events::TABS_ON_UPDATED, tabs::OnUpdated::kEventName, args_base.Pass()));
  event->restrict_to_browser_context = profile;
  event->user_gesture = EventRouter::USER_GESTURE_NOT_ENABLED;
  event->will_dispatch_callback =
      base::Bind(&WillDispatchTabUpdatedEvent,
                 contents,
                 changed_properties.get());
  EventRouter::Get(profile)->BroadcastEvent(event.Pass());
}

linked_ptr<TabsEventRouter::TabEntry> TabsEventRouter::GetTabEntry(
    WebContents* contents) {
  int tab_id = ExtensionTabUtil::GetTabId(contents);

  TabEntryMap::iterator i = tab_entries_.find(tab_id);
  if (tab_entries_.end() == i)
    return linked_ptr<TabEntry>(NULL);
  return i->second;
}

void TabsEventRouter::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    NavigationController* source_controller =
        content::Source<NavigationController>(source).ptr();
    linked_ptr<TabEntry> entry =
        GetTabEntry(source_controller->GetWebContents());
    CHECK(entry.get());
    TabUpdated(entry, (entry.get())->DidNavigate());
  } else if (type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED) {
    // Tab was destroyed after being detached (without being re-attached).
    WebContents* contents = content::Source<WebContents>(source).ptr();
    registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<NavigationController>(&contents->GetController()));
    registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
        content::Source<WebContents>(contents));
    favicon_scoped_observer_.Remove(
        favicon::ContentFaviconDriver::FromWebContents(contents));
  } else {
    NOTREACHED();
  }
}

void TabsEventRouter::TabChangedAt(WebContents* contents,
                                   int index,
                                   TabChangeType change_type) {
  linked_ptr<TabEntry> entry = GetTabEntry(contents);
  CHECK(entry.get());
  TabUpdated(entry, (entry.get())->UpdateLoadState());
}

void TabsEventRouter::TabReplacedAt(TabStripModel* tab_strip_model,
                                    WebContents* old_contents,
                                    WebContents* new_contents,
                                    int index) {
  // Notify listeners that the next tabs closing or being added are due to
  // WebContents being swapped.
  const int new_tab_id = ExtensionTabUtil::GetTabId(new_contents);
  const int old_tab_id = ExtensionTabUtil::GetTabId(old_contents);
  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(new FundamentalValue(new_tab_id));
  args->Append(new FundamentalValue(old_tab_id));

  DispatchEvent(Profile::FromBrowserContext(new_contents->GetBrowserContext()),
                events::TABS_ON_REPLACED, tabs::OnReplaced::kEventName,
                args.Pass(), EventRouter::USER_GESTURE_UNKNOWN);

  // Update tab_entries_.
  const int removed_count = tab_entries_.erase(old_tab_id);
  DCHECK_GT(removed_count, 0);
  UnregisterForTabNotifications(old_contents);

  if (GetTabEntry(new_contents).get() == NULL) {
    tab_entries_[new_tab_id] = make_linked_ptr(new TabEntry(new_contents));
    RegisterForTabNotifications(new_contents);
  }
}

void TabsEventRouter::TabPinnedStateChanged(WebContents* contents, int index) {
  TabStripModel* tab_strip = NULL;
  int tab_index;

  if (ExtensionTabUtil::GetTabStripModel(contents, &tab_strip, &tab_index)) {
    scoped_ptr<base::DictionaryValue> changed_properties(
        new base::DictionaryValue());
    changed_properties->SetBoolean(tabs_constants::kPinnedKey,
                                   tab_strip->IsTabPinned(tab_index));
    DispatchTabUpdatedEvent(contents, changed_properties.Pass());
  }
}

void TabsEventRouter::OnZoomChanged(
    const ZoomController::ZoomChangedEventData& data) {
  DCHECK(data.web_contents);
  int tab_id = ExtensionTabUtil::GetTabId(data.web_contents);
  if (tab_id < 0)
    return;

  // Prepare the zoom change information.
  api::tabs::OnZoomChange::ZoomChangeInfo zoom_change_info;
  zoom_change_info.tab_id = tab_id;
  zoom_change_info.old_zoom_factor =
      content::ZoomLevelToZoomFactor(data.old_zoom_level);
  zoom_change_info.new_zoom_factor =
      content::ZoomLevelToZoomFactor(data.new_zoom_level);
  ZoomModeToZoomSettings(data.zoom_mode,
                         &zoom_change_info.zoom_settings);

  // Dispatch the |onZoomChange| event.
  Profile* profile = Profile::FromBrowserContext(
      data.web_contents->GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_ZOOM_CHANGE,
                tabs::OnZoomChange::kEventName,
                api::tabs::OnZoomChange::Create(zoom_change_info),
                EventRouter::USER_GESTURE_UNKNOWN);
}

void TabsEventRouter::OnFaviconAvailable(const gfx::Image& image) {
}

void TabsEventRouter::OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                                       bool icon_url_changed) {
  if (icon_url_changed) {
    favicon::ContentFaviconDriver* content_favicon_driver =
        static_cast<favicon::ContentFaviconDriver*>(favicon_driver);
    FaviconUrlUpdated(content_favicon_driver->web_contents());
  }
}

}  // namespace extensions
