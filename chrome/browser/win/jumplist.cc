// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/jumplist.h"

#include <windows.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/metrics/jumplist_metrics_win.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/browser_distribution.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/history/core/browser/top_sites.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

using content::BrowserThread;
using JumpListData = JumpList::JumpListData;

namespace {

// Delay jumplist updates to allow collapsing of redundant update requests.
const int kDelayForJumplistUpdateInMS = 3500;

// JumpList folder's move operation status.
enum MoveOperationResult {
  SUCCESS = 0,
  MOVE_FILE_EX_FAILED = 1 << 0,
  COPY_DIR_FAILED = 1 << 1,
  DELETE_FILE_RECURSIVE_FAILED = 1 << 2,
  DELETE_SOURCE_FOLDER_FAILED = 1 << 3,
  DELETE_TARGET_FOLDER_FAILED = 1 << 4,
  DELETE_DIR_READ_ONLY = 1 << 5,
  END = 1 << 6
};

// Append the common switches to each shell link.
void AppendCommonSwitches(ShellLinkItem* shell_link) {
  const char* kSwitchNames[] = { switches::kUserDataDir };
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
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
  if (!IconUtil::CreateIconFileFromImageFamily(image_family, path,
                                               IconUtil::NORMAL_WRITE))
    return false;

  // Add this icon file to the list and return its absolute path.
  // The IShellLink::SetIcon() function needs the absolute path to an icon.
  *icon_path = path;
  return true;
}

// Helper method for RunUpdate to create icon files for the asynchrounously
// loaded icons.
void CreateIconFiles(const base::FilePath& icon_dir,
                     const ShellLinkItemList& item_list) {
  for (ShellLinkItemList::const_iterator item = item_list.begin();
      item != item_list.end(); ++item) {
    base::FilePath icon_path;
    if (CreateIconFile((*item)->icon_data(), icon_dir, &icon_path))
      (*item)->set_icon(icon_path.value(), 0);
  }
}

// Updates the "Tasks" category of the JumpList.
bool UpdateTaskCategory(
    JumpListUpdater* jumplist_updater,
    IncognitoModePrefs::Availability incognito_availability) {
  base::FilePath chrome_path;
  if (!PathService::Get(base::FILE_EXE, &chrome_path))
    return false;

  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  int icon_index = distribution->GetIconIndex();

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
  // add it to the collection. We use our application icon as the icon for
  // this item.
  if (incognito_availability != IncognitoModePrefs::DISABLED) {
    scoped_refptr<ShellLinkItem> incognito = CreateShellLink();
    incognito->GetCommandLine()->AppendSwitch(switches::kIncognito);
    base::string16 incognito_title =
        l10n_util::GetStringUTF16(IDS_NEW_INCOGNITO_WINDOW);
    base::ReplaceSubstringsAfterOffset(
        &incognito_title, 0, L"&", base::StringPiece16());
    incognito->set_title(incognito_title);
    incognito->set_icon(chrome_path.value(), icon_index);
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

  // Update the "Most Visited" category of the JumpList if it exists.
  // This update request is applied into the JumpList when we commit this
  // transaction.
  if (!jumplist_updater.AddCustomCategory(
          l10n_util::GetStringUTF16(IDS_NEW_TAB_MOST_VISITED),
          most_visited_pages, most_visited_items)) {
    return false;
  }

  // Update the "Recently Closed" category of the JumpList.
  if (!jumplist_updater.AddCustomCategory(
          l10n_util::GetStringUTF16(IDS_RECENTLY_CLOSED),
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

// A wrapper function for recording UMA histogram.
void RecordFolderMoveResult(int move_operation_status) {
  UMA_HISTOGRAM_ENUMERATION("WinJumplist.DetailedFolderMoveResults",
                            move_operation_status, MoveOperationResult::END);
}

// This function is an exact copy of //base for UMA debugging purpose of
// DeleteFile() below.
// Deletes all files and directories in a path.
// Returns false on the first failure it encounters.
bool DeleteFileRecursive(const base::FilePath& path,
                         const base::FilePath::StringType& pattern,
                         bool recursive) {
  base::FileEnumerator traversal(
      path, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES, pattern);
  for (base::FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    // Try to clear the read-only bit if we find it.
    base::FileEnumerator::FileInfo info = traversal.GetInfo();
    if ((info.find_data().dwFileAttributes & FILE_ATTRIBUTE_READONLY) &&
        (recursive || !info.IsDirectory())) {
      ::SetFileAttributes(
          current.value().c_str(),
          info.find_data().dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
    }

    if (info.IsDirectory()) {
      if (recursive && (!DeleteFileRecursive(current, pattern, true) ||
                        !::RemoveDirectory(current.value().c_str())))
        return false;
    } else if (!::DeleteFile(current.value().c_str())) {
      return false;
    }
  }
  return true;
}

// This function is a copy from //base for UMA debugging purpose.
// Deletes all files and directories in a path.
// Returns false on the first failure it encounters.
bool DeleteFile(const base::FilePath& path,
                bool recursive,
                int* move_operation_status) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (path.empty())
    return true;

  if (path.value().length() >= MAX_PATH)
    return false;

  // Handle any path with wildcards.
  if (path.BaseName().value().find_first_of(L"*?") !=
      base::FilePath::StringType::npos) {
    bool ret =
        DeleteFileRecursive(path.DirName(), path.BaseName().value(), recursive);
    if (!ret) {
      *move_operation_status |=
          MoveOperationResult::DELETE_FILE_RECURSIVE_FAILED;
      *move_operation_status |=
          MoveOperationResult::DELETE_SOURCE_FOLDER_FAILED;
    }
    return ret;
  }
  DWORD attr = ::GetFileAttributes(path.value().c_str());
  // We're done if we can't find the path.
  if (attr == INVALID_FILE_ATTRIBUTES)
    return true;
  // We may need to clear the read-only bit.
  if ((attr & FILE_ATTRIBUTE_READONLY) &&
      !::SetFileAttributes(path.value().c_str(),
                           attr & ~FILE_ATTRIBUTE_READONLY)) {
    *move_operation_status |= MoveOperationResult::DELETE_DIR_READ_ONLY;
    *move_operation_status |= MoveOperationResult::DELETE_FILE_RECURSIVE_FAILED;
    *move_operation_status |= MoveOperationResult::DELETE_SOURCE_FOLDER_FAILED;
    return false;
  }
  // Directories are handled differently if they're recursive.
  if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
    return !!::DeleteFile(path.value().c_str());
  // Handle a simple, single file delete.
  if (!recursive || DeleteFileRecursive(path, L"*", true)) {
    bool ret = !!::RemoveDirectory(path.value().c_str());
    if (!ret)
      *move_operation_status |=
          MoveOperationResult::DELETE_SOURCE_FOLDER_FAILED;
    return ret;
  }

  *move_operation_status |= MoveOperationResult::DELETE_FILE_RECURSIVE_FAILED;
  *move_operation_status |= MoveOperationResult::DELETE_SOURCE_FOLDER_FAILED;
  return false;
}

// This function is mostly copied from //base.
// It has some issue when the dest dir string contains the src dir string.
// Temporary fix by replacing the old logic with IsParent() call.
bool CopyDirectoryTemp(const base::FilePath& from_path,
                       const base::FilePath& to_path,
                       bool recursive) {
  // NOTE(maruel): Previous version of this function used to call
  // SHFileOperation().  This used to copy the file attributes and extended
  // attributes, OLE structured storage, NTFS file system alternate data
  // streams, SECURITY_DESCRIPTOR. In practice, this is not what we want, we
  // want the containing directory to propagate its SECURITY_DESCRIPTOR.
  base::ThreadRestrictions::AssertIOAllowed();

  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }

  // This function does not properly handle destinations within the source.
  base::FilePath real_to_path = to_path;
  if (base::PathExists(real_to_path)) {
    real_to_path = base::MakeAbsoluteFilePath(real_to_path);
    if (real_to_path.empty())
      return false;
  } else {
    real_to_path = base::MakeAbsoluteFilePath(real_to_path.DirName());
    if (real_to_path.empty())
      return false;
  }
  base::FilePath real_from_path = base::MakeAbsoluteFilePath(from_path);
  if (real_from_path.empty())
    return false;

  // Originally this CopyDirectory function returns false when to_path string
  // contains from_path string. While this can be that the to_path is within
  // the from_path, e.g., parent-child relationship in terms of folder, it
  // also can be the two paths are in the same folder, just that one name
  // contains
  // the other, e.g. C:\\JumpListIcon and C:\\JumpListIconOld.
  // We make this function not return false in the latter situation by calling
  // IsParent() function.
  if (real_to_path.value().size() >= real_from_path.value().size() &&
      real_from_path.IsParent(real_to_path)) {
    return false;
  }

  int traverse_type = base::FileEnumerator::FILES;
  if (recursive)
    traverse_type |= base::FileEnumerator::DIRECTORIES;
  base::FileEnumerator traversal(from_path, recursive, traverse_type);

  if (!base::PathExists(from_path)) {
    DLOG(ERROR) << "CopyDirectory() couldn't stat source directory: "
                << from_path.value().c_str();
    return false;
  }
  // TODO(maruel): This is not necessary anymore.
  DCHECK(recursive || base::DirectoryExists(from_path));

  base::FilePath current = from_path;
  bool from_is_dir = base::DirectoryExists(from_path);
  bool success = true;
  base::FilePath from_path_base = from_path;
  if (recursive && base::DirectoryExists(to_path)) {
    // If the destination already exists and is a directory, then the
    // top level of source needs to be copied.
    from_path_base = from_path.DirName();
  }

  while (success && !current.empty()) {
    // current is the source path, including from_path, so append
    // the suffix after from_path to to_path to create the target_path.
    base::FilePath target_path(to_path);
    if (from_path_base != current) {
      if (!from_path_base.AppendRelativePath(current, &target_path)) {
        success = false;
        break;
      }
    }

    if (from_is_dir) {
      if (!base::DirectoryExists(target_path) &&
          !::CreateDirectory(target_path.value().c_str(), NULL)) {
        DLOG(ERROR) << "CopyDirectory() couldn't create directory: "
                    << target_path.value().c_str();
        success = false;
      }
    } else if (!base::CopyFile(current, target_path)) {
      DLOG(ERROR) << "CopyDirectory() couldn't create file: "
                  << target_path.value().c_str();
      success = false;
    }

    current = traversal.Next();
    if (!current.empty())
      from_is_dir = traversal.GetInfo().IsDirectory();
  }

  return success;
}

// This function is a temporary fork of Move() and MoveUnsafe() in
// base/files/file_util.h, where UMA functions are added to gather user data.
// As //base is highly mature, we tend not to add this temporary function
// in it. This change will be reverted after user data gathered.
bool MoveDebugWithUMA(const base::FilePath& from_path,
                      const base::FilePath& to_path,
                      int folder_delete_fail) {
  if (from_path.ReferencesParent() || to_path.ReferencesParent())
    return false;

  base::ThreadRestrictions::AssertIOAllowed();

  // This variable records the status of move operations.
  int move_operation_status = MoveOperationResult::SUCCESS;
  if (folder_delete_fail)
    move_operation_status |= MoveOperationResult::DELETE_TARGET_FOLDER_FAILED;

  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }
  if (MoveFileEx(from_path.value().c_str(), to_path.value().c_str(),
                 MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING) != 0) {
    RecordFolderMoveResult(move_operation_status);
    return true;
  }

  move_operation_status |= MoveOperationResult::MOVE_FILE_EX_FAILED;

  // Keep the last error value from MoveFileEx around in case the below
  // fails.
  bool ret = false;
  DWORD last_error = ::GetLastError();

  if (base::DirectoryExists(from_path)) {
    // MoveFileEx fails if moving directory across volumes. We will simulate
    // the move by using Copy and Delete. Ideally we could check whether
    // from_path and to_path are indeed in different volumes.
    if (CopyDirectoryTemp(from_path, to_path, true)) {
      if (DeleteFile(from_path, true, &move_operation_status))
        ret = true;
    } else {
      move_operation_status |= MoveOperationResult::COPY_DIR_FAILED;
      move_operation_status |=
          MoveOperationResult::DELETE_FILE_RECURSIVE_FAILED;
      move_operation_status |= MoveOperationResult::DELETE_SOURCE_FOLDER_FAILED;
    }
  }

  if (!ret) {
    // Leave a clue about what went wrong so that it can be (at least) picked
    // up by a PLOG entry.
    ::SetLastError(last_error);
  }

  RecordFolderMoveResult(move_operation_status);

  return ret;
}

// Updates the jumplist, once all the data has been fetched.
void RunUpdateOnFileThread(
    IncognitoModePrefs::Availability incognito_availability,
    const std::wstring& app_id,
    const base::FilePath& icon_dir,
    base::RefCountedData<JumpListData>* ref_counted_data) {
  JumpListData* data = &ref_counted_data->data;
  ShellLinkItemList local_most_visited_pages;
  ShellLinkItemList local_recently_closed_pages;

  {
    base::AutoLock auto_lock(data->list_lock_);
    // Make sure we are not out of date: if icon_urls_ is not empty, then
    // another notification has been received since we processed this one
    if (!data->icon_urls_.empty())
      return;

    // Make local copies of lists so we can release the lock.
    local_most_visited_pages = data->most_visited_pages_;
    local_recently_closed_pages = data->recently_closed_pages_;
  }

  // Delete the directory which contains old icon files, rename the current
  // icon directory, and create a new directory which contains new JumpList
  // icon files.
  base::FilePath icon_dir_old(icon_dir.value() + L"Old");

  enum FolderOperationResult {
    SUCCESS = 0,
    DELETE_FAILED = 1 << 0,
    MOVE_FAILED = 1 << 1,
    CREATE_DIR_FAILED = 1 << 2,
    // This value is beyond the sum of all bit fields above and
    // should remain last (shifted by one more than the last value)
    END = 1 << 3
  };

  // This variable records the status of three folder operations.
  int folder_operation_status = FolderOperationResult::SUCCESS;

  if (base::PathExists(icon_dir_old) && !base::DeleteFile(icon_dir_old, true))
    folder_operation_status |= FolderOperationResult::DELETE_FAILED;
  if (!MoveDebugWithUMA(icon_dir, icon_dir_old, folder_operation_status))
    folder_operation_status |= FolderOperationResult::MOVE_FAILED;
  if (!base::CreateDirectory(icon_dir))
    folder_operation_status |= FolderOperationResult::CREATE_DIR_FAILED;

  UMA_HISTOGRAM_ENUMERATION("WinJumplist.FolderResults",
                            folder_operation_status,
                            FolderOperationResult::END);

  // Create temporary icon files for shortcuts in the "Most Visited" category.
  CreateIconFiles(icon_dir, local_most_visited_pages);

  // Create temporary icon files for shortcuts in the "Recently Closed"
  // category.
  CreateIconFiles(icon_dir, local_recently_closed_pages);

  // We finished collecting all resources needed for updating an application
  // JumpList. So, create a new JumpList and replace the current JumpList
  // with it.
  UpdateJumpList(app_id.c_str(),
                 local_most_visited_pages,
                 local_recently_closed_pages,
                 incognito_availability);
}

}  // namespace

JumpList::JumpListData::JumpListData() {}

JumpList::JumpListData::~JumpListData() {}

JumpList::JumpList(Profile* profile)
    : RefcountedKeyedService(content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::UI)),
      profile_(profile),
      jumplist_data_(new base::RefCountedData<JumpListData>),
      task_id_(base::CancelableTaskTracker::kBadTaskId),
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
  icon_dir_ = profile_->GetPath().Append(chrome::kJumpListIconDirname);

  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile_);
  if (top_sites) {
    // TopSites updates itself after a delay. This is especially noticable when
    // your profile is empty. Ask TopSites to update itself when jumplist is
    // initialized.
    top_sites->SyncWithHistory();
    // Register as TopSitesObserver so that we can update ourselves when the
    // TopSites changes.
    top_sites->AddObserver(this);
  }
  tab_restore_service->AddObserver(this);
  pref_change_registrar_.reset(new PrefChangeRegistrar);
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kIncognitoModeAvailability,
      base::Bind(&JumpList::OnIncognitoAvailabilityChanged, this));
}

