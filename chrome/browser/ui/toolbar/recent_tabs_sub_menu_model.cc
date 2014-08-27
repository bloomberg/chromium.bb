// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_delegate.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/open_tabs_ui_delegate.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon_base/favicon_types.h"
#include "grit/browser_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"

#if defined(USE_ASH)
#include "ash/accelerators/accelerator_table.h"
#endif  // defined(USE_ASH)

namespace {

// Initial comamnd ID's for navigatable (and hence executable) tab/window menu
// items.  The menumodel and storage structures are not 1-1:
// - menumodel has "Recently closed" header, "No tabs from other devices",
//   device section headers, separators, local and other devices' tab items, and
//   local window items.
// - |local_tab_navigation_items_| and |other_devices_tab_navigation_items_|
// only have navigatabale/executable tab items.
// - |local_window_items_| only has executable open window items.
// Using initial command IDs for local tab, local window and other devices' tab
// items makes it easier and less error-prone to manipulate the menumodel and
// storage structures.  These ids must be bigger than the maximum possible
// number of items in the menumodel, so that index of the last menu item doesn't
// clash with these values when menu items are retrieved via
// GetIndexOfCommandId().
// The range of all command ID's used in RecentTabsSubMenuModel, including the
// "Recently closed" headers, must be between
// |WrenchMenuModel::kMinRecentTabsCommandId| i.e. 1001 and 1200
// (|WrenchMenuModel::kMaxRecentTabsCommandId|) inclusively.
const int kFirstLocalTabCommandId = WrenchMenuModel::kMinRecentTabsCommandId;
const int kFirstLocalWindowCommandId = 1031;
const int kFirstOtherDevicesTabCommandId = 1051;
const int kMinDeviceNameCommandId = 1100;
const int kMaxDeviceNameCommandId = 1110;

// The maximum number of local recently closed entries (tab or window) to be
// shown in the menu.
const int kMaxLocalEntries = 8;

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

// Returns true if the command id identifies a tab menu item.
bool IsTabModelCommandId(int command_id) {
  return ((command_id >= kFirstLocalTabCommandId &&
           command_id < kFirstLocalWindowCommandId) ||
          (command_id >= kFirstOtherDevicesTabCommandId &&
           command_id < kMinDeviceNameCommandId));
}

// Returns true if the command id identifies a window menu item.
bool IsWindowModelCommandId(int command_id) {
  return command_id >= kFirstLocalWindowCommandId &&
         command_id < kFirstOtherDevicesTabCommandId;
}

bool IsDeviceNameCommandId(int command_id) {
  return command_id >= kMinDeviceNameCommandId &&
      command_id <= kMaxDeviceNameCommandId;
}

// Convert |tab_vector_index| to command id of menu item, with
// |first_command_id| as the base command id.
int TabVectorIndexToCommandId(int tab_vector_index, int first_command_id) {
  int command_id = tab_vector_index + first_command_id;
  DCHECK(IsTabModelCommandId(command_id));
  return command_id;
}

// Convert |window_vector_index| to command id of menu item.
int WindowVectorIndexToCommandId(int window_vector_index) {
  int command_id = window_vector_index + kFirstLocalWindowCommandId;
  DCHECK(IsWindowModelCommandId(command_id));
  return command_id;
}

// Convert |command_id| of menu item to index in |local_window_items_|.
int CommandIdToWindowVectorIndex(int command_id) {
  DCHECK(IsWindowModelCommandId(command_id));
  return command_id - kFirstLocalWindowCommandId;
}

}  // namespace

enum RecentTabAction {
  LOCAL_SESSION_TAB = 0,
  OTHER_DEVICE_TAB,
  RESTORE_WINDOW,
  SHOW_MORE,
  LIMIT_RECENT_TAB_ACTION
};

// An element in |RecentTabsSubMenuModel::local_tab_navigation_items_| or
// |RecentTabsSubMenuModel::other_devices_tab_navigation_items_| that stores
// the navigation information of a local or other devices' tab required to
// restore the tab.
struct RecentTabsSubMenuModel::TabNavigationItem {
  TabNavigationItem() : tab_id(-1) {}

