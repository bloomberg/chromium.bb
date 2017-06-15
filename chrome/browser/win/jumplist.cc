// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/jumplist.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/thread.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/metrics/jumplist_metrics_win.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/browser/win/jumplist_file_util.h"
#include "chrome/browser/win/jumplist_update_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/install_static/install_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/top_sites.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/session_types.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "url/gurl.h"

namespace {

// The default maximum number of items to display in JumpList is 10.
// https://msdn.microsoft.com/library/windows/desktop/dd378398.aspx
// The "Most visited" and "Recently closed" category titles always take 2 slots.
// For the remaining 8 slots, we allocate 5 slots to "most-visited" items and 3
// slots to "recently-closed" items, respectively.
constexpr size_t kMostVisitedItems = 5;
constexpr size_t kRecentlyClosedItems = 3;

// The number of updates to skip to alleviate the machine when a previous update
// was too slow.
constexpr int kUpdatesToSkipUnderHeavyLoad = 10;

// The delay before updating the JumpList to prevent update storms.
constexpr base::TimeDelta kDelayForJumplistUpdate =
    base::TimeDelta::FromMilliseconds(3500);

// The maximum allowed time for JumpListUpdater::BeginUpdate. Updates taking
// longer than this are discarded to prevent bogging down slow machines.
constexpr base::TimeDelta kTimeOutForJumplistBeginUpdate =
    base::TimeDelta::FromMilliseconds(500);

// The maximum allowed time for updating both "most visited" and "recently
// closed" categories via JumpListUpdater::AddCustomCategory.
constexpr base::TimeDelta kTimeOutForAddCustomCategory =
    base::TimeDelta::FromMilliseconds(500);

// The maximum allowed time for JumpListUpdater::CommitUpdate.
constexpr base::TimeDelta kTimeOutForJumplistCommitUpdate =
    base::TimeDelta::FromMilliseconds(1000);

// Appends the common switches to each shell link.
void AppendCommonSwitches(ShellLinkItem* shell_link) {
  const char* kSwitchNames[] = { switches::kUserDataDir };
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  shell_link->GetCommandLine()->CopySwitchesFrom(command_line,
                                                 kSwitchNames,
                                                 arraysize(kSwitchNames));
}

// Creates a ShellLinkItem preloaded with common switches.
scoped_refptr<ShellLinkItem> CreateShellLink() {
  auto link = base::MakeRefCounted<ShellLinkItem>();
  AppendCommonSwitches(link.get());
  return link;
}

// Creates a temporary icon file to be shown in JumpList.
bool CreateIconFile(const gfx::ImageSkia& image_skia,
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
  if (!image_skia.isNull()) {
    std::vector<float> supported_scales = image_skia.GetSupportedScales();
    for (auto& scale : supported_scales) {
      gfx::ImageSkiaRep image_skia_rep = image_skia.GetRepresentation(scale);
      if (!image_skia_rep.is_null())
        image_family.Add(
            gfx::Image::CreateFrom1xBitmap(image_skia_rep.sk_bitmap()));
    }
  }

  if (!IconUtil::CreateIconFileFromImageFamily(image_family, path,
                                               IconUtil::NORMAL_WRITE)) {
    // Delete the file created by CreateTemporaryFileInDir as it won't be used.
    base::DeleteFile(path, false);
    return false;
  }

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

  int icon_index = install_static::GetIconResourceIndex();

  ShellLinkItemList items;

  // Create an IShellLink object which launches Chrome, and add it to the
  // collection. We use our application icon as the icon for this item.
  // We remove '&' characters from this string so we can share it with our
  // system menu.
  if (incognito_availability != IncognitoModePrefs::FORCED) {
    scoped_refptr<ShellLinkItem> chrome = CreateShellLink();
    base::string16 chrome_title = l10n_util::GetStringUTF16(IDS_NEW_WINDOW);
    base::ReplaceSubstringsAfterOffset(
        &chrome_title, 0, L"&", base::StringPiece16());
    chrome->set_title(chrome_title);
    chrome->set_icon(chrome_path.value(), icon_index);
    items.push_back(chrome);
  }

  // Create an IShellLink object which launches Chrome in incognito mode, and
  // add it to the collection.
  if (incognito_availability != IncognitoModePrefs::DISABLED) {
    scoped_refptr<ShellLinkItem> incognito = CreateShellLink();
    incognito->GetCommandLine()->AppendSwitch(switches::kIncognito);
    base::string16 incognito_title =
        l10n_util::GetStringUTF16(IDS_NEW_INCOGNITO_WINDOW);
    base::ReplaceSubstringsAfterOffset(
        &incognito_title, 0, L"&", base::StringPiece16());
    incognito->set_title(incognito_title);
    incognito->set_icon(chrome_path.value(), icon_resources::kIncognitoIndex);
    items.push_back(incognito);
  }

  return jumplist_updater->AddTasks(items);
}

// Returns the full path of the JumpListIcons[|suffix|] directory in
// |profile_dir|.
base::FilePath GenerateJumplistIconDirName(
    const base::FilePath& profile_dir,
    const base::FilePath::StringPieceType& suffix) {
  base::FilePath::StringType dir_name(chrome::kJumpListIconDirname);
  suffix.AppendToString(&dir_name);
  return profile_dir.Append(dir_name);
}

}  // namespace

