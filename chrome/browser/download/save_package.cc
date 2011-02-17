// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/save_package.h"

#include <algorithm>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/download/save_file.h"
#include "chrome/browser/download/save_file_manager.h"
#include "chrome/browser/download/save_item.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageSerializerClient.h"
#include "ui/base/l10n/l10n_util.h"

using base::Time;
using WebKit::WebPageSerializerClient;

namespace {

// A counter for uniquely identifying each save package.
int g_save_package_id = 0;

// Default name which will be used when we can not get proper name from
// resource URL.
const char kDefaultSaveName[] = "saved_resource";

const FilePath::CharType kDefaultHtmlExtension[] =
#if defined(OS_WIN)
    FILE_PATH_LITERAL("htm");
#else
    FILE_PATH_LITERAL("html");
#endif

// Maximum number of file ordinal number. I think it's big enough for resolving
// name-conflict files which has same base file name.
const int32 kMaxFileOrdinalNumber = 9999;

// Maximum length for file path. Since Windows have MAX_PATH limitation for
// file path, we need to make sure length of file path of every saved file
// is less than MAX_PATH
#if defined(OS_WIN)
const uint32 kMaxFilePathLength = MAX_PATH - 1;
#elif defined(OS_POSIX)
const uint32 kMaxFilePathLength = PATH_MAX - 1;
#endif

// Maximum length for file ordinal number part. Since we only support the
// maximum 9999 for ordinal number, which means maximum file ordinal number part
// should be "(9998)", so the value is 6.
const uint32 kMaxFileOrdinalNumberPartLength = 6;

// If false, we don't prompt the user as to where to save the file.  This
// exists only for testing.
bool g_should_prompt_for_filename = true;

// Indexes used for specifying which element in the extensions dropdown
// the user chooses when picking a save type.
const int kSelectFileHtmlOnlyIndex = 1;
const int kSelectFileCompleteIndex = 2;

// Used for mapping between SavePackageType constants and the indexes above.
const SavePackage::SavePackageType kIndexToSaveType[] = {
  SavePackage::SAVE_TYPE_UNKNOWN,
  SavePackage::SAVE_AS_ONLY_HTML,
  SavePackage::SAVE_AS_COMPLETE_HTML,
};

// Used for mapping between the IDS_ string identifiers and the indexes above.
const int kIndexToIDS[] = {
  0, IDS_SAVE_PAGE_DESC_HTML_ONLY, IDS_SAVE_PAGE_DESC_COMPLETE,
};

int SavePackageTypeToIndex(SavePackage::SavePackageType type) {
  for (size_t i = 0; i < arraysize(kIndexToSaveType); ++i) {
    if (kIndexToSaveType[i] == type)
      return i;
  }
  NOTREACHED();
  return -1;
}

// Strip current ordinal number, if any. Should only be used on pure
// file names, i.e. those stripped of their extensions.
// TODO(estade): improve this to not choke on alternate encodings.
FilePath::StringType StripOrdinalNumber(
    const FilePath::StringType& pure_file_name) {
  FilePath::StringType::size_type r_paren_index =
      pure_file_name.rfind(FILE_PATH_LITERAL(')'));
  FilePath::StringType::size_type l_paren_index =
      pure_file_name.rfind(FILE_PATH_LITERAL('('));
  if (l_paren_index >= r_paren_index)
    return pure_file_name;

  for (FilePath::StringType::size_type i = l_paren_index + 1;
       i != r_paren_index; ++i) {
    if (!IsAsciiDigit(pure_file_name[i]))
      return pure_file_name;
  }

  return pure_file_name.substr(0, l_paren_index);
}

// Check whether we can save page as complete-HTML for the contents which
// have specified a MIME type. Now only contents which have the MIME type
// "text/html" can be saved as complete-HTML.
bool CanSaveAsComplete(const std::string& contents_mime_type) {
  return contents_mime_type == "text/html" ||
         contents_mime_type == "application/xhtml+xml";
}

}  // namespace

SavePackage::SavePackage(TabContents* tab_contents,
                         SavePackageType save_type,
                         const FilePath& file_full_path,
                         const FilePath& directory_full_path)
    : TabContentsObserver(tab_contents),
      file_manager_(NULL),
      download_(NULL),
      page_url_(GetUrlToBeSaved()),
      saved_main_file_path_(file_full_path),
      saved_main_directory_path_(directory_full_path),
      title_(tab_contents->GetTitle()),
      finished_(false),
      user_canceled_(false),
      disk_error_occurred_(false),
      save_type_(save_type),
      all_save_items_count_(0),
      wait_state_(INITIALIZE),
      tab_id_(tab_contents->GetRenderProcessHost()->id()),
      unique_id_(g_save_package_id++),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  DCHECK(page_url_.is_valid());
  DCHECK(save_type_ == SAVE_AS_ONLY_HTML ||
         save_type_ == SAVE_AS_COMPLETE_HTML);
  DCHECK(!saved_main_file_path_.empty() &&
         saved_main_file_path_.value().length() <= kMaxFilePathLength);
  DCHECK(!saved_main_directory_path_.empty() &&
         saved_main_directory_path_.value().length() < kMaxFilePathLength);
  InternalInit();
}

SavePackage::SavePackage(TabContents* tab_contents)
    : TabContentsObserver(tab_contents),
      file_manager_(NULL),
      download_(NULL),
      page_url_(GetUrlToBeSaved()),
      title_(tab_contents->GetTitle()),
      finished_(false),
      user_canceled_(false),
      disk_error_occurred_(false),
      save_type_(SAVE_TYPE_UNKNOWN),
      all_save_items_count_(0),
      wait_state_(INITIALIZE),
      tab_id_(tab_contents->GetRenderProcessHost()->id()),
      unique_id_(g_save_package_id++),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  DCHECK(page_url_.is_valid());
  InternalInit();
}

// This is for testing use. Set |finished_| as true because we don't want
// method Cancel to be be called in destructor in test mode.
// We also don't call InternalInit().
SavePackage::SavePackage(TabContents* tab_contents,
                         const FilePath& file_full_path,
                         const FilePath& directory_full_path)
    : TabContentsObserver(tab_contents),
      file_manager_(NULL),
      download_(NULL),
      saved_main_file_path_(file_full_path),
      saved_main_directory_path_(directory_full_path),
      finished_(true),
      user_canceled_(false),
      disk_error_occurred_(false),
      save_type_(SAVE_TYPE_UNKNOWN),
      all_save_items_count_(0),
      wait_state_(INITIALIZE),
      tab_id_(0),
      unique_id_(g_save_package_id++),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

