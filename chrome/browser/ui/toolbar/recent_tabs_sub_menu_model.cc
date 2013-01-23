// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"

#include "base/bind.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_delegate.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"

#if defined(USE_ASH)
#include "ash/accelerators/accelerator_table.h"
#endif  // defined(USE_ASH)

namespace {

// First comamnd id for navigatable (and hence executable) tab menu item.
// The model and menu are not 1-1:
// - menu has "Reopen closed tab", "No tabs from other devices", device section
//   headers, separators and executable tab items.
// - model only has navigatabale (and hence executable) tab items.
// Using an initial command id for tab items makes it easier and less error-
// prone to manipulate the model and menu.
// This value must be bigger than the maximum possible number of items in menu,
// so that index of last menu item doesn't clash with this value when menu items
// are retrieved via GetIndexOfCommandId.
const int kFirstTabCommandId = 100;

// Command Id for disabled menu items, e.g. device section header,
// "No tabs from other devices", etc.
const int kDisabledCommandId = 1000;

// Comparator function for use with std::sort that will sort sessions by
// descending modified_time (i.e., most recent first).
bool SortSessionsByRecency(const browser_sync::SyncedSession* s1,
                           const browser_sync::SyncedSession* s2) {
  return s1->modified_time > s2->modified_time;
}

// Comparator function for use with std::sort that will sort tabs by
// descending timestamp (i.e., most recent first).
bool SortTabsByRecency(const SessionTab* t1, const SessionTab* t2) {
  return t1->timestamp > t2->timestamp;
}

// Convert |model_index| to command id of menu item.
int ModelIndexToCommandId(int model_index) {
  int command_id = model_index + kFirstTabCommandId;
  DCHECK(command_id != kDisabledCommandId);
  return command_id;
}

// Convert |command_id| of menu item to index in model.
int CommandIdToModelIndex(int command_id) {
  DCHECK(command_id != kDisabledCommandId);
  return command_id - kFirstTabCommandId;
}

}  // namepace

// An element in |RecentTabsSubMenuModel::model_| that stores the navigation
// information of a local or foreign tab required to restore the tab.
struct RecentTabsSubMenuModel::NavigationItem {
  NavigationItem() : tab_id(-1) {}

  NavigationItem(const std::string& session_tag,
                 const SessionID::id_type& tab_id,
                 const GURL& url)
      : session_tag(session_tag),
        tab_id(tab_id),
        url(url) {}

  // For use by std::set for sorting.
  bool operator<(const NavigationItem& other) const {
    return url < other.url;
  }

  std::string session_tag;  // Empty for local tabs, non-empty for foreign tabs.
  SessionID::id_type tab_id;  // -1 for invalid, >= 0 otherwise.
  GURL url;
};

RecentTabsSubMenuModel::RecentTabsSubMenuModel(
    ui::AcceleratorProvider* accelerator_provider,
    Browser* browser,
    browser_sync::SessionModelAssociator* associator)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      browser_(browser),
      associator_(associator),
      default_favicon_(ResourceBundle::GetSharedInstance().
          GetNativeImageNamed(IDR_DEFAULT_FAVICON)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  Build();

  // Retrieve accelerator key for IDC_RESTORE_TAB now, because on ASH, it's not
  // defined in |accelerator_provider|, but in shell, so simply retrieve it now
  // for all ASH and non-ASH for use in |GetAcceleratorForCommandId|.
#if defined(USE_ASH)
  for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i) {
    const ash::AcceleratorData& accel_data = ash::kAcceleratorData[i];
    if (accel_data.action == ash::RESTORE_TAB) {
      reopen_closed_tab_accelerator_ = ui::Accelerator(accel_data.keycode,
                                                       accel_data.modifiers);
      break;
    }
  }
#else
  if (accelerator_provider) {
    accelerator_provider->GetAcceleratorForCommandId(
        IDC_RESTORE_TAB, &reopen_closed_tab_accelerator_);
  }
#endif  // defined(USE_ASH)
}