JumpList::UpdateResults::UpdateResults() {}

JumpList::UpdateResults::~UpdateResults() {}

JumpList::JumpList(Profile* profile)
    : profile_(profile),
      update_jumplist_task_runner_(base::CreateCOMSTATaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      delete_jumplisticons_task_runner_(
          base::CreateSequencedTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::BACKGROUND,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      weak_ptr_factory_(this) {
  DCHECK(Enabled());
  // To update JumpList when a tab is added or removed, we add this object to
  // the observer list of the TabRestoreService class.
  // When we add this object to the observer list, we save the pointer to this
  // TabRestoreService object. This pointer is used when we remove this object
  // from the observer list.
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile_);
  if (!tab_restore_service)
    return;

  app_id_ =
      shell_integration::win::GetChromiumModelIdForProfile(profile_->GetPath());

  // Register as TopSitesObserver so that we can update ourselves when the
  // TopSites changes. TopSites updates itself after a delay. This is especially
  // noticable when your profile is empty.
  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile_);
  if (top_sites)
    top_sites->AddObserver(this);

  // Register as TabRestoreServiceObserver so that we can update ourselves when
  // recently closed tabs have changes.
  tab_restore_service->AddObserver(this);

  // kIncognitoModeAvailability is monitored for changes on Incognito mode.
  pref_change_registrar_.reset(new PrefChangeRegistrar);
  pref_change_registrar_->Init(profile_->GetPrefs());
  // base::Unretained is safe since |this| is guaranteed to outlive
  // pref_change_registrar_.
  pref_change_registrar_->Add(
      prefs::kIncognitoModeAvailability,
      base::Bind(&JumpList::OnIncognitoAvailabilityChanged,
                 base::Unretained(this)));
}

JumpList::~JumpList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Terminate();
}

// static
bool JumpList::Enabled() {
  return JumpListUpdater::IsEnabled();
}

void JumpList::CancelPendingUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel a pending most-visited URL fetch by invalidating the weak pointer.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Cancel a pending favicon loading by invalidating its task id.
  if (task_id_ != base::CancelableTaskTracker::kBadTaskId) {
    cancelable_task_tracker_.TryCancel(task_id_);
    task_id_ = base::CancelableTaskTracker::kBadTaskId;
  }
}