  TabNavigationItem(const std::string& session_tag,
                    const SessionID::id_type& tab_id,
                    const base::string16& title,
                    const GURL& url)
      : session_tag(session_tag),
        tab_id(tab_id),
        title(title),
        url(url) {}

  // For use by std::set for sorting.
  bool operator<(const TabNavigationItem& other) const {
    return url < other.url;
  }

  // Empty for local tabs, non-empty for other devices' tabs.
  std::string session_tag;
  SessionID::id_type tab_id;  // -1 for invalid, >= 0 otherwise.
  base::string16 title;
  GURL url;
};

const int RecentTabsSubMenuModel::kRecentlyClosedHeaderCommandId = 1120;
const int RecentTabsSubMenuModel::kDisabledRecentlyClosedHeaderCommandId = 1121;

RecentTabsSubMenuModel::RecentTabsSubMenuModel(
    ui::AcceleratorProvider* accelerator_provider,
    Browser* browser,
    browser_sync::OpenTabsUIDelegate* open_tabs_delegate)
    : ui::SimpleMenuModel(this),
      browser_(browser),
      open_tabs_delegate_(open_tabs_delegate),
      last_local_model_index_(-1),
      default_favicon_(ui::ResourceBundle::GetSharedInstance().
                       GetNativeImageNamed(IDR_DEFAULT_FAVICON)),
      weak_ptr_factory_(this) {
  // Invoke asynchronous call to load tabs from local last session, which does
  // nothing if the tabs have already been loaded or they shouldn't be loaded.
  // TabRestoreServiceChanged() will be called after the tabs are loaded.
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());
  if (service) {
    service->LoadTabsFromLastSession();

  // TODO(sail): enable this when mac implements the dynamic menu, together with
  // MenuModelDelegate::MenuStructureChanged().
#if !defined(OS_MACOSX)
    service->AddObserver(this);
#endif
  }

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
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());
  if (service)
    service->RemoveObserver(this);
}

bool RecentTabsSubMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool RecentTabsSubMenuModel::IsCommandIdEnabled(int command_id) const {
  if (command_id == kRecentlyClosedHeaderCommandId ||
      command_id == kDisabledRecentlyClosedHeaderCommandId ||
      command_id == IDC_RECENT_TABS_NO_DEVICE_TABS ||
      IsDeviceNameCommandId(command_id)) {
    return false;
  }
  return true;
}

bool RecentTabsSubMenuModel::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  // If there are no recently closed items, we show the accelerator beside
  // the header, otherwise, we show it beside the first item underneath it.
  int index_in_menu = GetIndexOfCommandId(command_id);
  int header_index = GetIndexOfCommandId(kRecentlyClosedHeaderCommandId);
  if ((command_id == kDisabledRecentlyClosedHeaderCommandId ||
       (header_index != -1 && index_in_menu == header_index + 1)) &&
      reopen_closed_tab_accelerator_.key_code() != ui::VKEY_UNKNOWN) {
    *accelerator = reopen_closed_tab_accelerator_;
    return true;
  }
  return false;
}