SavePackage::~SavePackage() {
  // Stop receiving saving job's updates
  if (!finished_ && !canceled()) {
    // Unexpected quit.
    Cancel(true);
  }

  DCHECK(all_save_items_count_ == (waiting_item_queue_.size() +
                                   completed_count() +
                                   in_process_count()));
  // Free all SaveItems.
  while (!waiting_item_queue_.empty()) {
    // We still have some items which are waiting for start to save.
    SaveItem* save_item = waiting_item_queue_.front();
    waiting_item_queue_.pop();
    delete save_item;
  }

  STLDeleteValues(&saved_success_items_);
  STLDeleteValues(&in_progress_items_);
  STLDeleteValues(&saved_failed_items_);

  // The DownloadItem is owned by DownloadManager.
  download_ = NULL;

  file_manager_ = NULL;

  // If there's an outstanding save dialog, make sure it doesn't call us back
  // now that we're gone.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

// Retrieves the URL to be saved from tab_contents_ variable.
GURL SavePackage::GetUrlToBeSaved() {
  // Instead of using tab_contents_.GetURL here, we use url()
  // (which is the "real" url of the page)
  // from the NavigationEntry because it reflects its' origin
  // rather than the displayed one (returned by GetURL) which may be
  // different (like having "view-source:" on the front).
  NavigationEntry* active_entry =
      tab_contents()->controller().GetActiveEntry();
  return active_entry->url();
}

// Cancel all in progress request, might be called by user or internal error.
void SavePackage::Cancel(bool user_action) {
  if (!canceled()) {
    if (user_action)
      user_canceled_ = true;
    else
      disk_error_occurred_ = true;
    Stop();
  }
}

// Init() can be called directly, or indirectly via GetSaveInfo(). In both
// cases, we need file_manager_ to be initialized, so we do this first.
void SavePackage::InternalInit() {
  ResourceDispatcherHost* rdh = g_browser_process->resource_dispatcher_host();
  if (!rdh) {
    NOTREACHED();
    return;
  }

  file_manager_ = rdh->save_file_manager();
  if (!file_manager_) {
    NOTREACHED();
    return;
  }
}

// Initialize the SavePackage.
bool SavePackage::Init() {
  // Set proper running state.
  if (wait_state_ != INITIALIZE)
    return false;

  wait_state_ = START_PROCESS;

  // Initialize the request context and resource dispatcher.
  Profile* profile = tab_contents()->profile();
  if (!profile) {
    NOTREACHED();
    return false;
  }

  request_context_getter_ = profile->GetRequestContext();

  // Create the fake DownloadItem and display the view.
  DownloadManager* download_manager =
      tab_contents()->profile()->GetDownloadManager();
  download_ = new DownloadItem(download_manager,
                               saved_main_file_path_,
                               page_url_,
                               profile->IsOffTheRecord());

  // Transfer the ownership to the download manager. We need the DownloadItem
  // to be alive as long as the Profile is alive.
  download_manager->SavePageAsDownloadStarted(download_);

  tab_contents()->OnStartDownload(download_);

  // Check save type and process the save page job.
  if (save_type_ == SAVE_AS_COMPLETE_HTML) {
    // Get directory
    DCHECK(!saved_main_directory_path_.empty());
    GetAllSavableResourceLinksForCurrentPage();
  } else {
    wait_state_ = NET_FILES;
    SaveFileCreateInfo::SaveFileSource save_source = page_url_.SchemeIsFile() ?
        SaveFileCreateInfo::SAVE_FILE_FROM_FILE :
        SaveFileCreateInfo::SAVE_FILE_FROM_NET;
    SaveItem* save_item = new SaveItem(page_url_,
                                       GURL(),
                                       this,
                                       save_source);
    // Add this item to waiting list.
    waiting_item_queue_.push(save_item);
    all_save_items_count_ = 1;
    download_->set_total_bytes(1);

    DoSavingProcess();
  }

  return true;
}

// On POSIX, the length of |pure_file_name| + |file_name_ext| is further
// restricted by NAME_MAX. The maximum allowed path looks like:
// '/path/to/save_dir' + '/' + NAME_MAX.
uint32 SavePackage::GetMaxPathLengthForDirectory(const FilePath& base_dir) {
#if defined(OS_POSIX)
  return std::min(kMaxFilePathLength,
                  static_cast<uint32>(base_dir.value().length()) +
                  NAME_MAX + 1);
#else
  return kMaxFilePathLength;
#endif
}

// File name is considered being consist of pure file name, dot and file
// extension name. File name might has no dot and file extension, or has
// multiple dot inside file name. The dot, which separates the pure file
// name and file extension name, is last dot in the whole file name.
// This function is for making sure the length of specified file path is not
// great than the specified maximum length of file path and getting safe pure
// file name part if the input pure file name is too long.
// The parameter |dir_path| specifies directory part of the specified
// file path. The parameter |file_name_ext| specifies file extension
// name part of the specified file path (including start dot). The parameter
// |max_file_path_len| specifies maximum length of the specified file path.
// The parameter |pure_file_name| input pure file name part of the specified
// file path. If the length of specified file path is great than
// |max_file_path_len|, the |pure_file_name| will output new pure file name
// part for making sure the length of specified file path is less than
// specified maximum length of file path. Return false if the function can
// not get a safe pure file name, otherwise it returns true.
bool SavePackage::GetSafePureFileName(const FilePath& dir_path,
                                      const FilePath::StringType& file_name_ext,
                                      uint32 max_file_path_len,
                                      FilePath::StringType* pure_file_name) {
  DCHECK(!pure_file_name->empty());
  int available_length = static_cast<int>(max_file_path_len -
                                          dir_path.value().length() -
                                          file_name_ext.length());
  // Need an extra space for the separator.
  if (!file_util::EndsWithSeparator(dir_path))
    --available_length;

  // Plenty of room.
  if (static_cast<int>(pure_file_name->length()) <= available_length)
    return true;

  // Limited room. Truncate |pure_file_name| to fit.
  if (available_length > 0) {
    *pure_file_name = pure_file_name->substr(0, available_length);
    return true;
  }

  // Not enough room to even use a shortened |pure_file_name|.
  pure_file_name->clear();
  return false;
}

// Generate name for saving resource.
bool SavePackage::GenerateFileName(const std::string& disposition,
                                   const GURL& url,
                                   bool need_html_ext,
                                   FilePath::StringType* generated_name) {
  // TODO(jungshik): Figure out the referrer charset when having one
  // makes sense and pass it to GetSuggestedFilename.
  string16 suggested_name =
      net::GetSuggestedFilename(url, disposition, "",
                                ASCIIToUTF16(kDefaultSaveName));

  // TODO(evan): this code is totally wrong -- we should just generate
  // Unicode filenames and do all this encoding switching at the end.
  // However, I'm just shuffling wrong code around, at least not adding
  // to it.
#if defined(OS_WIN)
  FilePath file_path = FilePath(suggested_name);
#else
  FilePath file_path = FilePath(
      base::SysWideToNativeMB(UTF16ToWide(suggested_name)));
#endif

  DCHECK(!file_path.empty());
  FilePath::StringType pure_file_name =
      file_path.RemoveExtension().BaseName().value();
  FilePath::StringType file_name_ext = file_path.Extension();

  // If it is HTML resource, use ".htm{l,}" as its extension.
  if (need_html_ext) {
    file_name_ext = FILE_PATH_LITERAL(".");
    file_name_ext.append(kDefaultHtmlExtension);
  }

  // Need to make sure the suggested file name is not too long.
  uint32 max_path = GetMaxPathLengthForDirectory(saved_main_directory_path_);

  // Get safe pure file name.
  if (!GetSafePureFileName(saved_main_directory_path_, file_name_ext,
                           max_path, &pure_file_name))
    return false;

  FilePath::StringType file_name = pure_file_name + file_name_ext;

  // Check whether we already have same name.
  if (file_name_set_.find(file_name) == file_name_set_.end()) {
    file_name_set_.insert(file_name);
  } else {
    // Found same name, increase the ordinal number for the file name.
    FilePath::StringType base_file_name = StripOrdinalNumber(pure_file_name);

    // We need to make sure the length of base file name plus maximum ordinal
    // number path will be less than or equal to kMaxFilePathLength.
    if (!GetSafePureFileName(saved_main_directory_path_, file_name_ext,
        max_path - kMaxFileOrdinalNumberPartLength, &base_file_name))
      return false;

    // Prepare the new ordinal number.
    uint32 ordinal_number;
    FileNameCountMap::iterator it = file_name_count_map_.find(base_file_name);
    if (it == file_name_count_map_.end()) {
      // First base-name-conflict resolving, use 1 as initial ordinal number.
      file_name_count_map_[base_file_name] = 1;
      ordinal_number = 1;
    } else {
      // We have met same base-name conflict, use latest ordinal number.
      ordinal_number = it->second;
    }

    if (ordinal_number > (kMaxFileOrdinalNumber - 1)) {
      // Use a random file from temporary file.
      FilePath temp_file;
      file_util::CreateTemporaryFile(&temp_file);
      file_name = temp_file.RemoveExtension().BaseName().value();
      // Get safe pure file name.
      if (!GetSafePureFileName(saved_main_directory_path_,
                               FilePath::StringType(),
                               max_path, &file_name))
        return false;
    } else {
      for (int i = ordinal_number; i < kMaxFileOrdinalNumber; ++i) {
        FilePath::StringType new_name = base_file_name +
            StringPrintf(FILE_PATH_LITERAL("(%d)"), i) + file_name_ext;
        if (file_name_set_.find(new_name) == file_name_set_.end()) {
          // Resolved name conflict.
          file_name = new_name;
          file_name_count_map_[base_file_name] = ++i;
          break;
        }
      }
    }

    file_name_set_.insert(file_name);
  }

  DCHECK(!file_name.empty());
  generated_name->assign(file_name);

  return true;
}

// We have received a message from SaveFileManager about a new saving job. We
// create a SaveItem and store it in our in_progress list.
void SavePackage::StartSave(const SaveFileCreateInfo* info) {
  DCHECK(info && !info->url.is_empty());

  SaveUrlItemMap::iterator it = in_progress_items_.find(info->url.spec());
  if (it == in_progress_items_.end()) {
    // If not found, we must have cancel action.
    DCHECK(canceled());
    return;
  }
  SaveItem* save_item = it->second;

  DCHECK(!saved_main_file_path_.empty());

  save_item->SetSaveId(info->save_id);
  save_item->SetTotalBytes(info->total_bytes);

  // Determine the proper path for a saving job, by choosing either the default
  // save directory, or prompting the user.
  DCHECK(!save_item->has_final_name());
  if (info->url != page_url_) {
    FilePath::StringType generated_name;
    // For HTML resource file, make sure it will have .htm as extension name,
    // otherwise, when you open the saved page in Chrome again, download
    // file manager will treat it as downloadable resource, and download it
    // instead of opening it as HTML.
    bool need_html_ext =
        info->save_source == SaveFileCreateInfo::SAVE_FILE_FROM_DOM;
    if (!GenerateFileName(info->content_disposition,
                          GURL(info->url),
                          need_html_ext,
                          &generated_name)) {
      // We can not generate file name for this SaveItem, so we cancel the
      // saving page job if the save source is from serialized DOM data.
      // Otherwise, it means this SaveItem is sub-resource type, we treat it
      // as an error happened on saving. We can ignore this type error for
      // sub-resource links which will be resolved as absolute links instead
      // of local links in final saved contents.
      if (info->save_source == SaveFileCreateInfo::SAVE_FILE_FROM_DOM)
        Cancel(true);
      else
        SaveFinished(save_item->save_id(), 0, false);
      return;
    }

    // When saving page as only-HTML, we only have a SaveItem whose url
    // must be page_url_.
    DCHECK(save_type_ == SAVE_AS_COMPLETE_HTML);
    DCHECK(!saved_main_directory_path_.empty());

    // Now we get final name retrieved from GenerateFileName, we will use it
    // rename the SaveItem.
    FilePath final_name = saved_main_directory_path_.Append(generated_name);
    save_item->Rename(final_name);
  } else {
    // It is the main HTML file, use the name chosen by the user.
    save_item->Rename(saved_main_file_path_);
  }

  // If the save source is from file system, inform SaveFileManager to copy
  // corresponding file to the file path which this SaveItem specifies.
  if (info->save_source == SaveFileCreateInfo::SAVE_FILE_FROM_FILE) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(file_manager_,
                          &SaveFileManager::SaveLocalFile,
                          save_item->url(),
                          save_item->save_id(),
                          tab_id()));
    return;
  }

  // Check whether we begin to require serialized HTML data.
  if (save_type_ == SAVE_AS_COMPLETE_HTML && wait_state_ == HTML_DATA) {
    // Inform backend to serialize the all frames' DOM and send serialized
    // HTML data back.
    GetSerializedHtmlDataForCurrentPageWithLocalLinks();
  }
}

