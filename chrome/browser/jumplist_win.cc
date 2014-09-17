// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/jumplist_win.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/page_usage_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

// Append the common switches to each shell link.
void AppendCommonSwitches(ShellLinkItem* shell_link) {
  const char* kSwitchNames[] = { switches::kUserDataDir };
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  shell_link->GetCommandLine()->CopySwitchesFrom(command_line,
                                                 kSwitchNames,
                                                 arraysize(kSwitchNames));
}

// Create a ShellLinkItem preloaded with common switches.
scoped_refptr<ShellLinkItem> CreateShellLink() {
  scoped_refptr<ShellLinkItem> link(new ShellLinkItem);
  AppendCommonSwitches(link.get());
  return link;
}

// Creates a temporary icon file to be shown in JumpList.
bool CreateIconFile(const SkBitmap& bitmap,
                    const base::FilePath& icon_dir,
                    base::FilePath* icon_path) {
  // Retrieve the path to a temporary file.
  // We don't have to care about the extension of this temporary file because
  // JumpList does not care about it.
  base::FilePath path;
  if (!base::CreateTemporaryFileInDir(icon_dir, &path))
    return false;

  // Create an icon file from the favicon attached to the given |page|, and
  // save it as the temporary file.
  gfx::ImageFamily image_family;
  image_family.Add(gfx::Image::CreateFrom1xBitmap(bitmap));
  if (!IconUtil::CreateIconFileFromImageFamily(image_family, path))
    return false;

  // Add this icon file to the list and return its absolute path.
  // The IShellLink::SetIcon() function needs the absolute path to an icon.
  *icon_path = path;
  return true;
}

// Updates the "Tasks" category of the JumpList.
bool UpdateTaskCategory(
    JumpListUpdater* jumplist_updater,
    IncognitoModePrefs::Availability incognito_availability) {
  base::FilePath chrome_path;
  if (!PathService::Get(base::FILE_EXE, &chrome_path))
    return false;

  ShellLinkItemList items;

  // Create an IShellLink object which launches Chrome, and add it to the
  // collection. We use our application icon as the icon for this item.
  // We remove '&' characters from this string so we can share it with our
  // system menu.
  if (incognito_availability != IncognitoModePrefs::FORCED) {
    scoped_refptr<ShellLinkItem> chrome = CreateShellLink();
    base::string16 chrome_title = l10n_util::GetStringUTF16(IDS_NEW_WINDOW);
    ReplaceSubstringsAfterOffset(&chrome_title, 0, L"&", L"");
    chrome->set_title(chrome_title);
    chrome->set_icon(chrome_path.value(), 0);
    items.push_back(chrome);
  }

  // Create an IShellLink object which launches Chrome in incognito mode, and
  // add it to the collection. We use our application icon as the icon for
  // this item.
  if (incognito_availability != IncognitoModePrefs::DISABLED) {
    scoped_refptr<ShellLinkItem> incognito = CreateShellLink();
    incognito->GetCommandLine()->AppendSwitch(switches::kIncognito);
    base::string16 incognito_title =
        l10n_util::GetStringUTF16(IDS_NEW_INCOGNITO_WINDOW);
    ReplaceSubstringsAfterOffset(&incognito_title, 0, L"&", L"");
    incognito->set_title(incognito_title);
    incognito->set_icon(chrome_path.value(), 0);
    items.push_back(incognito);
  }

  return jumplist_updater->AddTasks(items);
}