void JumpList::Terminate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Stop();
  CancelPendingUpdate();
  update_in_progress_ = false;
  if (profile_) {
    sessions::TabRestoreService* tab_restore_service =
        TabRestoreServiceFactory::GetForProfile(profile_);
    if (tab_restore_service)
      tab_restore_service->RemoveObserver(this);
    scoped_refptr<history::TopSites> top_sites =
        TopSitesFactory::GetForProfile(profile_);
    if (top_sites)
      top_sites->RemoveObserver(this);
    pref_change_registrar_.reset();
  }
  profile_ = nullptr;
}

void JumpList::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Terminate();
}

void JumpList::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  top_sites_has_pending_notification_ = false;

  // There is no need to update the JumpList if the top most visited sites in
  // display have not changed.
  if (MostVisitedItemsUnchanged(most_visited_pages_, urls, kMostVisitedItems))
    return;

  most_visited_pages_.clear();

  const size_t num_items = std::min(urls.size(), kMostVisitedItems);
  for (size_t i = 0; i < num_items; ++i) {
    const history::MostVisitedURL& url = urls[i];
    scoped_refptr<ShellLinkItem> link = CreateShellLink();
    std::string url_string = url.url.spec();
    base::string16 url_string_wide = base::UTF8ToUTF16(url_string);
    link->GetCommandLine()->AppendArgNative(url_string_wide);
    link->GetCommandLine()->AppendSwitchASCII(switches::kWinJumplistAction,
                                              jumplist::kMostVisitedCategory);
    link->set_title(!url.title.empty() ? url.title : url_string_wide);
    link->set_url(url_string);
    most_visited_pages_.push_back(link);
    icon_urls_.emplace_back(std::move(url_string), std::move(link));
  }

  most_visited_should_update_ = true;

  // Send a query that retrieves the first favicon.
  StartLoadingFavicon();
}

void JumpList::TabRestoreServiceChanged(sessions::TabRestoreService* service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tab_restore_has_pending_notification_ = true;

  // Postpone handling this notification until a pending update completes.
  if (update_in_progress_)
    return;

  // if we have a pending favicon request, cancel it here as it's out of date.
  CancelPendingUpdate();

  // Initialize the one-shot timer to update the JumpList in a while.
  InitializeTimerForUpdate();
}

void JumpList::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {}

bool JumpList::AddTab(const sessions::TabRestoreService::Tab& tab,
                      size_t max_items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This code adds the URL and the title strings of the given tab to the
  // JumpList variables.
  if (recently_closed_pages_.size() >= max_items)
    return false;

  scoped_refptr<ShellLinkItem> link = CreateShellLink();
  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(tab.current_navigation_index);
  std::string url = current_navigation.virtual_url().spec();
  link->GetCommandLine()->AppendArgNative(base::UTF8ToUTF16(url));
  link->GetCommandLine()->AppendSwitchASCII(switches::kWinJumplistAction,
                                            jumplist::kRecentlyClosedCategory);
  link->set_title(current_navigation.title());
  link->set_url(url);
  recently_closed_pages_.push_back(link);
  icon_urls_.emplace_back(std::move(url), std::move(link));

  return true;
}

void JumpList::AddWindow(const sessions::TabRestoreService::Window& window,
                         size_t max_items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!window.tabs.empty());

  for (const auto& tab : window.tabs) {
    if (!AddTab(*tab, max_items))
      return;
  }
}

void JumpList::StartLoadingFavicon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (icon_urls_.empty()) {
    // No more favicons are needed by the application JumpList. Schedule a
    // RunUpdateJumpList call.
    PostRunUpdate();
    return;
  }

  base::ElapsedTimer timer;

  // Ask FaviconService if it has a favicon of a URL.
  // When FaviconService has one, it will call OnFaviconDataAvailable().
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  // base::Unretained is safe since |this| is guaranteed to outlive
  // cancelable_task_tracker_.
  task_id_ = favicon_service->GetFaviconImageForPageURL(
      GURL(icon_urls_.front().first),
      base::Bind(&JumpList::OnFaviconDataAvailable, base::Unretained(this)),
      &cancelable_task_tracker_);

  // TODO(chengx): Remove the UMA histogram after fixing http://crbug.com/717236
  UMA_HISTOGRAM_TIMES("WinJumplist.StartLoadingFaviconDuration",
                      timer.Elapsed());
}