// Look up SaveItem by save id from in progress map.
SaveItem* SavePackage::LookupItemInProcessBySaveId(int32 save_id) {
  if (in_process_count()) {
    for (SaveUrlItemMap::iterator it = in_progress_items_.begin();
        it != in_progress_items_.end(); ++it) {
      SaveItem* save_item = it->second;
      DCHECK(save_item->state() == SaveItem::IN_PROGRESS);
      if (save_item->save_id() == save_id)
        return save_item;
    }
  }
  return NULL;
}

// Remove SaveItem from in progress map and put it to saved map.
void SavePackage::PutInProgressItemToSavedMap(SaveItem* save_item) {
  SaveUrlItemMap::iterator it = in_progress_items_.find(
      save_item->url().spec());
  DCHECK(it != in_progress_items_.end());
  DCHECK(save_item == it->second);
  in_progress_items_.erase(it);

  if (save_item->success()) {
    // Add it to saved_success_items_.
    DCHECK(saved_success_items_.find(save_item->save_id()) ==
           saved_success_items_.end());
    saved_success_items_[save_item->save_id()] = save_item;
  } else {
    // Add it to saved_failed_items_.
    DCHECK(saved_failed_items_.find(save_item->url().spec()) ==
           saved_failed_items_.end());
    saved_failed_items_[save_item->url().spec()] = save_item;
  }
}