void RecentTabsSubMenuModel::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == IDC_SHOW_HISTORY) {
    UMA_HISTOGRAM_ENUMERATION("WrenchMenu.RecentTabsSubMenu", SHOW_MORE,
                              LIMIT_RECENT_TAB_ACTION);
    // We show all "other devices" on the history page.
    chrome::ExecuteCommandWithDisposition(browser_, IDC_SHOW_HISTORY,
        ui::DispositionFromEventFlags(event_flags));
    return;
  }

  DCHECK_NE(IDC_RECENT_TABS_NO_DEVICE_TABS, command_id);
  DCHECK(!IsDeviceNameCommandId(command_id));

  WindowOpenDisposition disposition =
      ui::DispositionFromEventFlags(event_flags);
  if (disposition == CURRENT_TAB)  // Force to open a new foreground tab.
    disposition = NEW_FOREGROUND_TAB;

  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());
  TabRestoreServiceDelegate* delegate =
      TabRestoreServiceDelegate::FindDelegateForWebContents(
          browser_->tab_strip_model()->GetActiveWebContents());
  if (IsTabModelCommandId(command_id)) {
    TabNavigationItems* tab_items = NULL;
    int tab_items_idx = CommandIdToTabVectorIndex(command_id, &tab_items);
    const TabNavigationItem& item = (*tab_items)[tab_items_idx];
    DCHECK(item.tab_id > -1 && item.url.is_valid());

    if (item.session_tag.empty()) {  // Restore tab of local session.
      if (service && delegate) {
        UMA_HISTOGRAM_ENUMERATION("WrenchMenu.RecentTabsSubMenu",
                                  LOCAL_SESSION_TAB, LIMIT_RECENT_TAB_ACTION);
        service->RestoreEntryById(delegate, item.tab_id,
                                  browser_->host_desktop_type(), disposition);
      }
    } else {  // Restore tab of session from other devices.
      browser_sync::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate();
      if (!open_tabs)
        return;
      const SessionTab* tab;
      if (!open_tabs->GetForeignTab(item.session_tag, item.tab_id, &tab))
        return;
      if (tab->navigations.empty())
        return;
      UMA_HISTOGRAM_ENUMERATION("WrenchMenu.RecentTabsSubMenu",
                                OTHER_DEVICE_TAB, LIMIT_RECENT_TAB_ACTION);
      SessionRestore::RestoreForeignSessionTab(
          browser_->tab_strip_model()->GetActiveWebContents(),
          *tab, disposition);
    }
  } else {
    DCHECK(IsWindowModelCommandId(command_id));
    if (service && delegate) {
      int window_items_idx = CommandIdToWindowVectorIndex(command_id);
      DCHECK(window_items_idx >= 0 &&
             window_items_idx < static_cast<int>(local_window_items_.size()));
      UMA_HISTOGRAM_ENUMERATION("WrenchMenu.RecentTabsSubMenu", RESTORE_WINDOW,
                                LIMIT_RECENT_TAB_ACTION);
      service->RestoreEntryById(delegate, local_window_items_[window_items_idx],
                                browser_->host_desktop_type(), disposition);
    }
  }
}

int RecentTabsSubMenuModel::GetFirstRecentTabsCommandId() {
  return WindowVectorIndexToCommandId(0);
}

const gfx::FontList* RecentTabsSubMenuModel::GetLabelFontListAt(
    int index) const {
  int command_id = GetCommandIdAt(index);
  if (command_id == kRecentlyClosedHeaderCommandId ||
      IsDeviceNameCommandId(command_id)) {
    return &ui::ResourceBundle::GetSharedInstance().GetFontList(
        ui::ResourceBundle::BoldFont);
  }
  return NULL;
}

int RecentTabsSubMenuModel::GetMaxWidthForItemAtIndex(int item_index) const {
  int command_id = GetCommandIdAt(item_index);
  if (command_id == IDC_RECENT_TABS_NO_DEVICE_TABS ||
      command_id == kRecentlyClosedHeaderCommandId ||
      command_id == kDisabledRecentlyClosedHeaderCommandId) {
    return -1;
  }
  return 320;
}

bool RecentTabsSubMenuModel::GetURLAndTitleForItemAtIndex(
    int index,
    std::string* url,
    base::string16* title) {
  int command_id = GetCommandIdAt(index);
  if (IsTabModelCommandId(command_id)) {
    TabNavigationItems* tab_items = NULL;
    int tab_items_idx = CommandIdToTabVectorIndex(command_id, &tab_items);
    const TabNavigationItem& item = (*tab_items)[tab_items_idx];
    *url = item.url.possibly_invalid_spec();
    *title = item.title;
    return true;
  }
  return false;
}