JumpList::~JumpList() {
  DCHECK(CalledOnValidThread());
  Terminate();
}

// static
bool JumpList::Enabled() {
  return JumpListUpdater::IsEnabled();
}

void JumpList::CancelPendingUpdate() {
  DCHECK(CalledOnValidThread());
  if (task_id_ != base::CancelableTaskTracker::kBadTaskId) {
    cancelable_task_tracker_.TryCancel(task_id_);
    task_id_ = base::CancelableTaskTracker::kBadTaskId;
  }
}

void JumpList::Terminate() {
  DCHECK(CalledOnValidThread());
  CancelPendingUpdate();
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
  profile_ = NULL;
}

void JumpList::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  Terminate();
}

void JumpList::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& urls) {
  DCHECK(CalledOnValidThread());
  // If we have a pending favicon request, cancel it here (it is out of date).
  CancelPendingUpdate();

  {
    JumpListData* data = &jumplist_data_->data;
    base::AutoLock auto_lock(data->list_lock_);
    data->most_visited_pages_.clear();
    for (size_t i = 0; i < urls.size(); i++) {
      const history::MostVisitedURL& url = urls[i];
      scoped_refptr<ShellLinkItem> link = CreateShellLink();
      std::string url_string = url.url.spec();
      std::wstring url_string_wide = base::UTF8ToWide(url_string);
      link->GetCommandLine()->AppendArgNative(url_string_wide);
      link->GetCommandLine()->AppendSwitchASCII(
          switches::kWinJumplistAction, jumplist::kMostVisitedCategory);
      link->set_title(!url.title.empty() ? url.title : url_string_wide);
      data->most_visited_pages_.push_back(link);
      data->icon_urls_.push_back(make_pair(url_string, link));
    }
  }

  // Send a query that retrieves the first favicon.
  StartLoadingFavicon();
}

