// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"

#include <stddef.h>
#include <utility>

#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using base::DictionaryValue;
using base::ListValue;
using base::FundamentalValue;
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

TabsEventRouter::TabEntry::TabEntry(TabsEventRouter* router,
                                    content::WebContents* contents)
    : WebContentsObserver(contents),
      complete_waiting_on_load_(false),
      was_audible_(contents->WasRecentlyAudible()),
      was_muted_(contents->IsAudioMuted()),
      router_(router) {}

scoped_ptr<base::DictionaryValue> TabsEventRouter::TabEntry::UpdateLoadState() {
  // The tab may go in & out of loading (for instance if iframes navigate).
  // We only want to respond to the first change from loading to !loading after
  // the NavigationEntryCommitted() was fired.
  scoped_ptr<base::DictionaryValue> changed_properties(
      new base::DictionaryValue());
  if (!complete_waiting_on_load_ || web_contents()->IsLoading()) {
    return changed_properties;
  }

  // Send "complete" state change.
  complete_waiting_on_load_ = false;
  changed_properties->SetString(tabs_constants::kStatusKey,
                                tabs_constants::kStatusValueComplete);
  return changed_properties;
}

scoped_ptr<base::DictionaryValue> TabsEventRouter::TabEntry::DidNavigate() {
  // Send "loading" state change.
  complete_waiting_on_load_ = true;
  scoped_ptr<base::DictionaryValue> changed_properties(
      new base::DictionaryValue());
  changed_properties->SetString(tabs_constants::kStatusKey,
                                tabs_constants::kStatusValueLoading);

  if (web_contents()->GetURL() != url_) {
    url_ = web_contents()->GetURL();
    changed_properties->SetString(tabs_constants::kUrlKey, url_.spec());
  }

  return changed_properties;
}

scoped_ptr<base::DictionaryValue> TabsEventRouter::TabEntry::TitleChanged() {
  scoped_ptr<base::DictionaryValue> changed_properties(
      new base::DictionaryValue());
  changed_properties->SetString(tabs_constants::kTitleKey,
                                web_contents()->GetTitle());
  return changed_properties;
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

void TabsEventRouter::TabEntry::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  router_->TabUpdated(this, DidNavigate());
}

void TabsEventRouter::TabEntry::TitleWasSet(content::NavigationEntry* entry,
                                            bool explicit_set) {
  router_->TabUpdated(this, TitleChanged());
}

void TabsEventRouter::TabEntry::WebContentsDestroyed() {
  // This is necessary because it's possible for tabs to be created, detached
  // and then destroyed without ever having been re-attached and closed. This
  // happens in the case of a devtools WebContents that is opened in window,
  // docked, then closed.
  // Warning: |this| will be deleted after this call.
  router_->UnregisterForTabNotifications(web_contents());
}

TabsEventRouter::TabsEventRouter(Profile* profile)
    : profile_(profile),
      favicon_scoped_observer_(this),
      browser_tab_strip_tracker_(this, this, this) {
  DCHECK(!profile->IsOffTheRecord());

  browser_tab_strip_tracker_.Init(
      BrowserTabStripTracker::InitWith::ALL_BROWERS);
}

TabsEventRouter::~TabsEventRouter() {
}

bool TabsEventRouter::ShouldTrackBrowser(Browser* browser) {
  return profile_->IsSameProfile(browser->profile()) &&
         ExtensionTabUtil::BrowserSupportsTabs(browser);
}

void TabsEventRouter::RegisterForTabNotifications(WebContents* contents) {
  favicon_scoped_observer_.Add(
      favicon::ContentFaviconDriver::FromWebContents(contents));

  ZoomController::FromWebContents(contents)->AddObserver(this);

  int tab_id = ExtensionTabUtil::GetTabId(contents);
  DCHECK(tab_entries_.find(tab_id) == tab_entries_.end());
  tab_entries_[tab_id] = make_scoped_ptr(new TabEntry(this, contents));
}

void TabsEventRouter::UnregisterForTabNotifications(WebContents* contents) {
  favicon_scoped_observer_.Remove(
      favicon::ContentFaviconDriver::FromWebContents(contents));

  ZoomController::FromWebContents(contents)->RemoveObserver(this);

  int tab_id = ExtensionTabUtil::GetTabId(contents);
  int removed_count = tab_entries_.erase(tab_id);
  DCHECK_GT(removed_count, 0);
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
  tab_value->SetBoolean(tabs_constants::kActiveKey, active);
  return true;
}