RecentTabsSubMenuModel::~RecentTabsSubMenuModel() {
}

bool RecentTabsSubMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool RecentTabsSubMenuModel::IsCommandIdEnabled(int command_id) const {
  if (command_id == IDC_RESTORE_TAB)
    return chrome::IsCommandEnabled(browser_, command_id);
  if (command_id == kDisabledCommandId ||
      command_id == IDC_RECENT_TABS_NO_DEVICE_TABS) {
    return false;
  }
  int model_index = CommandIdToModelIndex(command_id);
  return model_index >= 0 && model_index < static_cast<int>(model_.size());
}

bool RecentTabsSubMenuModel::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  if (command_id == IDC_RESTORE_TAB &&
      reopen_closed_tab_accelerator_.key_code() != ui::VKEY_UNKNOWN) {
    *accelerator = reopen_closed_tab_accelerator_;
    return true;
  }
  return false;
}

bool RecentTabsSubMenuModel::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDC_RESTORE_TAB;
}

string16 RecentTabsSubMenuModel::GetLabelForCommandId(int command_id) const {
  DCHECK_EQ(command_id, IDC_RESTORE_TAB);

  int string_id = IDS_RESTORE_TAB;
  if (IsCommandIdEnabled(command_id)) {
    TabRestoreService* service =
        TabRestoreServiceFactory::GetForProfile(browser_->profile());
    if (service && !service->entries().empty() &&
        service->entries().front()->type == TabRestoreService::WINDOW) {
      string_id = IDS_RESTORE_WINDOW;
    }
  }
  return l10n_util::GetStringUTF16(string_id);
}

void RecentTabsSubMenuModel::ExecuteCommand(int command_id) {
  ExecuteCommand(command_id, 0);
}

void RecentTabsSubMenuModel::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == IDC_RESTORE_TAB) {
    chrome::ExecuteCommandWithDisposition(browser_, command_id,
        ui::DispositionFromEventFlags(event_flags));
    return;
  }

  DCHECK_NE(kDisabledCommandId, command_id);
  DCHECK_NE(IDC_RECENT_TABS_NO_DEVICE_TABS, command_id);

  int model_idx = CommandIdToModelIndex(command_id);
  DCHECK(model_idx >= 0 && model_idx < static_cast<int>(model_.size()));
  const NavigationItem& item = model_[model_idx];
  DCHECK(item.tab_id > -1 && item.url.is_valid());

  WindowOpenDisposition disposition =
      ui::DispositionFromEventFlags(event_flags);
  if (disposition == CURRENT_TAB)  // Force to open a new foreground tab.
    disposition = NEW_FOREGROUND_TAB;

  if (item.session_tag.empty()) {  // Restore tab of local session.
    TabRestoreService* service =
        TabRestoreServiceFactory::GetForProfile(browser_->profile());
    if (!service)
      return;
    TabRestoreServiceDelegate* delegate =
        TabRestoreServiceDelegate::FindDelegateForWebContents(
            chrome::GetActiveWebContents(browser_));
    if (!delegate)
      return;
    service->RestoreEntryById(delegate, item.tab_id,
                              browser_->host_desktop_type(), disposition);
  } else {  // Restore tab of foreign session.
    browser_sync::SessionModelAssociator* associator = GetModelAssociator();
    if (!associator)
      return;
    const SessionTab* tab;
    if (!associator->GetForeignTab(item.session_tag, item.tab_id, &tab))
      return;
    if (tab->navigations.empty())
      return;
    SessionRestore::RestoreForeignSessionTab(
        chrome::GetActiveWebContents(browser_), *tab, disposition);
  }
}

int RecentTabsSubMenuModel::GetMaxWidthForItemAtIndex(int item_index) const {
  int command_id = GetCommandIdAt(item_index);
  if (command_id == IDC_RECENT_TABS_NO_DEVICE_TABS ||
      command_id == IDC_RESTORE_TAB) {
    return -1;
  }
  return 320;
}