// Called for updating saving state.
bool SavePackage::UpdateSaveProgress(int32 save_id,
                                     int64 size,
                                     bool write_success) {
  // Because we might have canceled this saving job before,
  // so we might not find corresponding SaveItem.
  SaveItem* save_item = LookupItemInProcessBySaveId(save_id);
  if (!save_item)
    return false;

  save_item->Update(size);

  // If we got disk error, cancel whole save page job.
  if (!write_success) {
    // Cancel job with reason of disk error.
    Cancel(false);
  }
  return true;
}

// Stop all page saving jobs that are in progress and instruct the file thread
// to delete all saved  files.
void SavePackage::Stop() {
  // If we haven't moved out of the initial state, there's nothing to cancel and
  // there won't be valid pointers for file_manager_ or download_.
  if (wait_state_ == INITIALIZE)
    return;

  // When stopping, if it still has some items in in_progress, cancel them.
  DCHECK(canceled());
  if (in_process_count()) {
    SaveUrlItemMap::iterator it = in_progress_items_.begin();
    for (; it != in_progress_items_.end(); ++it) {
      SaveItem* save_item = it->second;
      DCHECK(save_item->state() == SaveItem::IN_PROGRESS);
      save_item->Cancel();
    }
    // Remove all in progress item to saved map. For failed items, they will
    // be put into saved_failed_items_, for successful item, they will be put
    // into saved_success_items_.
    while (in_process_count())
      PutInProgressItemToSavedMap(in_progress_items_.begin()->second);
  }

  // This vector contains the save ids of the save files which SaveFileManager
  // needs to remove from its save_file_map_.
  SaveIDList save_ids;
  for (SavedItemMap::iterator it = saved_success_items_.begin();
      it != saved_success_items_.end(); ++it)
    save_ids.push_back(it->first);
  for (SaveUrlItemMap::iterator it = saved_failed_items_.begin();
      it != saved_failed_items_.end(); ++it)
    save_ids.push_back(it->second->save_id());

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(file_manager_,
                        &SaveFileManager::RemoveSavedFileFromFileMap,
                        save_ids));

  finished_ = true;
  wait_state_ = FAILED;

  // Inform the DownloadItem we have canceled whole save page job.
  download_->Cancel(false);
}

void SavePackage::CheckFinish() {
  if (in_process_count() || finished_)
    return;

  FilePath dir = (save_type_ == SAVE_AS_COMPLETE_HTML &&
                  saved_success_items_.size() > 1) ?
                  saved_main_directory_path_ : FilePath();

  // This vector contains the final names of all the successfully saved files
  // along with their save ids. It will be passed to SaveFileManager to do the
  // renaming job.
  FinalNameList final_names;
  for (SavedItemMap::iterator it = saved_success_items_.begin();
      it != saved_success_items_.end(); ++it)
    final_names.push_back(std::make_pair(it->first,
                                         it->second->full_path()));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(file_manager_,
                        &SaveFileManager::RenameAllFiles,
                        final_names,
                        dir,
                        tab_contents()->GetRenderProcessHost()->id(),
                        tab_contents()->render_view_host()->routing_id(),
                        id()));
}

// Successfully finished all items of this SavePackage.
void SavePackage::Finish() {
  // User may cancel the job when we're moving files to the final directory.
  if (canceled())
    return;

  wait_state_ = SUCCESSFUL;
  finished_ = true;

  // This vector contains the save ids of the save files which SaveFileManager
  // needs to remove from its save_file_map_.
  SaveIDList save_ids;
  for (SaveUrlItemMap::iterator it = saved_failed_items_.begin();
       it != saved_failed_items_.end(); ++it)
    save_ids.push_back(it->second->save_id());

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(file_manager_,
                        &SaveFileManager::RemoveSavedFileFromFileMap,
                        save_ids));

  download_->OnAllDataSaved(all_save_items_count_);
  download_->MarkAsComplete();
  // Notify download observers that we are complete (the call
  // to OnReadyToFinish() set the state to complete but did not notify).
  download_->UpdateObservers();

  NotificationService::current()->Notify(
      NotificationType::SAVE_PACKAGE_SUCCESSFULLY_FINISHED,
      Source<SavePackage>(this),
      Details<GURL>(&page_url_));
}