void JumpList::TabRestoreServiceChanged(sessions::TabRestoreService* service) {
  DCHECK(CalledOnValidThread());
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
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile_);
  for (const auto& entry : tab_restore_service->entries()) {
    switch (entry->type) {
      case sessions::TabRestoreService::TAB:
        AddTab(static_cast<const sessions::TabRestoreService::Tab&>(*entry),
               &temp_list, kRecentlyClosedCount);
        break;
      case sessions::TabRestoreService::WINDOW:
        AddWindow(
            static_cast<const sessions::TabRestoreService::Window&>(*entry),
            &temp_list, kRecentlyClosedCount);
        break;
    }
  }
  // Lock recently_closed_pages and copy temp_list into it.
  {
    JumpListData* data = &jumplist_data_->data;
    base::AutoLock auto_lock(data->list_lock_);
    data->recently_closed_pages_ = temp_list;
  }

  // Send a query that retrieves the first favicon.
  StartLoadingFavicon();
}

void JumpList::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {}

bool JumpList::AddTab(const sessions::TabRestoreService::Tab& tab,
                      ShellLinkItemList* list,
                      size_t max_items) {
  DCHECK(CalledOnValidThread());

  // This code adds the URL and the title strings of the given tab to the
  // specified list.
  if (list->size() >= max_items)
    return false;

  scoped_refptr<ShellLinkItem> link = CreateShellLink();
  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(tab.current_navigation_index);
  std::string url = current_navigation.virtual_url().spec();
  link->GetCommandLine()->AppendArgNative(base::UTF8ToWide(url));
  link->GetCommandLine()->AppendSwitchASCII(
      switches::kWinJumplistAction, jumplist::kRecentlyClosedCategory);
  link->set_title(current_navigation.title());
  list->push_back(link);
  {
    JumpListData* data = &jumplist_data_->data;
    base::AutoLock auto_lock(data->list_lock_);
    data->icon_urls_.push_back(std::make_pair(std::move(url), std::move(link)));
  }
  return true;
}