// Updates the application JumpList.
bool UpdateJumpList(const wchar_t* app_id,
                    const ShellLinkItemList& most_visited_pages,
                    const ShellLinkItemList& recently_closed_pages,
                    IncognitoModePrefs::Availability incognito_availability) {
  // JumpList is implemented only on Windows 7 or later.
  // So, we should return now when this function is called on earlier versions
  // of Windows.
  if (!JumpListUpdater::IsEnabled())
    return true;

  JumpListUpdater jumplist_updater(app_id);
  if (!jumplist_updater.BeginUpdate())
    return false;

  // We allocate 60% of the given JumpList slots to "most-visited" items
  // and 40% to "recently-closed" items, respectively.
  // Nevertheless, if there are not so many items in |recently_closed_pages|,
  // we give the remaining slots to "most-visited" items.
  const int kMostVisited = 60;
  const int kRecentlyClosed = 40;
  const int kTotal = kMostVisited + kRecentlyClosed;
  size_t most_visited_items =
      MulDiv(jumplist_updater.user_max_items(), kMostVisited, kTotal);
  size_t recently_closed_items =
      jumplist_updater.user_max_items() - most_visited_items;
  if (recently_closed_pages.size() < recently_closed_items) {
    most_visited_items += recently_closed_items - recently_closed_pages.size();
    recently_closed_items = recently_closed_pages.size();
  }

  // Update the "Most Visited" category of the JumpList.
  // This update request is applied into the JumpList when we commit this
  // transaction.
  if (!jumplist_updater.AddCustomCategory(
          base::UTF16ToWide(
              l10n_util::GetStringUTF16(IDS_NEW_TAB_MOST_VISITED)),
          most_visited_pages, most_visited_items)) {
    return false;
  }

  // Update the "Recently Closed" category of the JumpList.
  if (!jumplist_updater.AddCustomCategory(
          base::UTF16ToWide(
              l10n_util::GetStringUTF16(IDS_NEW_TAB_RECENTLY_CLOSED)),
          recently_closed_pages, recently_closed_items)) {
    return false;
  }

  // Update the "Tasks" category of the JumpList.
  if (!UpdateTaskCategory(&jumplist_updater, incognito_availability))
    return false;

  // Commit this transaction and send the updated JumpList to Windows.
  if (!jumplist_updater.CommitUpdate())
    return false;

  return true;
}

}  // namespace

JumpList::JumpList(Profile* profile)
    : profile_(profile),
      task_id_(base::CancelableTaskTracker::kBadTaskId),
      weak_ptr_factory_(this) {
  DCHECK(Enabled());
  // To update JumpList when a tab is added or removed, we add this object to
  // the observer list of the TabRestoreService class.
  // When we add this object to the observer list, we save the pointer to this
  // TabRestoreService object. This pointer is used when we remove this object
  // from the observer list.
  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile_);
  if (!tab_restore_service)
    return;

  app_id_ = ShellIntegration::GetChromiumModelIdForProfile(profile_->GetPath());
  icon_dir_ = profile_->GetPath().Append(chrome::kJumpListIconDirname);
  history::TopSites* top_sites = profile_->GetTopSites();
  if (top_sites) {
    // TopSites updates itself after a delay. This is especially noticable when
    // your profile is empty. Ask TopSites to update itself when jumplist is
    // initialized.
    top_sites->SyncWithHistory();
    registrar_.reset(new content::NotificationRegistrar);
    // Register for notification when TopSites changes so that we can update
    // ourself.
    registrar_->Add(this, chrome::NOTIFICATION_TOP_SITES_CHANGED,
                    content::Source<history::TopSites>(top_sites));
    // Register for notification when profile is destroyed to ensure that all
    // observers are detatched at that time.
    registrar_->Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                    content::Source<Profile>(profile_));
  }
  tab_restore_service->AddObserver(this);
  pref_change_registrar_.reset(new PrefChangeRegistrar);
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kIncognitoModeAvailability,
      base::Bind(&JumpList::OnIncognitoAvailabilityChanged, this));
}

JumpList::~JumpList() {
  Terminate();
}

// static
bool JumpList::Enabled() {
  return JumpListUpdater::IsEnabled();
}

void JumpList::Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_TOP_SITES_CHANGED: {
      // Most visited urls changed, query again.
      history::TopSites* top_sites = profile_->GetTopSites();
      if (top_sites) {
        top_sites->GetMostVisitedURLs(
            base::Bind(&JumpList::OnMostVisitedURLsAvailable,
                       weak_ptr_factory_.GetWeakPtr()), false);
      }
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      // Profile was destroyed, do clean-up.
      Terminate();
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification type.";
  }
}

void JumpList::CancelPendingUpdate() {
  if (task_id_ != base::CancelableTaskTracker::kBadTaskId) {
    cancelable_task_tracker_.TryCancel(task_id_);
    task_id_ = base::CancelableTaskTracker::kBadTaskId;
  }
}