void TabsEventRouter::TabCreatedAt(WebContents* contents,
                                   int index,
                                   bool active) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  scoped_ptr<base::ListValue> args(new base::ListValue);
  scoped_ptr<Event> event(new Event(
      events::TABS_ON_CREATED, tabs::OnCreated::kEventName, std::move(args)));
  event->restrict_to_browser_context = profile;
  event->user_gesture = EventRouter::USER_GESTURE_NOT_ENABLED;
  event->will_dispatch_callback =
      base::Bind(&WillDispatchTabCreatedEvent, contents, active);
  EventRouter::Get(profile)->BroadcastEvent(std::move(event));

  RegisterForTabNotifications(contents);
}

void TabsEventRouter::TabInsertedAt(WebContents* contents,
                                    int index,
                                    bool active) {
  if (!GetTabEntry(contents)) {
    // We've never seen this tab, send create event as long as we're not in the
    // constructor.
    if (browser_tab_strip_tracker_.is_processing_initial_browsers())
      RegisterForTabNotifications(contents);
    else
      TabCreatedAt(contents, index, active);
    return;
  }

  int tab_id = ExtensionTabUtil::GetTabId(contents);
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
                std::move(args), EventRouter::USER_GESTURE_UNKNOWN);
}

void TabsEventRouter::TabDetachedAt(WebContents* contents, int index) {
  if (!GetTabEntry(contents)) {
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
                std::move(args), EventRouter::USER_GESTURE_UNKNOWN);
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
                std::move(args), EventRouter::USER_GESTURE_UNKNOWN);

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
                tabs::OnActivated::kEventName, std::move(args), gesture);
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
                tabs::OnHighlighted::kEventName, std::move(args),
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
                std::move(args), EventRouter::USER_GESTURE_UNKNOWN);
}

void TabsEventRouter::TabUpdated(
    TabEntry* entry,
    scoped_ptr<base::DictionaryValue> changed_properties) {
  CHECK(entry->web_contents());

  bool audible = entry->web_contents()->WasRecentlyAudible();
  if (entry->SetAudible(audible)) {
    changed_properties->SetBoolean(tabs_constants::kAudibleKey, audible);
  }

  bool muted = entry->web_contents()->IsAudioMuted();
  if (entry->SetMuted(muted)) {
    changed_properties->Set(
        tabs_constants::kMutedInfoKey,
        ExtensionTabUtil::CreateMutedInfo(entry->web_contents()));
  }

  if (!changed_properties->empty()) {
    DispatchTabUpdatedEvent(entry->web_contents(),
                            std::move(changed_properties));
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
    DispatchTabUpdatedEvent(contents, std::move(changed_properties));
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

  scoped_ptr<Event> event(
      new Event(histogram_value, event_name, std::move(args)));
  event->restrict_to_browser_context = profile;
  event->user_gesture = user_gesture;
  event_router->BroadcastEvent(std::move(event));
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

  scoped_ptr<Event> event(new Event(events::TABS_ON_UPDATED,
                                    tabs::OnUpdated::kEventName,
                                    std::move(args_base)));
  event->restrict_to_browser_context = profile;
  event->user_gesture = EventRouter::USER_GESTURE_NOT_ENABLED;
  event->will_dispatch_callback =
      base::Bind(&WillDispatchTabUpdatedEvent,
                 contents,
                 changed_properties.get());
  EventRouter::Get(profile)->BroadcastEvent(std::move(event));
}

TabsEventRouter::TabEntry* TabsEventRouter::GetTabEntry(WebContents* contents) {
  const auto it = tab_entries_.find(ExtensionTabUtil::GetTabId(contents));

  return it == tab_entries_.end() ? nullptr : it->second.get();
}

void TabsEventRouter::TabChangedAt(WebContents* contents,
                                   int index,
                                   TabChangeType change_type) {
  TabEntry* entry = GetTabEntry(contents);
  CHECK(entry);
  TabUpdated(entry, entry->UpdateLoadState());
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
                std::move(args), EventRouter::USER_GESTURE_UNKNOWN);

  UnregisterForTabNotifications(old_contents);

  if (!GetTabEntry(new_contents))
    RegisterForTabNotifications(new_contents);
}

void TabsEventRouter::TabPinnedStateChanged(WebContents* contents, int index) {
  TabStripModel* tab_strip = NULL;
  int tab_index;

  if (ExtensionTabUtil::GetTabStripModel(contents, &tab_strip, &tab_index)) {
    scoped_ptr<base::DictionaryValue> changed_properties(
        new base::DictionaryValue());
    changed_properties->SetBoolean(tabs_constants::kPinnedKey,
                                   tab_strip->IsTabPinned(tab_index));
    DispatchTabUpdatedEvent(contents, std::move(changed_properties));
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

void TabsEventRouter::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  if (notification_icon_type == NON_TOUCH_16_DIP && icon_url_changed) {
    favicon::ContentFaviconDriver* content_favicon_driver =
        static_cast<favicon::ContentFaviconDriver*>(favicon_driver);
    FaviconUrlUpdated(content_favicon_driver->web_contents());
  }
}

}  // namespace extensions