void JumpList::OnFaviconDataAvailable(
    const favicon_base::FaviconImageResult& image_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ElapsedTimer timer;

  // If there is currently a favicon request in progress, it is now outdated,
  // as we have received another, so nullify the handle from the old request.
  task_id_ = base::CancelableTaskTracker::kBadTaskId;

  // Attach the received data to the ShellLinkItem object. This data will be
  // decoded by the RunUpdateJumpList method.
  if (!icon_urls_.empty()) {
    if (!image_result.image.IsEmpty() && icon_urls_.front().second.get()) {
      gfx::ImageSkia image_skia = image_result.image.AsImageSkia();
      image_skia.EnsureRepsForSupportedScales();
      std::unique_ptr<gfx::ImageSkia> deep_copy(image_skia.DeepCopy());
      icon_urls_.front().second->set_icon_image(*deep_copy);
    }
    icon_urls_.pop_front();
  }

  // TODO(chengx): Remove the UMA histogram after fixing http://crbug.com/717236
  UMA_HISTOGRAM_TIMES("WinJumplist.OnFaviconDataAvailableDuration",
                      timer.Elapsed());

  // Check whether we need to load more favicons.
  StartLoadingFavicon();
}

void JumpList::OnIncognitoAvailabilityChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (icon_urls_.empty())
    PostRunUpdate();
}

void JumpList::PostRunUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT0("browser", "JumpList::PostRunUpdate");

  update_in_progress_ = true;

  base::FilePath profile_dir = profile_->GetPath();

  // Check if incognito windows (or normal windows) are disabled by policy.
  IncognitoModePrefs::Availability incognito_availability =
      IncognitoModePrefs::GetAvailability(profile_->GetPrefs());

  // Make local copies of JumpList member variables and use them for an update.
  ShellLinkItemList local_most_visited_pages = most_visited_pages_;
  ShellLinkItemList local_recently_closed_pages = recently_closed_pages_;

  bool most_visited_should_update = most_visited_should_update_;
  bool recently_closed_should_update = recently_closed_should_update_;

  auto update_results = base::MakeUnique<UpdateResults>();
  update_results->most_visited_icons_in_update = most_visited_icons_;
  update_results->recently_closed_icons_in_update = recently_closed_icons_;

  // Parameter evaluation order is unspecified in C++. Ensure the pointer value
  // is obtained before base::Passed() is called.
  auto* update_results_raw = update_results.get();

  // Post a task to update the JumpList, which consists of 1) create new icons,
  // 2) delete old icons, 3) notify the OS.
  if (!update_jumplist_task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&JumpList::RunUpdateJumpList, app_id_, profile_dir,
                     local_most_visited_pages, local_recently_closed_pages,
                     most_visited_should_update, recently_closed_should_update,
                     incognito_availability, update_results_raw),
          base::Bind(&JumpList::OnRunUpdateCompletion,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(std::move(update_results))))) {
    OnRunUpdateCompletion(base::MakeUnique<UpdateResults>());
  }
}

void JumpList::TopSitesLoaded(history::TopSites* top_sites) {
}

void JumpList::TopSitesChanged(history::TopSites* top_sites,
                               ChangeReason change_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  top_sites_has_pending_notification_ = true;

  // Postpone handling this notification until a pending update completes.
  if (update_in_progress_)
    return;

  // If we have a pending favicon request, cancel it here as it's out of date.
  CancelPendingUpdate();

  // Initialize the one-shot timer to update the JumpList in a while.
  InitializeTimerForUpdate();
}

