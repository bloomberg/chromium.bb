// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/browser_proxy.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/platform_thread.h"
#include "base/time.h"
#include "chrome/test/automation/autocomplete_edit_proxy.h"
#include "chrome/test/automation/automation_constants.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "gfx/point.h"

using base::TimeDelta;
using base::TimeTicks;


bool BrowserProxy::ActivateTab(int tab_index) {
  if (!is_valid())
    return false;

  int activate_tab_response = -1;

  if (!sender_->Send(new AutomationMsg_ActivateTab(
                         0, handle_, tab_index, &activate_tab_response))) {
    return false;
  }

  if (activate_tab_response >= 0)
    return true;

  return false;
}

bool BrowserProxy::BringToFront() {
  if (!is_valid())
    return false;

  bool succeeded = false;

  if (!sender_->Send(new AutomationMsg_BringBrowserToFront(
                         0, handle_, &succeeded))) {
    return false;
  }

  return succeeded;
}

bool BrowserProxy::IsMenuCommandEnabled(int id, bool* enabled) {
  if (!is_valid())
    return false;

  return sender_->Send(new AutomationMsg_IsMenuCommandEnabled(0, handle_, id,
                                                              enabled));
}

bool BrowserProxy::AppendTab(const GURL& tab_url) {
  if (!is_valid())
    return false;

  int append_tab_response = -1;

  sender_->Send(new AutomationMsg_AppendTab(0, handle_, tab_url,
                                            &append_tab_response));
  return append_tab_response >= 0;
}

bool BrowserProxy::GetActiveTabIndex(int* active_tab_index) const {
  if (!is_valid())
    return false;

  if (!active_tab_index) {
    NOTREACHED();
    return false;
  }

  int active_tab_index_response = -1;

  if (!sender_->Send(new AutomationMsg_ActiveTabIndex(
                         0, handle_, &active_tab_index_response))) {
    return false;
  }

  if (active_tab_index_response >= 0) {
    *active_tab_index = active_tab_index_response;
    return true;
  }

  return false;
}

scoped_refptr<TabProxy> BrowserProxy::GetTab(int tab_index) const {
  if (!is_valid())
    return NULL;

  int tab_handle = 0;

  sender_->Send(new AutomationMsg_Tab(0, handle_, tab_index, &tab_handle));
  if (!tab_handle)
    return NULL;

  TabProxy* tab = static_cast<TabProxy*>(tracker_->GetResource(tab_handle));
  if (!tab) {
    tab = new TabProxy(sender_, tracker_, tab_handle);
    tab->AddRef();
  }

  // Since there is no scoped_refptr::attach.
  scoped_refptr<TabProxy> result;
  result.swap(&tab);
  return result;
}

scoped_refptr<TabProxy> BrowserProxy::GetActiveTab() const {
  int active_tab_index;
  if (!GetActiveTabIndex(&active_tab_index))
    return NULL;
  return GetTab(active_tab_index);
}

bool BrowserProxy::GetTabCount(int* num_tabs) const {
  if (!is_valid())
    return false;

  if (!num_tabs) {
    NOTREACHED();
    return false;
  }

  int tab_count_response = -1;

  if (!sender_->Send(new AutomationMsg_TabCount(
                         0, handle_, &tab_count_response))) {
    return false;
  }

  if (tab_count_response >= 0) {
    *num_tabs = tab_count_response;
    return true;
  }

  return false;
}

bool BrowserProxy::GetType(Browser::Type* type) const {
  if (!is_valid())
    return false;

  if (!type) {
    NOTREACHED();
    return false;
  }

  int type_as_int;
  if (!sender_->Send(new AutomationMsg_Type(0, handle_, &type_as_int)))
    return false;

  *type = static_cast<Browser::Type>(type_as_int);
  return true;
}

bool BrowserProxy::ApplyAccelerator(int id) {
  return RunCommandAsync(id);
}

bool BrowserProxy::SimulateDrag(const gfx::Point& start,
                                const gfx::Point& end,
                                int flags,
                                bool press_escape_en_route) {
  if (!is_valid())
    return false;

  std::vector<gfx::Point> drag_path;
  drag_path.push_back(start);
  drag_path.push_back(end);

  bool result = false;

  if (!sender_->Send(new AutomationMsg_WindowDrag(
          0, handle_, drag_path, flags, press_escape_en_route, &result))) {
    return false;
  }

  return result;
}