void JumpList::Terminate() {
  CancelPendingUpdate();
  if (profile_) {
    TabRestoreService* tab_restore_service =
        TabRestoreServiceFactory::GetForProfile(profile_);
    if (tab_restore_service)
      tab_restore_service->RemoveObserver(this);
    registrar_.reset();
    pref_change_registrar_.reset();
  }
  profile_ = NULL;
}

void JumpList::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& data) {

  // If we have a pending favicon request, cancel it here (it is out of date).
  CancelPendingUpdate();

  {
    base::AutoLock auto_lock(list_lock_);
    most_visited_pages_.clear();
    for (size_t i = 0; i < data.size(); i++) {
      const history::MostVisitedURL& url = data[i];
      scoped_refptr<ShellLinkItem> link = CreateShellLink();
      std::string url_string = url.url.spec();
      std::wstring url_string_wide = base::UTF8ToWide(url_string);
      link->GetCommandLine()->AppendArgNative(url_string_wide);
      link->set_title(!url.title.empty()? url.title : url_string_wide);
      most_visited_pages_.push_back(link);
      icon_urls_.push_back(make_pair(url_string, link));
    }
  }

  // Send a query that retrieves the first favicon.
  StartLoadingFavicon();
}

void JumpList::TabRestoreServiceChanged(TabRestoreService* service) {
  // if we have a pending handle request, cancel it here (it is out of date).
  CancelPendingUpdate();

  // local list to pass to methods
  ShellLinkItemList temp_list;

  // Create a list of ShellLinkItems from the "Recently Closed" pages.
  // As noted above, we create a ShellLinkItem objects with the following
  // parameters.
  // * arguments
  //   The last URL of the tab object.
  // * title
  //   The title of the last URL.
  // * icon
  //   An empty string. This value is to be updated in OnFaviconDataAvailable().
  // This code is copied from
  // RecentlyClosedTabsHandler::TabRestoreServiceChanged() to emulate it.
  const int kRecentlyClosedCount = 4;
  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile_);
  const TabRestoreService::Entries& entries = tab_restore_service->entries();
  for (TabRestoreService::Entries::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    const TabRestoreService::Entry* entry = *it;
    if (entry->type == TabRestoreService::TAB) {
      AddTab(static_cast<const TabRestoreService::Tab*>(entry),
             &temp_list, kRecentlyClosedCount);
    } else if (entry->type == TabRestoreService::WINDOW) {
      AddWindow(static_cast<const TabRestoreService::Window*>(entry),
                &temp_list, kRecentlyClosedCount);
    }
  }
  // Lock recently_closed_pages and copy temp_list into it.
  {
    base::AutoLock auto_lock(list_lock_);
    recently_closed_pages_ = temp_list;
  }

  // Send a query that retrieves the first favicon.
  StartLoadingFavicon();
}

void JumpList::TabRestoreServiceDestroyed(TabRestoreService* service) {
}

bool JumpList::AddTab(const TabRestoreService::Tab* tab,
                      ShellLinkItemList* list,
                      size_t max_items) {
  // This code adds the URL and the title strings of the given tab to the
  // specified list.
  if (list->size() >= max_items)
    return false;

  scoped_refptr<ShellLinkItem> link = CreateShellLink();
  const sessions::SerializedNavigationEntry& current_navigation =
      tab->navigations.at(tab->current_navigation_index);
  std::string url = current_navigation.virtual_url().spec();
  link->GetCommandLine()->AppendArgNative(base::UTF8ToWide(url));
  link->set_title(current_navigation.title());
  list->push_back(link);
  icon_urls_.push_back(make_pair(url, link));
  return true;
}

void JumpList::AddWindow(const TabRestoreService::Window* window,
                         ShellLinkItemList* list,
                         size_t max_items) {
  // This code enumerates al the tabs in the given window object and add their
  // URLs and titles to the list.
  DCHECK(!window->tabs.empty());

  for (size_t i = 0; i < window->tabs.size(); ++i) {
    if (!AddTab(&window->tabs[i], list, max_items))
      return;
  }
}