void JumpList::InitializeTimerForUpdate() {
  if (timer_.IsRunning()) {
    timer_.Reset();
  } else {
    // base::Unretained is safe since |this| is guaranteed to outlive timer_.
    timer_.Start(FROM_HERE, kDelayForJumplistUpdate,
                 base::Bind(&JumpList::OnDelayTimer, base::Unretained(this)));
  }
}

void JumpList::OnDelayTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (updates_to_skip_ > 0) {
    --updates_to_skip_;
    return;
  }

  // Retrieve the recently closed URLs synchronously.
  if (tab_restore_has_pending_notification_) {
    tab_restore_has_pending_notification_ = false;
    ProcessTabRestoreServiceNotification();
  }

  // If TopSites has updates, retrieve the URLs asynchronously, and on its
  // completion, trigger favicon loading.
  // Otherwise, call StartLoadingFavicon directly to start favicon loading.
  if (top_sites_has_pending_notification_)
    ProcessTopSitesNotification();
  else
    StartLoadingFavicon();
}

void JumpList::ProcessTopSitesNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Opening the first tab in one session triggers a TopSite history sync.
  // Delay this sync till the first tab is closed to allow the "recently closed"
  // category from last session to stay longer. All previous pending
  // notifications from TopSites are ignored.
  if (!has_tab_closed_) {
    top_sites_has_pending_notification_ = false;
    return;
  }

  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile_);
  if (top_sites) {
    top_sites->GetMostVisitedURLs(
        base::Bind(&JumpList::OnMostVisitedURLsAvailable,
                   weak_ptr_factory_.GetWeakPtr()),
        false);
  }
}

void JumpList::ProcessTabRestoreServiceNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create a list of ShellLinkItems from the "Recently Closed" pages.
  // As noted above, we create a ShellLinkItem objects with the following
  // parameters.
  // * arguments
  //   The last URL of the tab object.
  // * title
  //   The title of the last URL.
  // * icon
  //   An empty string. This value is to be updated in OnFaviconDataAvailable().

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile_);

  recently_closed_pages_.clear();

  for (const auto& entry : tab_restore_service->entries()) {
    if (recently_closed_pages_.size() >= kRecentlyClosedItems)
      break;
    switch (entry->type) {
      case sessions::TabRestoreService::TAB:
        AddTab(static_cast<const sessions::TabRestoreService::Tab&>(*entry),
               kRecentlyClosedItems);
        break;
      case sessions::TabRestoreService::WINDOW:
        AddWindow(
            static_cast<const sessions::TabRestoreService::Window&>(*entry),
            kRecentlyClosedItems);
        break;
    }
  }

  recently_closed_should_update_ = true;

  // Force a TopSite history sync when closing a first tab in one session.
  if (!has_tab_closed_) {
    has_tab_closed_ = true;
    scoped_refptr<history::TopSites> top_sites =
        TopSitesFactory::GetForProfile(profile_);
    if (top_sites)
      top_sites->SyncWithHistory();
  }
}

// static
void JumpList::DeleteIconFiles(const base::FilePath& icon_dir,
                               URLIconCache* icon_cache) {
  DCHECK(icon_cache);

  // Put all cached icon file paths into a set.
  base::flat_set<base::FilePath> cached_files;
  cached_files.reserve(icon_cache->size());

  for (const auto& url_path_pair : *icon_cache)
    cached_files.insert(url_path_pair.second);

  DeleteNonCachedFiles(icon_dir, cached_files);
}