bool BrowserProxy::WaitForTabCountToBecome(int count, int wait_timeout) {
  const TimeTicks start = TimeTicks::Now();
  const TimeDelta timeout = TimeDelta::FromMilliseconds(wait_timeout);
  while (TimeTicks::Now() - start < timeout) {
    PlatformThread::Sleep(automation::kSleepTime);
    int new_count;
    if (!GetTabCount(&new_count))
      return false;
    if (count == new_count)
      return true;
  }
  // If we get here, the tab count doesn't match.
  return false;
}

bool BrowserProxy::WaitForTabToBecomeActive(int tab,
                                            int wait_timeout) {
  const TimeTicks start = TimeTicks::Now();
  const TimeDelta timeout = TimeDelta::FromMilliseconds(wait_timeout);
  while (TimeTicks::Now() - start < timeout) {
    PlatformThread::Sleep(automation::kSleepTime);
    int active_tab;
    if (GetActiveTabIndex(&active_tab) && active_tab == tab)
      return true;
  }
  // If we get here, the active tab hasn't changed.
  return false;
}

bool BrowserProxy::OpenFindInPage() {
  if (!is_valid())
    return false;

  return sender_->Send(new AutomationMsg_OpenFindInPage(0, handle_));
  // This message expects no response.
}

bool BrowserProxy::GetFindWindowLocation(int* x, int* y) {
  if (!is_valid() || !x || !y)
    return false;

  return sender_->Send(
      new AutomationMsg_FindWindowLocation(0, handle_, x, y));
}

bool BrowserProxy::IsFindWindowFullyVisible(bool* is_visible) {
  if (!is_valid())
    return false;

  if (!is_visible) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(
      new AutomationMsg_FindWindowVisibility(0, handle_, is_visible));
}

bool BrowserProxy::RunCommandAsync(int browser_command) const {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_WindowExecuteCommandAsync(0, handle_,
                                                            browser_command,
                                                            &result));

  return result;
}

bool BrowserProxy::RunCommand(int browser_command) const {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_WindowExecuteCommand(0, handle_,
                                                       browser_command,
                                                       &result));

  return result;
}

bool BrowserProxy::GetBookmarkBarVisibility(bool* is_visible,
                                            bool* is_animating) {
  if (!is_valid())
    return false;

  if (!is_visible || !is_animating) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(new AutomationMsg_BookmarkBarVisibility(
      0, handle_, is_visible, is_animating));
}

bool BrowserProxy::GetBookmarksAsJSON(std::string *json_string) {
  if (!is_valid())
    return false;

  if (!WaitForBookmarkModelToLoad())
    return false;

  bool result = false;
  sender_->Send(new AutomationMsg_GetBookmarksAsJSON(0, handle_,
                                                     json_string,
                                                     &result));
  return result;
}

bool BrowserProxy::WaitForBookmarkModelToLoad() {
  if (!is_valid())
    return false;

  bool result = false;
  sender_->Send(new AutomationMsg_WaitForBookmarkModelToLoad(0, handle_,
                                                             &result));
  return result;
}

bool BrowserProxy::AddBookmarkGroup(int64 parent_id, int index,
                                    std::wstring& title) {
  if (!is_valid())
    return false;
  bool result = false;
  sender_->Send(new AutomationMsg_AddBookmarkGroup(0, handle_,
                                                   parent_id, index,
                                                   title,
                                                   &result));
  return result;
}

bool BrowserProxy::AddBookmarkURL(int64 parent_id, int index,
                                  std::wstring& title, const GURL& url) {
  if (!is_valid())
    return false;
  bool result = false;
  sender_->Send(new AutomationMsg_AddBookmarkURL(0, handle_,
                                                 parent_id, index,
                                                 title, url,
                                                 &result));
  return result;
}