void JumpList::AddWindow(const sessions::TabRestoreService::Window& window,
                         ShellLinkItemList* list,
                         size_t max_items) {
  DCHECK(CalledOnValidThread());

  // This code enumerates al the tabs in the given window object and add their
  // URLs and titles to the list.
  DCHECK(!window.tabs.empty());

  for (const auto& tab : window.tabs) {
    if (!AddTab(*tab, list, max_items))
      return;
  }
}

void JumpList::StartLoadingFavicon() {
  DCHECK(CalledOnValidThread());

  GURL url;
  bool waiting_for_icons = true;
  {
    JumpListData* data = &jumplist_data_->data;
    base::AutoLock auto_lock(data->list_lock_);
    waiting_for_icons = !data->icon_urls_.empty();
    if (waiting_for_icons) {
      // Ask FaviconService if it has a favicon of a URL.
      // When FaviconService has one, it will call OnFaviconDataAvailable().
      url = GURL(data->icon_urls_.front().first);
    }
  }

  if (!waiting_for_icons) {
    // No more favicons are needed by the application JumpList. Schedule a
    // RunUpdateOnFileThread call.
    PostRunUpdate();
    return;
  }

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  task_id_ = favicon_service->GetFaviconImageForPageURL(
      url,
      base::Bind(&JumpList::OnFaviconDataAvailable, base::Unretained(this)),
      &cancelable_task_tracker_);
}