// static
int JumpList::CreateIconFiles(const base::FilePath& icon_dir,
                              const ShellLinkItemList& item_list,
                              size_t max_items,
                              URLIconCache* icon_cache) {
  DCHECK(icon_cache);

  // TODO(chengx): Remove the UMA histogram after fixing http://crbug.com/40407.
  SCOPED_UMA_HISTOGRAM_TIMER("WinJumplist.CreateIconFilesDuration");

  int icons_created = 0;

  // Reuse icons for urls that already present in the current JumpList.
  URLIconCache updated_map;
  for (ShellLinkItemList::const_iterator iter = item_list.begin();
       iter != item_list.end() && max_items > 0; ++iter, --max_items) {
    ShellLinkItem* item = iter->get();
    auto cache_iter = icon_cache->find(item->url());
    if (cache_iter != icon_cache->end()) {
      item->set_icon(cache_iter->second.value(), 0);
      updated_map[item->url()] = cache_iter->second;
    } else {
      base::FilePath icon_path;
      if (CreateIconFile(item->icon_image(), icon_dir, &icon_path)) {
        ++icons_created;
        item->set_icon(icon_path.value(), 0);
        updated_map[item->url()] = icon_path;
      }
    }
  }
  icon_cache->swap(updated_map);

  return icons_created;
}

// static
int JumpList::UpdateIconFiles(const base::FilePath& icon_dir,
                              const ShellLinkItemList& page_list,
                              size_t slot_limit,
                              URLIconCache* icon_cache) {
  DCHECK(icon_cache);

  int icons_created = 0;

  // Clear the JumpList icon folder at |icon_dir| and the cache when
  // 1) |icon_cache| is empty. This happens when "Most visited" or "Recently
  //    closed" category updates for the 1st time after Chrome is launched.
  // 2) The number of icons in |icon_dir| has exceeded the limit.
  if (icon_cache->empty() || FilesExceedLimitInDir(icon_dir, slot_limit * 2)) {
    DeleteDirectoryContentAndLogRuntime(icon_dir, kFileDeleteLimit);
    icon_cache->clear();
    // Create new icons only when the directory exists and is empty.
    if (base::CreateDirectory(icon_dir) && base::IsDirectoryEmpty(icon_dir))
      icons_created +=
          CreateIconFiles(icon_dir, page_list, slot_limit, icon_cache);
  } else if (base::CreateDirectory(icon_dir)) {
    icons_created +=
        CreateIconFiles(icon_dir, page_list, slot_limit, icon_cache);
    DeleteIconFiles(icon_dir, icon_cache);
  }

  return icons_created;
}