bool BrowserProxy::ReparentBookmark(int64 id, int64 new_parent_id, int index) {
  if (!is_valid())
    return false;
  bool result = false;
  sender_->Send(new AutomationMsg_ReparentBookmark(0, handle_,
                                                   id, new_parent_id,
                                                   index,
                                                   &result));
  return result;
}

bool BrowserProxy::SetBookmarkTitle(int64 id, std::wstring& title) {
  if (!is_valid())
    return false;
  bool result = false;
  sender_->Send(new AutomationMsg_SetBookmarkTitle(0, handle_,
                                                   id, title,
                                                   &result));
  return result;
}

bool BrowserProxy::SetBookmarkURL(int64 id, const GURL& url) {
  if (!is_valid())
    return false;
  bool result = false;
  sender_->Send(new AutomationMsg_SetBookmarkURL(0, handle_,
                                                 id, url,
                                                 &result));
  return result;
}

bool BrowserProxy::RemoveBookmark(int64 id) {
  if (!is_valid())
    return false;
  bool result = false;
  sender_->Send(new AutomationMsg_RemoveBookmark(0, handle_,
                                                 id,
                                                 &result));
  return result;
}

bool BrowserProxy::IsShelfVisible(bool* is_visible) {
  if (!is_valid())
    return false;

  if (!is_visible) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(new AutomationMsg_ShelfVisibility(0, handle_,
                                                         is_visible));
}

bool BrowserProxy::SetShelfVisible(bool is_visible) {
  if (!is_valid())
    return false;

  return sender_->Send(new AutomationMsg_SetShelfVisibility(0, handle_,
                                                            is_visible));
}

bool BrowserProxy::SetIntPreference(const std::string& name, int value) {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_SetIntPreference(0, handle_, name, value,
                                                   &result));
  return result;
}

bool BrowserProxy::SetStringPreference(const std::string& name,
                                       const std::string& value) {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_SetStringPreference(0, handle_, name, value,
                                                      &result));
  return result;
}

bool BrowserProxy::GetBooleanPreference(const std::string& name,
                                        bool* value) {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_GetBooleanPreference(0, handle_, name, value,
                                                       &result));
  return result;
}

bool BrowserProxy::SetBooleanPreference(const std::string& name,
                                        bool value) {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_SetBooleanPreference(0, handle_, name,
                                                       value, &result));
  return result;
}

bool BrowserProxy::SetDefaultContentSetting(ContentSettingsType content_type,
                                            ContentSetting setting) {
  return SetContentSetting(std::string(), content_type, setting);
}

bool BrowserProxy::SetContentSetting(const std::string& host,
                                     ContentSettingsType content_type,
                                     ContentSetting setting) {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_SetContentSetting(0, handle_, host,
                                                    content_type, setting,
                                                    &result));
  return result;
}

bool BrowserProxy::TerminateSession() {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_TerminateSession(0, handle_, &result));

  return result;
}

scoped_refptr<WindowProxy> BrowserProxy::GetWindow() const {
  if (!is_valid())
    return NULL;

  bool handle_ok = false;
  int window_handle = 0;

  sender_->Send(new AutomationMsg_WindowForBrowser(0, handle_, &handle_ok,
                                                   &window_handle));
  if (!handle_ok)
    return NULL;

  WindowProxy* window =
      static_cast<WindowProxy*>(tracker_->GetResource(window_handle));
  if (!window) {
    window = new WindowProxy(sender_, tracker_, window_handle);
    window->AddRef();
  }

  // Since there is no scoped_refptr::attach.
  scoped_refptr<WindowProxy> result;
  result.swap(&window);
  return result;
}

scoped_refptr<AutocompleteEditProxy> BrowserProxy::GetAutocompleteEdit() {
  if (!is_valid())
    return NULL;

  bool handle_ok = false;
  int autocomplete_edit_handle = 0;

  sender_->Send(new AutomationMsg_AutocompleteEditForBrowser(
      0, handle_, &handle_ok, &autocomplete_edit_handle));

  if (!handle_ok)
    return NULL;

  AutocompleteEditProxy* p = static_cast<AutocompleteEditProxy*>(
        tracker_->GetResource(autocomplete_edit_handle));

  if (!p) {
    p = new AutocompleteEditProxy(sender_, tracker_, autocomplete_edit_handle);
    p->AddRef();
  }

  // Since there is no scoped_refptr::attach.
  scoped_refptr<AutocompleteEditProxy> result;
  result.swap(&p);
  return result;
}