void RecentTabsSubMenuModel::Build() {
  // The menu contains:
  // - Reopen closed tab, then separator
  // - device 1 section header, then list of tabs from device, then separator
  // - device 2 section header, then list of tabs from device, then separator
  // ...
  // |model_| only contains navigatable (and hence executable) tab items for
  // other devices.
  BuildLastClosed();
  BuildDevices();
  if (model_.empty()) {
    AddItemWithStringId(IDC_RECENT_TABS_NO_DEVICE_TABS,
                        IDS_RECENT_TABS_NO_DEVICE_TABS);
  }
}

void RecentTabsSubMenuModel::BuildLastClosed() {
  AddItem(IDC_RESTORE_TAB, GetLabelForCommandId(IDC_RESTORE_TAB));
  AddSeparator(ui::NORMAL_SEPARATOR);
}

void RecentTabsSubMenuModel::BuildDevices() {
  browser_sync::SessionModelAssociator* associator = GetModelAssociator();
  if (!associator)
    return;

  std::vector<const browser_sync::SyncedSession*> sessions;
  if (!associator->GetAllForeignSessions(&sessions))
    return;

  // Sort sessions from most recent to least recent.
  std::sort(sessions.begin(), sessions.end(), SortSessionsByRecency);

  const size_t kMaxSessionsToShow = 3;
  size_t num_sessions_added = 0;
  bool need_separator = false;
  for (size_t i = 0;
       i < sessions.size() && num_sessions_added < kMaxSessionsToShow;
       ++i) {
    const browser_sync::SyncedSession* session = sessions[i];
    const std::string& session_tag = session->session_tag;

    // Get windows of session.
    std::vector<const SessionWindow*> windows;
    if (!associator->GetForeignSession(session_tag, &windows) ||
        windows.empty()) {
      continue;
    }

    // Collect tabs from all windows of session, pruning those that are not
    // syncable or are NewTabPage, then sort them from most recent to least
    // recent, independent of which window the tabs were from.
    std::vector<const SessionTab*> tabs_in_session;
    for (size_t j = 0; j < windows.size(); ++j) {
      const SessionWindow* window = windows[j];
      for (size_t t = 0; t < window->tabs.size(); ++t) {
        const SessionTab* tab = window->tabs[t];
        if (tab->navigations.empty())
          continue;
        const TabNavigation& current_navigation =
            tab->navigations.at(tab->normalized_navigation_index());
        if (current_navigation.virtual_url() ==
            GURL(chrome::kChromeUINewTabURL)) {
          continue;
        }
        tabs_in_session.push_back(tab);
      }
    }
    if (tabs_in_session.empty())
      continue;
    std::sort(tabs_in_session.begin(), tabs_in_session.end(),
              SortTabsByRecency);

    // Build tab menu items from sorted session tabs.
    const size_t kMaxTabsPerSessionToShow = 4;
    for (size_t k = 0;
         k < std::min(tabs_in_session.size(), kMaxTabsPerSessionToShow);
         ++k) {
      BuildForeignTabItem(session_tag, *tabs_in_session[k],
          // Only need |session_name| for the first tab of the session.
          !k ? session->session_name : std::string(), session->device_type,
          need_separator);
      need_separator = false;
    }  // for all tabs in one session

    ++num_sessions_added;
    need_separator = true;
  }  // for all sessions
}

void RecentTabsSubMenuModel::BuildForeignTabItem(
    const std::string& session_tag,
    const SessionTab& tab,
    const std::string& session_name,
    browser_sync::SyncedSession::DeviceType device_type,
    bool need_separator) {
  if (need_separator)
    AddSeparator(ui::NORMAL_SEPARATOR);

  if (!session_name.empty()) {
    AddItem(kDisabledCommandId, UTF8ToUTF16(session_name));
    AddDeviceFavicon(GetItemCount() - 1, device_type);
  }

  const TabNavigation& current_navigation =
      tab.navigations.at(tab.normalized_navigation_index());
  NavigationItem item(session_tag, tab.tab_id.id(),
                      current_navigation.virtual_url());
  int command_id = ModelIndexToCommandId(model_.size());
  // There may be no tab title, in which case, use the url as tab title.
  AddItem(command_id,
          current_navigation.title().empty() ?
              UTF8ToUTF16(item.url.spec()) : current_navigation.title());
  AddTabFavicon(model_.size(), command_id, item.url);
  model_.push_back(item);
}