// Called for updating end state.
void SavePackage::SaveFinished(int32 save_id, int64 size, bool is_success) {
  // Because we might have canceled this saving job before,
  // so we might not find corresponding SaveItem. Just ignore it.
  SaveItem* save_item = LookupItemInProcessBySaveId(save_id);
  if (!save_item)
    return;

  // Let SaveItem set end state.
  save_item->Finish(size, is_success);
  // Remove the associated save id and SavePackage.
  file_manager_->RemoveSaveFile(save_id, save_item->url(), this);

  PutInProgressItemToSavedMap(save_item);

  // Inform the DownloadItem to update UI.
  // We use the received bytes as number of saved files.
  download_->Update(completed_count());

  if (save_item->save_source() == SaveFileCreateInfo::SAVE_FILE_FROM_DOM &&
      save_item->url() == page_url_ && !save_item->received_bytes()) {
    // If size of main HTML page is 0, treat it as disk error.
    Cancel(false);
    return;
  }

  if (canceled()) {
    DCHECK(finished_);
    return;
  }

  // Continue processing the save page job.
  DoSavingProcess();

  // Check whether we can successfully finish whole job.
  CheckFinish();
}

// Sometimes, the net io will only call SaveFileManager::SaveFinished with
// save id -1 when it encounters error. Since in this case, save id will be
// -1, so we can only use URL to find which SaveItem is associated with
// this error.
// Saving an item failed. If it's a sub-resource, ignore it. If the error comes
// from serializing HTML data, then cancel saving page.
void SavePackage::SaveFailed(const GURL& save_url) {
  SaveUrlItemMap::iterator it = in_progress_items_.find(save_url.spec());
  if (it == in_progress_items_.end()) {
    NOTREACHED();  // Should not exist!
    return;
  }
  SaveItem* save_item = it->second;

  save_item->Finish(0, false);

  PutInProgressItemToSavedMap(save_item);

  // Inform the DownloadItem to update UI.
  // We use the received bytes as number of saved files.
  download_->Update(completed_count());

  if (save_type_ == SAVE_AS_ONLY_HTML ||
      save_item->save_source() == SaveFileCreateInfo::SAVE_FILE_FROM_DOM) {
    // We got error when saving page. Treat it as disk error.
    Cancel(true);
  }

  if (canceled()) {
    DCHECK(finished_);
    return;
  }

  // Continue processing the save page job.
  DoSavingProcess();

  CheckFinish();
}

void SavePackage::SaveCanceled(SaveItem* save_item) {
  // Call the RemoveSaveFile in UI thread.
  file_manager_->RemoveSaveFile(save_item->save_id(),
                                save_item->url(),
                                this);
  if (save_item->save_id() != -1)
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(file_manager_,
                          &SaveFileManager::CancelSave,
                          save_item->save_id()));
}

// Initiate a saving job of a specific URL. We send the request to
// SaveFileManager, which will dispatch it to different approach according to
// the save source. Parameter process_all_remaining_items indicates whether
// we need to save all remaining items.
void SavePackage::SaveNextFile(bool process_all_remaining_items) {
  DCHECK(tab_contents());
  DCHECK(waiting_item_queue_.size());

  do {
    // Pop SaveItem from waiting list.
    SaveItem* save_item = waiting_item_queue_.front();
    waiting_item_queue_.pop();

    // Add the item to in_progress_items_.
    SaveUrlItemMap::iterator it = in_progress_items_.find(
        save_item->url().spec());
    DCHECK(it == in_progress_items_.end());
    in_progress_items_[save_item->url().spec()] = save_item;
    save_item->Start();
    file_manager_->SaveURL(save_item->url(),
                           save_item->referrer(),
                           tab_contents()->GetRenderProcessHost()->id(),
                           routing_id(),
                           save_item->save_source(),
                           save_item->full_path(),
                           request_context_getter_.get(),
                           this);
  } while (process_all_remaining_items && waiting_item_queue_.size());
}


// Open download page in windows explorer on file thread, to avoid blocking the
// user interface.
void SavePackage::ShowDownloadInShell() {
  DCHECK(file_manager_);
  DCHECK(finished_ && !canceled() && !saved_main_file_path_.empty());
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(OS_MACOSX)
  // Mac OS X requires opening downloads on the UI thread.
  platform_util::ShowItemInFolder(saved_main_file_path_);
#else
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(file_manager_,
                        &SaveFileManager::OnShowSavedFileInShell,
                        saved_main_file_path_));
#endif
}

// Calculate the percentage of whole save page job.
int SavePackage::PercentComplete() {
  if (!all_save_items_count_)
    return 0;
  else if (!in_process_count())
    return 100;
  else
    return completed_count() / all_save_items_count_;
}

// Continue processing the save page job after one SaveItem has been
// finished.
void SavePackage::DoSavingProcess() {
  if (save_type_ == SAVE_AS_COMPLETE_HTML) {
    // We guarantee that images and JavaScripts must be downloaded first.
    // So when finishing all those sub-resources, we will know which
    // sub-resource's link can be replaced with local file path, which
    // sub-resource's link need to be replaced with absolute URL which
    // point to its internet address because it got error when saving its data.
    SaveItem* save_item = NULL;
    // Start a new SaveItem job if we still have job in waiting queue.
    if (waiting_item_queue_.size()) {
      DCHECK(wait_state_ == NET_FILES);
      save_item = waiting_item_queue_.front();
      if (save_item->save_source() != SaveFileCreateInfo::SAVE_FILE_FROM_DOM) {
        SaveNextFile(false);
      } else if (!in_process_count()) {
        // If there is no in-process SaveItem, it means all sub-resources
        // have been processed. Now we need to start serializing HTML DOM
        // for the current page to get the generated HTML data.
        wait_state_ = HTML_DATA;
        // All non-HTML resources have been finished, start all remaining
        // HTML files.
        SaveNextFile(true);
      }
    } else if (in_process_count()) {
      // Continue asking for HTML data.
      DCHECK(wait_state_ == HTML_DATA);
    }
  } else {
    // Save as HTML only.
    DCHECK(wait_state_ == NET_FILES);
    DCHECK(save_type_ == SAVE_AS_ONLY_HTML);
    if (waiting_item_queue_.size()) {
      DCHECK(all_save_items_count_ == waiting_item_queue_.size());
      SaveNextFile(false);
    }
  }
}

bool SavePackage::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SavePackage, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SendCurrentPageAllSavableResourceLinks,
                        OnReceivedSavableResourceLinksForCurrentPage)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SendSerializedHtmlData,
                        OnReceivedSerializedHtmlData)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// After finishing all SaveItems which need to get data from net.