bool BrowserProxy::IsFullscreen(bool* is_fullscreen) {
  DCHECK(is_fullscreen);

  if (!is_valid())
    return false;

  return sender_->Send(new AutomationMsg_IsFullscreen(0, handle_,
                                                      is_fullscreen));
}

bool BrowserProxy::IsFullscreenBubbleVisible(bool* is_visible) {
  DCHECK(is_visible);

  if (!is_valid())
    return false;

  return sender_->Send(new AutomationMsg_IsFullscreenBubbleVisible(0, handle_,
                                                                   is_visible));
}

bool BrowserProxy::ShutdownSessionService() {
  bool did_shutdown = false;
  bool succeeded = sender_->Send(
      new AutomationMsg_ShutdownSessionService(0, handle_, &did_shutdown));

  if (!succeeded) {
    DLOG(ERROR) <<
        "ShutdownSessionService did not complete in a timely fashion";
    return false;
  }

  return did_shutdown;
}

bool BrowserProxy::StartTrackingPopupMenus() {
  if (!is_valid())
    return false;

  bool result = false;
  if (!sender_->Send(new AutomationMsg_StartTrackingPopupMenus
      (0, handle_, &result)))
    return false;
  return result;
}

bool BrowserProxy::WaitForPopupMenuToOpen() {
  if (!is_valid())
    return false;

  bool result = false;
  if (!sender_->Send(new AutomationMsg_WaitForPopupMenuToOpen
      (0, &result)))
    return false;
  return result;
}

bool BrowserProxy::SendJSONRequest(const std::string& request,
                                   std::string* response) {
  if (!is_valid())
    return false;

  bool result = false;
  return sender_->Send(new AutomationMsg_SendJSONRequest(0, handle_,
                                                         request, response,
                                                         &result));
  return result;
}

bool BrowserProxy::GetInitialLoadTimes(float* min_start_time,
                                       float* max_stop_time,
                                       std::vector<float>* stop_times) {
  std::string json_response;
  const char* kJSONCommand = "{\"command\": \"GetInitialLoadTimes\"}";

  *max_stop_time = 0;
  *min_start_time = -1;
  if (!SendJSONRequest(kJSONCommand, &json_response)) {
    // Older browser versions do not support GetInitialLoadTimes.
    // Fail gracefully and do not record them in this case.
    return false;
  }
  std::string error;
  base::JSONReader reader;
  scoped_ptr<Value> values(reader.ReadAndReturnError(json_response, true,
                                                     NULL, &error));
  if (!error.empty() || values->GetType() != Value::TYPE_DICTIONARY)
    return false;

  DictionaryValue* values_dict = static_cast<DictionaryValue*>(values.get());

  Value* tabs_value;
  if (!values_dict->Get("tabs", &tabs_value) ||
      tabs_value->GetType() != Value::TYPE_LIST)
    return false;

  ListValue* tabs_list = static_cast<ListValue*>(tabs_value);

  for (size_t i = 0; i < tabs_list->GetSize(); i++) {
    float stop_ms = 0;
    float start_ms = 0;
    Value* tab_value;
    DictionaryValue* tab_dict;

    if (!tabs_list->Get(i, &tab_value) ||
        tab_value->GetType() != Value::TYPE_DICTIONARY)
      return false;
    tab_dict = static_cast<DictionaryValue*>(tab_value);

    double temp;
    if (!tab_dict->GetReal("load_start_ms", &temp))
      return false;
    start_ms = static_cast<float>(temp);
    // load_stop_ms can only be null if WaitForInitialLoads did not run.
    if (!tab_dict->GetReal("load_stop_ms", &temp))
      return false;
    stop_ms = static_cast<float>(temp);

    if (i == 0)
      *min_start_time = start_ms;

    *min_start_time = std::min(start_ms, *min_start_time);
    *max_stop_time = std::max(stop_ms, *max_stop_time);
    stop_times->push_back(stop_ms);
  }
  std::sort(stop_times->begin(), stop_times->end());
  return true;
}