void JumpList::StartLoadingFavicon() {
  GURL url;
  bool waiting_for_icons = true;
  {
    base::AutoLock auto_lock(list_lock_);
    waiting_for_icons = !icon_urls_.empty();
    if (waiting_for_icons) {
      // Ask FaviconService if it has a favicon of a URL.
      // When FaviconService has one, it will call OnFaviconDataAvailable().
      url = GURL(icon_urls_.front().first);
    }
  }

  if (!waiting_for_icons) {
    // No more favicons are needed by the application JumpList. Schedule a
    // RunUpdate call.
    PostRunUpdate();
    return;
  }

  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  task_id_ = favicon_service->GetFaviconImageForPageURL(
      url,
      base::Bind(&JumpList::OnFaviconDataAvailable, base::Unretained(this)),
      &cancelable_task_tracker_);
}

void JumpList::OnFaviconDataAvailable(
    const favicon_base::FaviconImageResult& image_result) {
  // If there is currently a favicon request in progress, it is now outdated,
  // as we have received another, so nullify the handle from the old request.
  task_id_ = base::CancelableTaskTracker::kBadTaskId;
  // lock the list to set icon data and pop the url
  {
    base::AutoLock auto_lock(list_lock_);
    // Attach the received data to the ShellLinkItem object.
    // This data will be decoded by the RunUpdate method.
    if (!image_result.image.IsEmpty()) {
      if (!icon_urls_.empty() && icon_urls_.front().second)
        icon_urls_.front().second->set_icon_data(image_result.image.AsBitmap());
    }

    if (!icon_urls_.empty())
      icon_urls_.pop_front();
  }
  // Check whether we need to load more favicons.
  StartLoadingFavicon();
}

void JumpList::OnIncognitoAvailabilityChanged() {
  bool waiting_for_icons = true;
  {
    base::AutoLock auto_lock(list_lock_);
    waiting_for_icons = !icon_urls_.empty();
  }
  if (!waiting_for_icons)
    PostRunUpdate();
  // If |icon_urls_| isn't empty then OnFaviconDataAvailable will eventually
  // call PostRunUpdate().
}

void JumpList::PostRunUpdate() {
  // Check if incognito windows (or normal windows) are disabled by policy.
  IncognitoModePrefs::Availability incognito_availability =
      profile_ ? IncognitoModePrefs::GetAvailability(profile_->GetPrefs())
               : IncognitoModePrefs::ENABLED;

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&JumpList::RunUpdate, this, incognito_availability));
}

void JumpList::RunUpdate(
    IncognitoModePrefs::Availability incognito_availability) {
  ShellLinkItemList local_most_visited_pages;
  ShellLinkItemList local_recently_closed_pages;

  {
    base::AutoLock auto_lock(list_lock_);
    // Make sure we are not out of date: if icon_urls_ is not empty, then
    // another notification has been received since we processed this one
    if (!icon_urls_.empty())
      return;

    // Make local copies of lists so we can release the lock.
    local_most_visited_pages = most_visited_pages_;
    local_recently_closed_pages = recently_closed_pages_;
  }

  // Delete the directory which contains old icon files, rename the current
  // icon directory, and create a new directory which contains new JumpList
  // icon files.
  base::FilePath icon_dir_old(icon_dir_.value() + L"Old");
  if (base::PathExists(icon_dir_old))
    base::DeleteFile(icon_dir_old, true);
  base::Move(icon_dir_, icon_dir_old);
  base::CreateDirectory(icon_dir_);

  // Create temporary icon files for shortcuts in the "Most Visited" category.
  CreateIconFiles(local_most_visited_pages);

  // Create temporary icon files for shortcuts in the "Recently Closed"
  // category.
  CreateIconFiles(local_recently_closed_pages);

  // We finished collecting all resources needed for updating an application
  // JumpList. So, create a new JumpList and replace the current JumpList
  // with it.
  UpdateJumpList(app_id_.c_str(), local_most_visited_pages,
                 local_recently_closed_pages, incognito_availability);
}

void JumpList::CreateIconFiles(const ShellLinkItemList& item_list) {
  for (ShellLinkItemList::const_iterator item = item_list.begin();
      item != item_list.end(); ++item) {
    base::FilePath icon_path;
    if (CreateIconFile((*item)->icon_data(), icon_dir_, &icon_path))
      (*item)->set_icon(icon_path.value(), 0);
  }
}