// We collect all URLs which have local storage and send the
// map:(originalURL:currentLocalPath) to render process (backend).
// Then render process will serialize DOM and send data to us.
void SavePackage::GetSerializedHtmlDataForCurrentPageWithLocalLinks() {
  if (wait_state_ != HTML_DATA)
    return;
  std::vector<GURL> saved_links;
  std::vector<FilePath> saved_file_paths;
  int successful_started_items_count = 0;

  // Collect all saved items which have local storage.
  // First collect the status of all the resource files and check whether they
  // have created local files although they have not been completely saved.
  // If yes, the file can be saved. Otherwise, there is a disk error, so we
  // need to cancel the page saving job.
  for (SaveUrlItemMap::iterator it = in_progress_items_.begin();
       it != in_progress_items_.end(); ++it) {
    DCHECK(it->second->save_source() ==
           SaveFileCreateInfo::SAVE_FILE_FROM_DOM);
    if (it->second->has_final_name())
      successful_started_items_count++;
    saved_links.push_back(it->second->url());
    saved_file_paths.push_back(it->second->file_name());
  }

  // If not all file of HTML resource have been started, then wait.
  if (successful_started_items_count != in_process_count())
    return;

  // Collect all saved success items.
  for (SavedItemMap::iterator it = saved_success_items_.begin();
       it != saved_success_items_.end(); ++it) {
    DCHECK(it->second->has_final_name());
    saved_links.push_back(it->second->url());
    saved_file_paths.push_back(it->second->file_name());
  }

  // Get the relative directory name.
  FilePath relative_dir_name = saved_main_directory_path_.BaseName();

  tab_contents()->render_view_host()->
      GetSerializedHtmlDataForCurrentPageWithLocalLinks(
      saved_links, saved_file_paths, relative_dir_name);
}

// Process the serialized HTML content data of a specified web page
// retrieved from render process.
void SavePackage::OnReceivedSerializedHtmlData(const GURL& frame_url,
                                               const std::string& data,
                                               int32 status) {
  WebPageSerializerClient::PageSerializationStatus flag =
      static_cast<WebPageSerializerClient::PageSerializationStatus>(status);
  // Check current state.
  if (wait_state_ != HTML_DATA)
    return;

  int id = tab_id();
  // If the all frames are finished saving, we need to close the
  // remaining SaveItems.
  if (flag == WebPageSerializerClient::AllFramesAreFinished) {
    for (SaveUrlItemMap::iterator it = in_progress_items_.begin();
         it != in_progress_items_.end(); ++it) {
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(file_manager_,
                            &SaveFileManager::SaveFinished,
                            it->second->save_id(),
                            it->second->url(),
                            id,
                            true));
    }
    return;
  }

  SaveUrlItemMap::iterator it = in_progress_items_.find(frame_url.spec());
  if (it == in_progress_items_.end())
    return;
  SaveItem* save_item = it->second;
  DCHECK(save_item->save_source() == SaveFileCreateInfo::SAVE_FILE_FROM_DOM);

  if (!data.empty()) {
    // Prepare buffer for saving HTML data.
    scoped_refptr<net::IOBuffer> new_data(new net::IOBuffer(data.size()));
    memcpy(new_data->data(), data.data(), data.size());

    // Call write file functionality in file thread.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(file_manager_,
                          &SaveFileManager::UpdateSaveProgress,
                          save_item->save_id(),
                          new_data,
                          static_cast<int>(data.size())));
  }

  // Current frame is completed saving, call finish in file thread.
  if (flag == WebPageSerializerClient::CurrentFrameIsFinished) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(file_manager_,
                          &SaveFileManager::SaveFinished,
                          save_item->save_id(),
                          save_item->url(),
                          id,
                          true));
  }
}

// Ask for all savable resource links from backend, include main frame and
// sub-frame.
void SavePackage::GetAllSavableResourceLinksForCurrentPage() {
  if (wait_state_ != START_PROCESS)
    return;

  wait_state_ = RESOURCES_LIST;
  tab_contents()->render_view_host()->
      GetAllSavableResourceLinksForCurrentPage(page_url_);
}

// Give backend the lists which contain all resource links that have local
// storage, after which, render process will serialize DOM for generating
// HTML data.
void SavePackage::OnReceivedSavableResourceLinksForCurrentPage(
    const std::vector<GURL>& resources_list,
    const std::vector<GURL>& referrers_list,
    const std::vector<GURL>& frames_list) {
  if (wait_state_ != RESOURCES_LIST)
    return;

  DCHECK(resources_list.size() == referrers_list.size());
  all_save_items_count_ = static_cast<int>(resources_list.size()) +
                           static_cast<int>(frames_list.size());

  // We use total bytes as the total number of files we want to save.
  download_->set_total_bytes(all_save_items_count_);

  if (all_save_items_count_) {
    // Put all sub-resources to wait list.
    for (int i = 0; i < static_cast<int>(resources_list.size()); ++i) {
      const GURL& u = resources_list[i];
      DCHECK(u.is_valid());
      SaveFileCreateInfo::SaveFileSource save_source = u.SchemeIsFile() ?
          SaveFileCreateInfo::SAVE_FILE_FROM_FILE :
          SaveFileCreateInfo::SAVE_FILE_FROM_NET;
      SaveItem* save_item = new SaveItem(u, referrers_list[i],
                                         this, save_source);
      waiting_item_queue_.push(save_item);
    }
    // Put all HTML resources to wait list.
    for (int i = 0; i < static_cast<int>(frames_list.size()); ++i) {
      const GURL& u = frames_list[i];
      DCHECK(u.is_valid());
      SaveItem* save_item = new SaveItem(u, GURL(),
          this, SaveFileCreateInfo::SAVE_FILE_FROM_DOM);
      waiting_item_queue_.push(save_item);
    }
    wait_state_ = NET_FILES;
    DoSavingProcess();
  } else {
    // No resource files need to be saved, treat it as user cancel.
    Cancel(true);
  }
}

void SavePackage::SetShouldPromptUser(bool should_prompt) {
  g_should_prompt_for_filename = should_prompt;
}