void RecentTabsSubMenuModel::AddDeviceFavicon(
    int index_in_menu,
    browser_sync::SyncedSession::DeviceType device_type) {
  int favicon_id = -1;
  switch (device_type) {
    case browser_sync::SyncedSession::TYPE_PHONE:
      favicon_id = IDR_PHONE_FAVICON;
      break;

    case browser_sync::SyncedSession::TYPE_TABLET:
      favicon_id = IDR_TABLET_FAVICON;
      break;

    case browser_sync::SyncedSession::TYPE_CHROMEOS:
    case browser_sync::SyncedSession::TYPE_WIN:
    case browser_sync::SyncedSession::TYPE_MACOSX:
    case browser_sync::SyncedSession::TYPE_LINUX:
    case browser_sync::SyncedSession::TYPE_OTHER:
    case browser_sync::SyncedSession::TYPE_UNSET:
      favicon_id = IDR_LAPTOP_FAVICON;
      break;
  };

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SetIcon(index_in_menu, rb.GetNativeImageNamed(favicon_id));
}

void RecentTabsSubMenuModel::AddTabFavicon(int model_index,
                                           int command_id,
                                           const GURL& url) {
  int index_in_menu = GetIndexOfCommandId(command_id);

  // If tab has synced favicon, use it.
  // Note that currently, foreign tab only has favicon if --sync-tab-favicons
  // switch is on; according to zea@, this flag is now automatically enabled for
  // iOS and android, and they're looking into enabling it for other platforms.
  browser_sync::SessionModelAssociator* associator = GetModelAssociator();
  std::string favicon_png;
  if (associator &&
      associator->GetSyncedFaviconForPageURL(url.spec(), &favicon_png)) {
    gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
        reinterpret_cast<const unsigned char*>(favicon_png.data()),
        favicon_png.size());
    SetIcon(index_in_menu, image);
    return;
  }

  // Otherwise, start to fetch the favicon from local history asynchronously.
  // Set default icon first.
  SetIcon(index_in_menu, default_favicon_);
  // Start request to fetch actual icon if possible.
  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      browser_->profile(), Profile::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;

  favicon_service->GetFaviconImageForURL(
      FaviconService::FaviconForURLParams(browser_->profile(),
                                          url,
                                          history::FAVICON,
                                          gfx::kFaviconSize),
      base::Bind(&RecentTabsSubMenuModel::OnFaviconDataAvailable,
                 weak_ptr_factory_.GetWeakPtr(),
                 command_id),
      &cancelable_task_tracker_);
}

void RecentTabsSubMenuModel::OnFaviconDataAvailable(
    int command_id,
    const history::FaviconImageResult& image_result) {
  if (image_result.image.IsEmpty())
    return;
  DCHECK(!model_.empty());
  int index_in_menu = GetIndexOfCommandId(command_id);
  DCHECK(index_in_menu != -1);
  SetIcon(index_in_menu, image_result.image);
  if (GetMenuModelDelegate())
    GetMenuModelDelegate()->OnIconChanged(index_in_menu);
}

browser_sync::SessionModelAssociator*
    RecentTabsSubMenuModel::GetModelAssociator() {
  if (!associator_) {
    ProfileSyncService* service = ProfileSyncServiceFactory::GetInstance()->
        GetForProfile(browser_->profile());
    // Only return the associator if it exists and it is done syncing sessions.
    if (service && service->ShouldPushChanges())
      associator_ = service->GetSessionModelAssociator();
  }
  return associator_;
}