void JumpList::OnFaviconDataAvailable(
    const favicon_base::FaviconImageResult& image_result) {
  DCHECK(CalledOnValidThread());

  // If there is currently a favicon request in progress, it is now outdated,
  // as we have received another, so nullify the handle from the old request.
  task_id_ = base::CancelableTaskTracker::kBadTaskId;
  // Lock the list to set icon data and pop the url.
  {
    JumpListData* data = &jumplist_data_->data;
    base::AutoLock auto_lock(data->list_lock_);
    // Attach the received data to the ShellLinkItem object.
    // This data will be decoded by the RunUpdateOnFileThread method.
    if (!image_result.image.IsEmpty()) {
      if (!data->icon_urls_.empty() && data->icon_urls_.front().second.get())
        data->icon_urls_.front().second->set_icon_data(
            image_result.image.AsBitmap());
    }

    if (!data->icon_urls_.empty())
      data->icon_urls_.pop_front();
  }
  // Check whether we need to load more favicons.
  StartLoadingFavicon();
}

void JumpList::OnIncognitoAvailabilityChanged() {
  DCHECK(CalledOnValidThread());

  bool waiting_for_icons = true;
  {
    JumpListData* data = &jumplist_data_->data;
    base::AutoLock auto_lock(data->list_lock_);
    waiting_for_icons = !data->icon_urls_.empty();
  }
  if (!waiting_for_icons)
    PostRunUpdate();
  // If |icon_urls_| isn't empty then OnFaviconDataAvailable will eventually
  // call PostRunUpdate().
}