FilePath SavePackage::GetSuggestedNameForSaveAs(
    bool can_save_as_complete,
    const std::string& contents_mime_type) {
  FilePath name_with_proper_ext =
      FilePath::FromWStringHack(UTF16ToWideHack(title_));

  // If the page's title matches its URL, use the URL. Try to use the last path
  // component or if there is none, the domain as the file name.
  // Normally we want to base the filename on the page title, or if it doesn't
  // exist, on the URL. It's not easy to tell if the page has no title, because
  // if the page has no title, TabContents::GetTitle() will return the page's
  // URL (adjusted for display purposes). Therefore, we convert the "title"
  // back to a URL, and if it matches the original page URL, we know the page
  // had no title (or had a title equal to its URL, which is fine to treat
  // similarly).
  GURL fixed_up_title_url =
      URLFixerUpper::FixupURL(UTF16ToUTF8(title_), std::string());

  if (page_url_ == fixed_up_title_url) {
    std::string url_path;
    std::vector<std::string> url_parts;
    base::SplitString(page_url_.path(), '/', &url_parts);
    if (!url_parts.empty()) {
      for (int i = static_cast<int>(url_parts.size()) - 1; i >= 0; --i) {
        url_path = url_parts[i];
        if (!url_path.empty())
          break;
      }
    }
    if (url_path.empty())
      url_path = page_url_.host();
    name_with_proper_ext = FilePath::FromWStringHack(UTF8ToWide(url_path));
  }

  // Ask user for getting final saving name.
  name_with_proper_ext = EnsureMimeExtension(name_with_proper_ext,
                                             contents_mime_type);
  // Adjust extension for complete types.
  if (can_save_as_complete)
    name_with_proper_ext = EnsureHtmlExtension(name_with_proper_ext);

  FilePath::StringType file_name = name_with_proper_ext.value();
  file_util::ReplaceIllegalCharactersInPath(&file_name, ' ');
  return FilePath(file_name);
}

FilePath SavePackage::EnsureHtmlExtension(const FilePath& name) {
  // If the file name doesn't have an extension suitable for HTML files,
  // append one.
  FilePath::StringType ext = name.Extension();
  if (!ext.empty())
    ext.erase(ext.begin());  // Erase preceding '.'.
  std::string mime_type;
  if (!net::GetMimeTypeFromExtension(ext, &mime_type) ||
      !CanSaveAsComplete(mime_type)) {
    return FilePath(name.value() + FILE_PATH_LITERAL(".") +
                    kDefaultHtmlExtension);
  }
  return name;
}

FilePath SavePackage::EnsureMimeExtension(const FilePath& name,
    const std::string& contents_mime_type) {
  // Start extension at 1 to skip over period if non-empty.
  FilePath::StringType ext = name.Extension().length() ?
      name.Extension().substr(1) : name.Extension();
  FilePath::StringType suggested_extension =
      ExtensionForMimeType(contents_mime_type);
  std::string mime_type;
  if (!suggested_extension.empty() &&
      (!net::GetMimeTypeFromExtension(ext, &mime_type) ||
      !IsSavableContents(mime_type))) {
    // Extension is absent or needs to be updated.
    return FilePath(name.value() + FILE_PATH_LITERAL(".") +
                    suggested_extension);
  }
  return name;
}

const FilePath::CharType* SavePackage::ExtensionForMimeType(
    const std::string& contents_mime_type) {
  static const struct {
    const FilePath::CharType *mime_type;
    const FilePath::CharType *suggested_extension;
  } extensions[] = {
    { FILE_PATH_LITERAL("text/html"), kDefaultHtmlExtension },
    { FILE_PATH_LITERAL("text/xml"), FILE_PATH_LITERAL("xml") },
    { FILE_PATH_LITERAL("application/xhtml+xml"), FILE_PATH_LITERAL("xhtml") },
    { FILE_PATH_LITERAL("text/plain"), FILE_PATH_LITERAL("txt") },
    { FILE_PATH_LITERAL("text/css"), FILE_PATH_LITERAL("css") },
  };
#if defined(OS_POSIX)
  FilePath::StringType mime_type(contents_mime_type);
#elif defined(OS_WIN)
  FilePath::StringType mime_type(UTF8ToWide(contents_mime_type));
#endif  // OS_WIN
  for (uint32 i = 0; i < ARRAYSIZE_UNSAFE(extensions); ++i) {
    if (mime_type == extensions[i].mime_type)
      return extensions[i].suggested_extension;
  }
  return FILE_PATH_LITERAL("");
}



// static.
// Check whether the preference has the preferred directory for saving file. If
// not, initialize it with default directory.
FilePath SavePackage::GetSaveDirPreference(PrefService* prefs) {
  DCHECK(prefs);

  if (!prefs->FindPreference(prefs::kSaveFileDefaultDirectory)) {
    DCHECK(prefs->FindPreference(prefs::kDownloadDefaultDirectory));
    FilePath default_save_path = prefs->GetFilePath(
        prefs::kDownloadDefaultDirectory);
    prefs->RegisterFilePathPref(prefs::kSaveFileDefaultDirectory,
                                default_save_path);
  }

  // Get the directory from preference.
  FilePath save_file_path = prefs->GetFilePath(
      prefs::kSaveFileDefaultDirectory);
  DCHECK(!save_file_path.empty());

  return save_file_path;
}

void SavePackage::GetSaveInfo() {
  // Can't use tab_contents_ in the file thread, so get the data that we need
  // before calling to it.
  PrefService* prefs = tab_contents()->profile()->GetPrefs();
  FilePath website_save_dir = GetSaveDirPreference(prefs);
  FilePath download_save_dir = prefs->GetFilePath(
      prefs::kDownloadDefaultDirectory);
  std::string mime_type = tab_contents()->contents_mime_type();

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &SavePackage::CreateDirectoryOnFileThread,
          website_save_dir, download_save_dir, mime_type));
}

void SavePackage::CreateDirectoryOnFileThread(
    const FilePath& website_save_dir,
    const FilePath& download_save_dir,
    const std::string& mime_type) {
  FilePath save_dir;
  // If the default html/websites save folder doesn't exist...
  if (!file_util::DirectoryExists(website_save_dir)) {
    // If the default download dir doesn't exist, create it.
    if (!file_util::DirectoryExists(download_save_dir))
      file_util::CreateDirectory(download_save_dir);
    save_dir = download_save_dir;
  } else {
    // If it does exist, use the default save dir param.
    save_dir = website_save_dir;
  }

  bool can_save_as_complete = CanSaveAsComplete(mime_type);
  FilePath suggested_filename = GetSuggestedNameForSaveAs(can_save_as_complete,
                                                          mime_type);
  FilePath::StringType pure_file_name =
      suggested_filename.RemoveExtension().BaseName().value();
  FilePath::StringType file_name_ext = suggested_filename.Extension();

  // Need to make sure the suggested file name is not too long.
  uint32 max_path = GetMaxPathLengthForDirectory(save_dir);

  if (GetSafePureFileName(save_dir, file_name_ext, max_path, &pure_file_name)) {
    save_dir = save_dir.Append(pure_file_name + file_name_ext);
  } else {
    // Cannot create a shorter filename. This will cause the save as operation
    // to fail unless the user pick a shorter name. Continuing even though it
    // will fail because returning means no save as popup for the user, which
    // is even more confusing. This case should be rare though.
    save_dir = save_dir.Append(suggested_filename);
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &SavePackage::ContinueGetSaveInfo, save_dir,
                        can_save_as_complete));
}

