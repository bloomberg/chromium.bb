// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/other_device_menu_controller.h"

#include "base/metrics/histogram.h"
#include "base/string16.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/ntp/foreign_session_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/text_elider.h"
#include "ui/base/window_open_disposition.h"

namespace {

// The max number of tabs that will be added to the menu.
const size_t kMaxTabsToShow = 18;

// The max width of a menu.  Menu text exceeding this will be elided.
const int kMaxWidth = 375;

// Enumerates the different menu item types.
enum ItemType {
  // An item for restoring a single tab.
  TAB,
  // An item for restoring all tabs in this session.
  OPEN_ALL,
  // Number of enum entries, used for UMA histogram reporting macros.
  ITEM_TYPE_ENUM_COUNT,
};

// Helper function that returns the largest tab timestamp for the window.
double GetMostRecentTabTimestamp(const SessionWindow* window) {
  double max_timestamp = 0;
  for (size_t i = 0, num_tabs = window->tabs.size(); i < num_tabs; ++i) {
    linked_ptr<DictionaryValue> tab_value =
        linked_ptr<DictionaryValue>(new DictionaryValue());
    if (browser_sync::ForeignSessionHandler::SessionTabToValue(
            *window->tabs[i], tab_value.get())) {
      double timestamp;
      tab_value->GetDouble("timestamp", &timestamp);
      if (timestamp > max_timestamp)
        max_timestamp = timestamp;
    }
  }
  return max_timestamp;
}

// Comparator function to sort windows by last-modified time.  Windows in a
// session share the same timestamp, so we need to use tab timestamps instead.
// instead.
bool SortWindowsByRecency(const SessionWindow* w1, const SessionWindow* w2) {
  return GetMostRecentTabTimestamp(w1) > GetMostRecentTabTimestamp(w2);
}

}  // namespace

OtherDeviceMenuController::OtherDeviceMenuController(
    content::WebUI* web_ui,
    const std::string& session_id,
    const gfx::Point& location)
    : web_ui_(web_ui),
      session_id_(session_id),
      location_(location),
      ALLOW_THIS_IN_INITIALIZER_LIST(menu_model_(this)) {

  // Initialize the model.
  AddDeviceTabs();

  // Add a "Open all" menu item if there is more than one tab.
  if (!tab_data_.empty()) {
    linked_ptr<DictionaryValue> open_all_tab_value =
        linked_ptr<DictionaryValue>(new DictionaryValue());
    // kInvalidId signifies that the entire session should be opened.
    open_all_tab_value->SetInteger(
        "sessionId",
        browser_sync::ForeignSessionHandler::kInvalidId);
    open_all_tab_value->SetInteger(
        "windowId",
        browser_sync::ForeignSessionHandler::kInvalidId);
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_.AddItem(
        tab_data_.size(),
        l10n_util::GetStringUTF16(IDS_NEW_TAB_OTHER_SESSIONS_OPEN_ALL));
    tab_data_.push_back(open_all_tab_value);
  }

  // Create the view.
  view_.reset(OtherDeviceMenu::Create(&menu_model_));
}

OtherDeviceMenuController::~OtherDeviceMenuController() {
}

void OtherDeviceMenuController::ShowMenu() {
  content::WebContents* web_contents = web_ui_->GetWebContents();
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  view_->ShowMenu(browser->window()->GetNativeWindow(), location_);
}

bool OtherDeviceMenuController::IsCommandIdChecked(int command_id) const {
  return false;
}

bool OtherDeviceMenuController::IsCommandIdEnabled(int command_id) const {
  return true;
}

void OtherDeviceMenuController::ExecuteCommand(int command_id) {
  ExecuteCommand(command_id, 0);
}

// TODO(jeremycho): Figure out why mouse wheel clicks don't trigger this.
void OtherDeviceMenuController::ExecuteCommand(int command_id,
                                               int event_flags) {
  DCHECK_GT(tab_data_.size(), static_cast<size_t>(command_id)) <<
      "Invalid command_id from other device menu.";

  linked_ptr<DictionaryValue> tab_data = tab_data_[command_id];
  // This is not a mistake - sessionId actually refers to the tab id.
  // See http://crbug.com/154865.
  int tab_id = browser_sync::ForeignSessionHandler::kInvalidId;
  tab_data->GetInteger("sessionId", &tab_id);

  int window_id = browser_sync::ForeignSessionHandler::kInvalidId;
  tab_data->GetInteger("windowId", &window_id);

  if (tab_id != browser_sync::ForeignSessionHandler::kInvalidId) {
    WindowOpenDisposition disposition =
        chrome::DispositionFromEventFlags(event_flags);
    browser_sync::ForeignSessionHandler::OpenForeignSessionTab(
        web_ui_, session_id_, window_id, tab_id, disposition);
  } else {
    browser_sync::ForeignSessionHandler::OpenForeignSessionWindows(
        web_ui_, session_id_, window_id);
  }
  ItemType itemType = tab_id ==
      browser_sync::ForeignSessionHandler::kInvalidId ? OPEN_ALL : TAB;
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.OtherDeviceMenu",
                            itemType, ITEM_TYPE_ENUM_COUNT);
}

bool OtherDeviceMenuController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void OtherDeviceMenuController::AddDeviceTabs() {
  browser_sync::SessionModelAssociator* associator =
      browser_sync::ForeignSessionHandler::GetModelAssociator(web_ui_);
  std::vector<const SessionWindow*> windows;

  // Populate the menu with the device's tabs, using separators between windows.
  if (associator && associator->GetForeignSession(session_id_, &windows)) {
    // Show windows by descending last-modified time.
    std::sort(windows.begin(), windows.end(), SortWindowsByRecency);
    bool last_window_has_tabs = false;
    for (std::vector<const SessionWindow*>::const_iterator it =
             windows.begin(); it != windows.end(); ++it) {
      if (last_window_has_tabs)
        menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
      last_window_has_tabs = false;

      const SessionWindow* window = *it;
      for (size_t i = 0, num_tabs = window->tabs.size(); i < num_tabs; ++i) {
        linked_ptr<DictionaryValue> tab_value =
            linked_ptr<DictionaryValue>(new DictionaryValue());
        if (!browser_sync::ForeignSessionHandler::SessionTabToValue(
                *window->tabs[i], tab_value.get())) {
          continue;
        }
        last_window_has_tabs = true;
        tab_value->SetInteger("windowId", window->window_id.id());
        string16 title;
        tab_value->GetString("title", &title);
        title = ui::ElideText(
            title, gfx::Font(), kMaxWidth, ui::ELIDE_AT_END);
        menu_model_.AddItem(tab_data_.size(), title);
        // TODO(jeremycho): Use tab_value.GetString("url", &url) to request
        // favicons. http://crbug.com/153410.
        tab_data_.push_back(tab_value);
        if (tab_data_.size() >= kMaxTabsToShow)
          return;
      }
    }
  }
}

// OtherDevice menu ----------------------------------------------------------

OtherDeviceMenu::~OtherDeviceMenu() {}