void RecentTabsSubMenuModel::Build() {
  // The menu contains:
  // - Recently closed header, then list of local recently closed tabs/windows,
  //   then separator
  // - device 1 section header, then list of tabs from device, then separator
  // - device 2 section header, then list of tabs from device, then separator
  // - device 3 section header, then list of tabs from device, then separator
  // - More... to open the history tab to get more other devices.
  // |local_tab_navigation_items_| and |other_devices_tab_navigation_items_|
  // only contain navigatable (and hence executable) tab items for local
  // recently closed tabs and tabs from other devices respectively.
  // |local_window_items_| contains the local recently closed windows.
  BuildLocalEntries();
  BuildTabsFromOtherDevices();
}

void RecentTabsSubMenuModel::BuildLocalEntries() {
  // All local items use InsertItem*At() to append or insert a menu item.
  // We're appending if building the entries for the first time i.e. invoked
  // from Constructor(), inserting when local entries change subsequently i.e.
  // invoked from TabRestoreServiceChanged().

  DCHECK_EQ(last_local_model_index_, -1);

  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());
  if (!service || service->entries().size() == 0) {
    // This is to show a disabled restore tab entry with the accelerator to
    // teach users about this command.
    InsertItemWithStringIdAt(++last_local_model_index_,
                             kDisabledRecentlyClosedHeaderCommandId,
                             IDS_NEW_TAB_RECENTLY_CLOSED);
  } else {
    InsertItemWithStringIdAt(++last_local_model_index_,
                             kRecentlyClosedHeaderCommandId,
                             IDS_NEW_TAB_RECENTLY_CLOSED);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    SetIcon(last_local_model_index_,
            rb.GetNativeImageNamed(IDR_RECENTLY_CLOSED_WINDOW));

    int added_count = 0;
    TabRestoreService::Entries entries = service->entries();
    for (TabRestoreService::Entries::const_iterator it = entries.begin();
         it != entries.end() && added_count < kMaxLocalEntries; ++it) {
      TabRestoreService::Entry* entry = *it;
      if (entry->type == TabRestoreService::TAB) {
        TabRestoreService::Tab* tab =
            static_cast<TabRestoreService::Tab*>(entry);
        const sessions::SerializedNavigationEntry& current_navigation =
            tab->navigations.at(tab->current_navigation_index);
        BuildLocalTabItem(
            entry->id,
            current_navigation.title(),
            current_navigation.virtual_url(),
            ++last_local_model_index_);
      } else  {
        DCHECK_EQ(entry->type, TabRestoreService::WINDOW);
        BuildLocalWindowItem(
            entry->id,
            static_cast<TabRestoreService::Window*>(entry)->tabs.size(),
            ++last_local_model_index_);
      }
      ++added_count;
    }
  }

  DCHECK_GE(last_local_model_index_, 0);
}