void SavePackage::ContinueGetSaveInfo(const FilePath& suggested_path,
                                      bool can_save_as_complete) {
  DownloadPrefs* download_prefs =
      tab_contents()->profile()->GetDownloadManager()->download_prefs();
  int file_type_index =
      SavePackageTypeToIndex(
          static_cast<SavePackageType>(download_prefs->save_file_type()));

  SelectFileDialog::FileTypeInfo file_type_info;
  FilePath::StringType default_extension;

  // If the contents can not be saved as complete-HTML, do not show the
  // file filters.
  if (can_save_as_complete) {
    bool add_extra_extension = false;
    FilePath::StringType extra_extension;
    if (!suggested_path.Extension().empty() &&
        suggested_path.Extension().compare(FILE_PATH_LITERAL("htm")) &&
        suggested_path.Extension().compare(FILE_PATH_LITERAL("html"))) {
      add_extra_extension = true;
      extra_extension = suggested_path.Extension().substr(1);
    }

    file_type_info.extensions.resize(2);
    file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1].push_back(
        FILE_PATH_LITERAL("htm"));
    file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1].push_back(
        FILE_PATH_LITERAL("html"));

    if (add_extra_extension) {
      file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1].push_back(
          extra_extension);
    }

    file_type_info.extension_description_overrides.push_back(
        l10n_util::GetStringUTF16(kIndexToIDS[kSelectFileCompleteIndex - 1]));
    file_type_info.extensions[kSelectFileCompleteIndex - 1].push_back(
        FILE_PATH_LITERAL("htm"));
    file_type_info.extensions[kSelectFileCompleteIndex - 1].push_back(
        FILE_PATH_LITERAL("html"));

    if (add_extra_extension) {
      file_type_info.extensions[kSelectFileCompleteIndex - 1].push_back(
          extra_extension);
    }

    file_type_info.extension_description_overrides.push_back(
        l10n_util::GetStringUTF16(kIndexToIDS[kSelectFileCompleteIndex]));
    file_type_info.include_all_files = false;
    default_extension = kDefaultHtmlExtension;
  } else {
    file_type_info.extensions.resize(1);
    file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1].push_back(
        suggested_path.Extension());

    if (!file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1][0].empty()) {
      // Drop the .
      file_type_info.extensions[kSelectFileHtmlOnlyIndex - 1][0].erase(0, 1);
    }

    file_type_info.include_all_files = true;
    file_type_index = 1;
  }

  if (g_should_prompt_for_filename) {
    if (!select_file_dialog_.get())
      select_file_dialog_ = SelectFileDialog::Create(this);
    select_file_dialog_->SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE,
                                    string16(),
                                    suggested_path,
                                    &file_type_info,
                                    file_type_index,
                                    default_extension,
                                    platform_util::GetTopLevel(
                                        tab_contents()->GetNativeView()),
                                    NULL);
  } else {
    // Just use 'suggested_path' instead of opening the dialog prompt.
    ContinueSave(suggested_path, file_type_index);
  }
}

// Called after the save file dialog box returns.
void SavePackage::ContinueSave(const FilePath& final_name,
                               int index) {
  // Ensure the filename is safe.
  saved_main_file_path_ = final_name;
  download_util::GenerateSafeFileName(tab_contents()->contents_mime_type(),
                                      &saved_main_file_path_);

  // The option index is not zero-based.
  DCHECK(index >= kSelectFileHtmlOnlyIndex &&
         index <= kSelectFileCompleteIndex);

  saved_main_directory_path_ = saved_main_file_path_.DirName();

  PrefService* prefs = tab_contents()->profile()->GetPrefs();
  StringPrefMember save_file_path;
  save_file_path.Init(prefs::kSaveFileDefaultDirectory, prefs, NULL);
#if defined(OS_POSIX)
  std::string path_string = saved_main_directory_path_.value();
#elif defined(OS_WIN)
  std::string path_string = WideToUTF8(saved_main_directory_path_.value());
#endif
  // If user change the default saving directory, we will remember it just
  // like IE and FireFox.
  if (!tab_contents()->profile()->IsOffTheRecord() &&
      save_file_path.GetValue() != path_string) {
    save_file_path.SetValue(path_string);
  }

  save_type_ = kIndexToSaveType[index];

  prefs->SetInteger(prefs::kSaveFileType, save_type_);

  if (save_type_ == SavePackage::SAVE_AS_COMPLETE_HTML) {
    // Make new directory for saving complete file.
    saved_main_directory_path_ = saved_main_directory_path_.Append(
        saved_main_file_path_.RemoveExtension().BaseName().value() +
        FILE_PATH_LITERAL("_files"));
  }

  Init();
}

// Static
bool SavePackage::IsSavableURL(const GURL& url) {
  for (int i = 0; chrome::kSavableSchemes[i] != NULL; ++i) {
    if (url.SchemeIs(chrome::kSavableSchemes[i])) {
      return true;
    }
  }
  return false;
}

// Static
bool SavePackage::IsSavableContents(const std::string& contents_mime_type) {
  // WebKit creates Document object when MIME type is application/xhtml+xml,
  // so we also support this MIME type.
  return contents_mime_type == "text/html" ||
         contents_mime_type == "text/xml" ||
         contents_mime_type == "application/xhtml+xml" ||
         contents_mime_type == "text/plain" ||
         contents_mime_type == "text/css" ||
         net::IsSupportedJavascriptMimeType(contents_mime_type.c_str());
}

// SelectFileDialog::Listener interface.
void SavePackage::FileSelected(const FilePath& path,
                               int index, void* params) {
  ContinueSave(path, index);
}

void SavePackage::FileSelectionCanceled(void* params) {
}