void JumpList::PostRunUpdate() {
  DCHECK(CalledOnValidThread());

  TRACE_EVENT0("browser", "JumpList::PostRunUpdate");
  // Initialize the one-shot timer to update the jumplists in a while.
  // If there is already a request queued then cancel it and post the new
  // request. This ensures that JumpListUpdates won't happen until there has
  // been a brief quiet period, thus avoiding update storms.
  if (timer_.IsRunning()) {
    timer_.Reset();
  } else {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(kDelayForJumplistUpdateInMS),
                 this,
                 &JumpList::DeferredRunUpdate);
  }
}

void JumpList::DeferredRunUpdate() {
  DCHECK(CalledOnValidThread());

  TRACE_EVENT0("browser", "JumpList::DeferredRunUpdate");
  // Check if incognito windows (or normal windows) are disabled by policy.
  IncognitoModePrefs::Availability incognito_availability =
      profile_ ? IncognitoModePrefs::GetAvailability(profile_->GetPrefs())
               : IncognitoModePrefs::ENABLED;

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&RunUpdateOnFileThread,
                 incognito_availability,
                 app_id_,
                 icon_dir_,
                 base::RetainedRef(jumplist_data_)));
}

void JumpList::TopSitesLoaded(history::TopSites* top_sites) {
}

void JumpList::TopSitesChanged(history::TopSites* top_sites,
                               ChangeReason change_reason) {
  top_sites->GetMostVisitedURLs(
      base::Bind(&JumpList::OnMostVisitedURLsAvailable,
                 weak_ptr_factory_.GetWeakPtr()),
      false);
}