// static
void JumpList::RunUpdateJumpList(
    const base::string16& app_id,
    const base::FilePath& profile_dir,
    const ShellLinkItemList& most_visited_pages,
    const ShellLinkItemList& recently_closed_pages,
    bool most_visited_should_update,
    bool recently_closed_should_update,
    IncognitoModePrefs::Availability incognito_availability,
    UpdateResults* update_results) {
  if (!JumpListUpdater::IsEnabled())
    return;

  DCHECK(update_results);

  JumpListUpdater jumplist_updater(app_id);

  base::ElapsedTimer begin_update_timer;

  if (!jumplist_updater.BeginUpdate())
    return;

  // Discard this JumpList update if JumpListUpdater::BeginUpdate takes longer
  // than the maximum allowed time, as it's very likely the following update
  // steps will also take a long time. As we've not updated the icons on the
  // disk, discarding this update wont't affect the current JumpList used by OS.
  if (begin_update_timer.Elapsed() >= kTimeOutForJumplistBeginUpdate) {
    update_results->update_timeout = true;
    return;
  }

  // Record the desired number of icons created in this JumpList update.
  int icons_created = 0;

  // Update the icons for "Most Visisted" category of the JumpList if needed.
  if (most_visited_should_update) {
    base::FilePath icon_dir_most_visited = GenerateJumplistIconDirName(
        profile_dir, FILE_PATH_LITERAL("MostVisited"));

    icons_created += UpdateIconFiles(
        icon_dir_most_visited, most_visited_pages, kMostVisitedItems,
        &update_results->most_visited_icons_in_update);
  }

  // Update the icons for "Recently Closed" category of the JumpList if needed.
  if (recently_closed_should_update) {
    base::FilePath icon_dir_recent_closed = GenerateJumplistIconDirName(
        profile_dir, FILE_PATH_LITERAL("RecentClosed"));

    icons_created += UpdateIconFiles(
        icon_dir_recent_closed, recently_closed_pages, kRecentlyClosedItems,
        &update_results->recently_closed_icons_in_update);
  }

  // TODO(chengx): Remove the UMA histogram after fixing http://crbug.com/40407.
  UMA_HISTOGRAM_COUNTS_100("WinJumplist.CreateIconFilesCount", icons_created);

  // TODO(chengx): Remove the UMA histogram after fixing http://crbug.com/40407.
  SCOPED_UMA_HISTOGRAM_TIMER("WinJumplist.UpdateJumpListDuration");

  base::ElapsedTimer add_custom_category_timer;

  // Update the "Most Visited" category of the JumpList if it exists.
  // This update request is applied into the JumpList when we commit this
  // transaction.
  if (!jumplist_updater.AddCustomCategory(
          l10n_util::GetStringUTF16(IDS_NEW_TAB_MOST_VISITED),
          most_visited_pages, kMostVisitedItems)) {
    return;
  }

  // Update the "Recently Closed" category of the JumpList.
  if (!jumplist_updater.AddCustomCategory(
          l10n_util::GetStringUTF16(IDS_RECENTLY_CLOSED), recently_closed_pages,
          kRecentlyClosedItems)) {
    return;
  }

  // If JumpListUpdater::AddCustomCategory or JumpListUpdater::CommitUpdate
  // takes longer than the maximum allowed time, skip the next
  // |kUpdatesToSkipUnderHeavyLoad| updates. This update should be finished
  // because we've already updated the icons on the disk. If discarding this
  // update from here, some items in the current JumpList may not have icons
  // as they've been delete from the disk. In this case, the background color of
  // the JumpList panel is used instead, which doesn't look nice.

  if (add_custom_category_timer.Elapsed() >= kTimeOutForAddCustomCategory)
    update_results->update_timeout = true;

  // Update the "Tasks" category of the JumpList.
  if (!UpdateTaskCategory(&jumplist_updater, incognito_availability))
    return;

  base::ElapsedTimer commit_update_timer;

  // Commit this transaction and send the updated JumpList to Windows.
  if (jumplist_updater.CommitUpdate())
    update_results->update_success = true;

  if (commit_update_timer.Elapsed() >= kTimeOutForJumplistCommitUpdate)
    update_results->update_timeout = true;
}

void JumpList::OnRunUpdateCompletion(
    std::unique_ptr<UpdateResults> update_results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Update JumpList member variables based on the results from the update run
  // just finished.
  if (update_results->update_timeout)
    updates_to_skip_ = kUpdatesToSkipUnderHeavyLoad;

  if (update_results->update_success) {
    most_visited_icons_.swap(update_results->most_visited_icons_in_update);
    recently_closed_icons_.swap(
        update_results->recently_closed_icons_in_update);
    most_visited_should_update_ = false;
    recently_closed_should_update_ = false;
  }

  update_in_progress_ = false;

  // If there is any new notification during the update run just finished, start
  // another JumpList update.
  // Otherwise, post tasks to delete the JumpListIcons and JumpListIconsOld
  // folders as they are no longer needed. Now we have the
  // JumpListIcons{MostVisited, RecentClosed} folders instead.
  if (top_sites_has_pending_notification_ ||
      tab_restore_has_pending_notification_) {
    InitializeTimerForUpdate();
  } else {
    base::FilePath profile_dir = profile_->GetPath();
    base::FilePath icon_dir =
        GenerateJumplistIconDirName(profile_dir, FILE_PATH_LITERAL(""));
    delete_jumplisticons_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DeleteDirectory, std::move(icon_dir), kFileDeleteLimit));

    base::FilePath icon_dir_old =
        GenerateJumplistIconDirName(profile_dir, FILE_PATH_LITERAL("Old"));
    delete_jumplisticons_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DeleteDirectory, std::move(icon_dir_old),
                              kFileDeleteLimit));
  }
}