void RecentTabsSubMenuModel::BuildTabsFromOtherDevices() {
  // All other devices' items (device headers or tabs) use AddItem*() to append
  // a menu item, because they are always only built once (i.e. invoked from
  // Constructor()) and don't change after that.

  browser_sync::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate();
  std::vector<const browser_sync::SyncedSession*> sessions;
  if (!open_tabs || !open_tabs->GetAllForeignSessions(&sessions)) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(IDC_RECENT_TABS_NO_DEVICE_TABS,
                        IDS_RECENT_TABS_NO_DEVICE_TABS);
    return;
  }

  // Sort sessions from most recent to least recent.
  std::sort(sessions.begin(), sessions.end(), SortSessionsByRecency);

  const size_t kMaxSessionsToShow = 3;
  size_t num_sessions_added = 0;
  for (size_t i = 0;
       i < sessions.size() && num_sessions_added < kMaxSessionsToShow; ++i) {
    const browser_sync::SyncedSession* session = sessions[i];
    const std::string& session_tag = session->session_tag;

    // Get windows of session.
    std::vector<const SessionWindow*> windows;
    if (!open_tabs->GetForeignSession(session_tag, &windows) ||
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
        const sessions::SerializedNavigationEntry& current_navigation =
            tab->navigations.at(tab->normalized_navigation_index());
        if (chrome::IsNTPURL(current_navigation.virtual_url(),
                             browser_->profile())) {
          continue;
        }
        tabs_in_session.push_back(tab);
      }
    }
    if (tabs_in_session.empty())
      continue;
    std::sort(tabs_in_session.begin(), tabs_in_session.end(),
              SortTabsByRecency);

    // Add the header for the device session.
    DCHECK(!session->session_name.empty());
    AddSeparator(ui::NORMAL_SEPARATOR);
    int command_id = kMinDeviceNameCommandId + i;
    DCHECK_LE(command_id, kMaxDeviceNameCommandId);
    AddItem(command_id, base::UTF8ToUTF16(session->session_name));
    AddDeviceFavicon(GetItemCount() - 1, session->device_type);

    // Build tab menu items from sorted session tabs.
    const size_t kMaxTabsPerSessionToShow = 4;
    for (size_t k = 0;
         k < std::min(tabs_in_session.size(), kMaxTabsPerSessionToShow);
         ++k) {
      BuildOtherDevicesTabItem(session_tag, *tabs_in_session[k]);
    }  // for all tabs in one session

    ++num_sessions_added;
  }  // for all sessions

  // We are not supposed to get here unless at least some items were added.
  DCHECK_GT(GetItemCount(), 0);
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(IDC_SHOW_HISTORY, IDS_RECENT_TABS_MORE);
}

void RecentTabsSubMenuModel::BuildLocalTabItem(int session_id,
                                               const base::string16& title,
                                               const GURL& url,
                                               int curr_model_index) {
  TabNavigationItem item(std::string(), session_id, title, url);
  int command_id = TabVectorIndexToCommandId(
      local_tab_navigation_items_.size(), kFirstLocalTabCommandId);
  // See comments in BuildLocalEntries() about usage of InsertItem*At().
  // There may be no tab title, in which case, use the url as tab title.
  InsertItemAt(curr_model_index, command_id,
               title.empty() ? base::UTF8ToUTF16(item.url.spec()) : title);
  AddTabFavicon(command_id, item.url);
  local_tab_navigation_items_.push_back(item);
}

void RecentTabsSubMenuModel::BuildLocalWindowItem(
    const SessionID::id_type& window_id,
    int num_tabs,
    int curr_model_index) {
  int command_id = WindowVectorIndexToCommandId(local_window_items_.size());
  // See comments in BuildLocalEntries() about usage of InsertItem*At().
  if (num_tabs == 1) {
    InsertItemWithStringIdAt(curr_model_index, command_id,
                             IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_SINGLE);
  } else {
    InsertItemAt(curr_model_index, command_id, l10n_util::GetStringFUTF16(
        IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_MULTIPLE,
        base::IntToString16(num_tabs)));
  }
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SetIcon(curr_model_index, rb.GetNativeImageNamed(IDR_RECENTLY_CLOSED_WINDOW));
  local_window_items_.push_back(window_id);
}

void RecentTabsSubMenuModel::BuildOtherDevicesTabItem(
    const std::string& session_tag,
    const SessionTab& tab) {
  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(tab.normalized_navigation_index());
  TabNavigationItem item(session_tag, tab.tab_id.id(),
                         current_navigation.title(),
                         current_navigation.virtual_url());
  int command_id = TabVectorIndexToCommandId(
      other_devices_tab_navigation_items_.size(),
      kFirstOtherDevicesTabCommandId);
  // See comments in BuildTabsFromOtherDevices() about usage of AddItem*().
  // There may be no tab title, in which case, use the url as tab title.
  AddItem(command_id,
          current_navigation.title().empty() ?
              base::UTF8ToUTF16(item.url.spec()) : current_navigation.title());
  AddTabFavicon(command_id, item.url);
  other_devices_tab_navigation_items_.push_back(item);
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
  }

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SetIcon(index_in_menu, rb.GetNativeImageNamed(favicon_id));
}

void RecentTabsSubMenuModel::AddTabFavicon(int command_id, const GURL& url) {
  bool is_local_tab = command_id < kFirstOtherDevicesTabCommandId;
  int index_in_menu = GetIndexOfCommandId(command_id);

  if (!is_local_tab) {
    // If tab has synced favicon, use it.
    // Note that currently, other devices' tabs only have favicons if
    // --sync-tab-favicons switch is on; according to zea@, this flag is now
    // automatically enabled for iOS and android, and they're looking into
    // enabling it for other platforms.
    browser_sync::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate();
    scoped_refptr<base::RefCountedMemory> favicon_png;
    if (open_tabs &&
        open_tabs->GetSyncedFaviconForPageURL(url.spec(), &favicon_png)) {
      gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(favicon_png);
      SetIcon(index_in_menu, image);
      return;
    }
  }

  // Otherwise, start to fetch the favicon from local history asynchronously.
  // Set default icon first.
  SetIcon(index_in_menu, default_favicon_);
  // Start request to fetch actual icon if possible.
  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      browser_->profile(), Profile::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;

  favicon_service->GetFaviconImageForPageURL(
      url,
      base::Bind(&RecentTabsSubMenuModel::OnFaviconDataAvailable,
                 weak_ptr_factory_.GetWeakPtr(),
                 command_id),
      is_local_tab ? &local_tab_cancelable_task_tracker_
                   : &other_devices_tab_cancelable_task_tracker_);
}

void RecentTabsSubMenuModel::OnFaviconDataAvailable(
    int command_id,
    const favicon_base::FaviconImageResult& image_result) {
  if (image_result.image.IsEmpty())
    return;
  int index_in_menu = GetIndexOfCommandId(command_id);
  DCHECK_GT(index_in_menu, -1);
  SetIcon(index_in_menu, image_result.image);
  ui::MenuModelDelegate* menu_model_delegate = GetMenuModelDelegate();
  if (menu_model_delegate)
    menu_model_delegate->OnIconChanged(index_in_menu);
}

int RecentTabsSubMenuModel::CommandIdToTabVectorIndex(
    int command_id,
    TabNavigationItems** tab_items) {
  DCHECK(IsTabModelCommandId(command_id));
  if (command_id >= kFirstOtherDevicesTabCommandId) {
    *tab_items = &other_devices_tab_navigation_items_;
    return command_id - kFirstOtherDevicesTabCommandId;
  }
  *tab_items = &local_tab_navigation_items_;
  return command_id - kFirstLocalTabCommandId;
}

void RecentTabsSubMenuModel::ClearLocalEntries() {
  // Remove local items (recently closed tabs and windows) from menumodel.
  while (last_local_model_index_ >= 0)
    RemoveItemAt(last_local_model_index_--);

  // Cancel asynchronous FaviconService::GetFaviconImageForPageURL() tasks of
  // all
  // local tabs.
  local_tab_cancelable_task_tracker_.TryCancelAll();

  // Remove all local tab navigation items.
  local_tab_navigation_items_.clear();

  // Remove all local window items.
  local_window_items_.clear();
}

browser_sync::OpenTabsUIDelegate*
    RecentTabsSubMenuModel::GetOpenTabsUIDelegate() {
  if (!open_tabs_delegate_) {
    ProfileSyncService* service = ProfileSyncServiceFactory::GetInstance()->
        GetForProfile(browser_->profile());
    // Only return the delegate if it exists and it is done syncing sessions.
    if (service && service->ShouldPushChanges())
      open_tabs_delegate_ = service->GetOpenTabsUIDelegate();
  }
  return open_tabs_delegate_;
}

void RecentTabsSubMenuModel::TabRestoreServiceChanged(
    TabRestoreService* service) {
  ClearLocalEntries();

  BuildLocalEntries();

  ui::MenuModelDelegate* menu_model_delegate = GetMenuModelDelegate();
  if (menu_model_delegate)
    menu_model_delegate->OnMenuStructureChanged();
}

void RecentTabsSubMenuModel::TabRestoreServiceDestroyed(
    TabRestoreService* service) {
  TabRestoreServiceChanged(service);
}
