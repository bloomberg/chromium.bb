// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_package.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "content/browser/bad_message.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/download/save_file.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/download/save_item.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_util.h"
#include "net/url_request/url_request_context.h"
#include "url/url_constants.h"

using base::Time;

namespace content {
namespace {

// Generates unique ids for SavePackage::unique_id_ field.
SavePackageId GetNextSavePackageId() {
  static int g_save_package_id = 0;
  return SavePackageId::FromUnsafeValue(g_save_package_id++);
}

// Default name which will be used when we can not get proper name from
// resource URL.
const char kDefaultSaveName[] = "saved_resource";

// Maximum number of file ordinal number. I think it's big enough for resolving
// name-conflict files which has same base file name.
const int32_t kMaxFileOrdinalNumber = 9999;

// Maximum length for file path. Since Windows have MAX_PATH limitation for
// file path, we need to make sure length of file path of every saved file
// is less than MAX_PATH
#if defined(OS_WIN)
const uint32_t kMaxFilePathLength = MAX_PATH - 1;
#elif defined(OS_POSIX)
const uint32_t kMaxFilePathLength = PATH_MAX - 1;
#endif

// Maximum length for file ordinal number part. Since we only support the
// maximum 9999 for ordinal number, which means maximum file ordinal number part
// should be "(9998)", so the value is 6.
const uint32_t kMaxFileOrdinalNumberPartLength = 6;

// Strip current ordinal number, if any. Should only be used on pure
// file names, i.e. those stripped of their extensions.
// TODO(estade): improve this to not choke on alternate encodings.
base::FilePath::StringType StripOrdinalNumber(
    const base::FilePath::StringType& pure_file_name) {
  base::FilePath::StringType::size_type r_paren_index =
      pure_file_name.rfind(FILE_PATH_LITERAL(')'));
  base::FilePath::StringType::size_type l_paren_index =
      pure_file_name.rfind(FILE_PATH_LITERAL('('));
  if (l_paren_index >= r_paren_index)
    return pure_file_name;

  for (base::FilePath::StringType::size_type i = l_paren_index + 1;
       i != r_paren_index; ++i) {
    if (!base::IsAsciiDigit(pure_file_name[i]))
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

// Request handle for SavePackage downloads. Currently doesn't support
// pause/resume/cancel, but returns a WebContents.
class SavePackageRequestHandle : public DownloadRequestHandleInterface {
 public:
  SavePackageRequestHandle(base::WeakPtr<SavePackage> save_package)
      : save_package_(save_package) {}

  // DownloadRequestHandleInterface
  WebContents* GetWebContents() const override {
    return save_package_.get() ? save_package_->web_contents() : NULL;
  }
  DownloadManager* GetDownloadManager() const override { return NULL; }
  void PauseRequest() const override {}
  void ResumeRequest() const override {}
  void CancelRequest() const override {}
  std::string DebugString() const override {
    return "SavePackage DownloadRequestHandle";
  }

 private:
  base::WeakPtr<SavePackage> save_package_;
};

}  // namespace

const base::FilePath::CharType SavePackage::kDefaultHtmlExtension[] =
    FILE_PATH_LITERAL("html");

SavePackage::SavePackage(WebContents* web_contents,
                         SavePageType save_type,
                         const base::FilePath& file_full_path,
                         const base::FilePath& directory_full_path)
    : WebContentsObserver(web_contents),
      number_of_frames_pending_response_(0),
      file_manager_(NULL),
      download_manager_(NULL),
      download_(NULL),
      page_url_(GetUrlToBeSaved()),
      saved_main_file_path_(file_full_path),
      saved_main_directory_path_(directory_full_path),
      title_(web_contents->GetTitle()),
      start_tick_(base::TimeTicks::Now()),
      finished_(false),
      mhtml_finishing_(false),
      user_canceled_(false),
      disk_error_occurred_(false),
      save_type_(save_type),
      all_save_items_count_(0),
      file_name_set_(&base::FilePath::CompareLessIgnoreCase),
      wait_state_(INITIALIZE),
      unique_id_(GetNextSavePackageId()),
      wrote_to_completed_file_(false),
      wrote_to_failed_file_(false) {
  DCHECK(page_url_.is_valid());
  DCHECK((save_type_ == SAVE_PAGE_TYPE_AS_ONLY_HTML) ||
         (save_type_ == SAVE_PAGE_TYPE_AS_MHTML) ||
         (save_type_ == SAVE_PAGE_TYPE_AS_COMPLETE_HTML))
      << save_type_;
  DCHECK(!saved_main_file_path_.empty() &&
         saved_main_file_path_.value().length() <= kMaxFilePathLength);
  DCHECK(!saved_main_directory_path_.empty() &&
         saved_main_directory_path_.value().length() < kMaxFilePathLength);
  InternalInit();
}

SavePackage::SavePackage(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      number_of_frames_pending_response_(0),
      file_manager_(NULL),
      download_manager_(NULL),
      download_(NULL),
      page_url_(GetUrlToBeSaved()),
      title_(web_contents->GetTitle()),
      start_tick_(base::TimeTicks::Now()),
      finished_(false),
      mhtml_finishing_(false),
      user_canceled_(false),
      disk_error_occurred_(false),
      save_type_(SAVE_PAGE_TYPE_UNKNOWN),
      all_save_items_count_(0),
      file_name_set_(&base::FilePath::CompareLessIgnoreCase),
      wait_state_(INITIALIZE),
      unique_id_(GetNextSavePackageId()),
      wrote_to_completed_file_(false),
      wrote_to_failed_file_(false) {
  DCHECK(page_url_.is_valid());
  InternalInit();
}

// This is for testing use. Set |finished_| as true because we don't want
// method Cancel to be be called in destructor in test mode.
// We also don't call InternalInit().
SavePackage::SavePackage(WebContents* web_contents,
                         const base::FilePath& file_full_path,
                         const base::FilePath& directory_full_path)
    : WebContentsObserver(web_contents),
      number_of_frames_pending_response_(0),
      file_manager_(NULL),
      download_manager_(NULL),
      download_(NULL),
      saved_main_file_path_(file_full_path),
      saved_main_directory_path_(directory_full_path),
      start_tick_(base::TimeTicks::Now()),
      finished_(true),
      mhtml_finishing_(false),
      user_canceled_(false),
      disk_error_occurred_(false),
      save_type_(SAVE_PAGE_TYPE_UNKNOWN),
      all_save_items_count_(0),
      file_name_set_(&base::FilePath::CompareLessIgnoreCase),
      wait_state_(INITIALIZE),
      unique_id_(GetNextSavePackageId()),
      wrote_to_completed_file_(false),
      wrote_to_failed_file_(false) {}

SavePackage::~SavePackage() {
  // Stop receiving saving job's updates
  if (!finished_ && !canceled()) {
    // Unexpected quit.
    Cancel(true);
  }

  // We should no longer be observing the DownloadItem at this point.
  CHECK(!download_);

  DCHECK_EQ(all_save_items_count_, waiting_item_queue_.size() +
                                       completed_count() + in_process_count());

  // Free all SaveItems.
  STLDeleteElements(&waiting_item_queue_);
  STLDeleteValues(&in_progress_items_);
  STLDeleteValues(&saved_success_items_);
  STLDeleteValues(&saved_failed_items_);
  // Clear containers that contain (now dangling/invalid) pointers to the
  // save items freed above.  This is not strictly required (as the containers
  // will be destructed soon by ~SavePackage), but seems like good code hygiene.
  frame_tree_node_id_to_contained_save_items_.clear();
  frame_tree_node_id_to_save_item_.clear();
  url_to_save_item_.clear();

  file_manager_ = NULL;
}

GURL SavePackage::GetUrlToBeSaved() {
  // Instead of using web_contents_.GetURL here, we use url() (which is the
  // "real" url of the page) from the NavigationEntry because it reflects its
  // origin rather than the displayed one (returned by GetURL) which may be
  // different (like having "view-source:" on the front).
  NavigationEntry* visible_entry =
      web_contents()->GetController().GetVisibleEntry();
  return visible_entry ? visible_entry->GetURL() : GURL::EmptyGURL();
}

void SavePackage::Cancel(bool user_action) {
  if (!canceled()) {
    if (user_action)
      user_canceled_ = true;
    else
      disk_error_occurred_ = true;
    Stop();
  }
  RecordSavePackageEvent(SAVE_PACKAGE_CANCELLED);
}

// Init() can be called directly, or indirectly via GetSaveInfo(). In both
// cases, we need file_manager_ to be initialized, so we do this first.
void SavePackage::InternalInit() {
  ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
  if (!rdh) {
    NOTREACHED();
    return;
  }

  file_manager_ = rdh->save_file_manager();
  DCHECK(file_manager_);

  download_manager_ = static_cast<DownloadManagerImpl*>(
      BrowserContext::GetDownloadManager(
          web_contents()->GetBrowserContext()));
  DCHECK(download_manager_);

  RecordSavePackageEvent(SAVE_PACKAGE_STARTED);
}

bool SavePackage::Init(
    const SavePackageDownloadCreatedCallback& download_created_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Set proper running state.
  if (wait_state_ != INITIALIZE)
    return false;

  wait_state_ = START_PROCESS;

  // Initialize the request context and resource dispatcher.
  BrowserContext* browser_context = web_contents()->GetBrowserContext();
  if (!browser_context) {
    NOTREACHED();
    return false;
  }

  scoped_ptr<DownloadRequestHandleInterface> request_handle(
      new SavePackageRequestHandle(AsWeakPtr()));
  // The download manager keeps ownership but adds us as an observer.
  download_manager_->CreateSavePackageDownloadItem(
      saved_main_file_path_, page_url_,
      ((save_type_ == SAVE_PAGE_TYPE_AS_MHTML) ? "multipart/related"
                                               : "text/html"),
      std::move(request_handle),
      base::Bind(&SavePackage::InitWithDownloadItem, AsWeakPtr(),
                 download_created_callback));
  return true;
}

void SavePackage::InitWithDownloadItem(
    const SavePackageDownloadCreatedCallback& download_created_callback,
    DownloadItemImpl* item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(item);
  download_ = item;
  download_->AddObserver(this);
  // Confirm above didn't delete the tab out from under us.
  if (!download_created_callback.is_null())
    download_created_callback.Run(download_);

  // Check save type and process the save page job.
  if (save_type_ == SAVE_PAGE_TYPE_AS_COMPLETE_HTML) {
    // Get directory
    DCHECK(!saved_main_directory_path_.empty());
    GetSavableResourceLinks();
  } else if (save_type_ == SAVE_PAGE_TYPE_AS_MHTML) {
    web_contents()->GenerateMHTML(saved_main_file_path_, base::Bind(
        &SavePackage::OnMHTMLGenerated, this));
  } else {
    DCHECK_EQ(SAVE_PAGE_TYPE_AS_ONLY_HTML, save_type_);
    wait_state_ = NET_FILES;
    SaveFileCreateInfo::SaveFileSource save_source = page_url_.SchemeIsFile() ?
        SaveFileCreateInfo::SAVE_FILE_FROM_FILE :
        SaveFileCreateInfo::SAVE_FILE_FROM_NET;
    SaveItem* save_item = new SaveItem(page_url_, Referrer(), this, save_source,
                                       FrameTreeNode::kFrameTreeNodeInvalidId);
    // Add this item to waiting list.
    waiting_item_queue_.push_back(save_item);
    all_save_items_count_ = 1;
    download_->SetTotalBytes(1);

    DoSavingProcess();
  }
}

void SavePackage::OnMHTMLGenerated(int64_t size) {
  if (size <= 0) {
    Cancel(false);
    return;
  }
  wrote_to_completed_file_ = true;

  // Hack to avoid touching download_ after user cancel.
  // TODO(rdsmith/benjhayden): Integrate canceling on DownloadItem
  // with SavePackage flow.
  if (download_->GetState() == DownloadItem::IN_PROGRESS) {
    download_->SetTotalBytes(size);
    download_->DestinationUpdate(size, 0, std::string());
    // Must call OnAllDataSaved here in order for
    // GDataDownloadObserver::ShouldUpload() to return true.
    // ShouldCompleteDownload() may depend on the gdata uploader to finish.
    download_->OnAllDataSaved(DownloadItem::kEmptyFileHash);
  }

  if (!download_manager_->GetDelegate()) {
    Finish();
    return;
  }

  if (download_manager_->GetDelegate()->ShouldCompleteDownload(
          download_, base::Bind(&SavePackage::Finish, this))) {
    Finish();
  }
}

// On POSIX, the length of |pure_file_name| + |file_name_ext| is further
// restricted by NAME_MAX. The maximum allowed path looks like:
// '/path/to/save_dir' + '/' + NAME_MAX.
uint32_t SavePackage::GetMaxPathLengthForDirectory(
    const base::FilePath& base_dir) {
#if defined(OS_POSIX)
  return std::min(
      kMaxFilePathLength,
      static_cast<uint32_t>(base_dir.value().length()) + NAME_MAX + 1);
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
bool SavePackage::GetSafePureFileName(
    const base::FilePath& dir_path,
    const base::FilePath::StringType& file_name_ext,
    uint32_t max_file_path_len,
    base::FilePath::StringType* pure_file_name) {
  DCHECK(!pure_file_name->empty());
  int available_length = static_cast<int>(max_file_path_len -
                                          dir_path.value().length() -
                                          file_name_ext.length());
  // Need an extra space for the separator.
  if (!dir_path.EndsWithSeparator())
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
                                   base::FilePath::StringType* generated_name) {
  // TODO(jungshik): Figure out the referrer charset when having one
  // makes sense and pass it to GenerateFileName.
  base::FilePath file_path = net::GenerateFileName(url,
                                                   disposition,
                                                   std::string(),
                                                   std::string(),
                                                   std::string(),
                                                   kDefaultSaveName);

  DCHECK(!file_path.empty());
  base::FilePath::StringType pure_file_name =
      file_path.RemoveExtension().BaseName().value();
  base::FilePath::StringType file_name_ext = file_path.Extension();

  // If it is HTML resource, use ".html" as its extension.
  if (need_html_ext) {
    file_name_ext = FILE_PATH_LITERAL(".");
    file_name_ext.append(kDefaultHtmlExtension);
  }

  // Need to make sure the suggested file name is not too long.
  uint32_t max_path = GetMaxPathLengthForDirectory(saved_main_directory_path_);

  // Get safe pure file name.
  if (!GetSafePureFileName(saved_main_directory_path_, file_name_ext,
                           max_path, &pure_file_name))
    return false;

  base::FilePath::StringType file_name = pure_file_name + file_name_ext;

  // Check whether we already have same name in a case insensitive manner.
  FileNameSet::const_iterator iter = file_name_set_.find(file_name);
  if (iter == file_name_set_.end()) {
    file_name_set_.insert(file_name);
  } else {
    // Found same name, increase the ordinal number for the file name.
    pure_file_name =
        base::FilePath(*iter).RemoveExtension().BaseName().value();
    base::FilePath::StringType base_file_name =
        StripOrdinalNumber(pure_file_name);

    // We need to make sure the length of base file name plus maximum ordinal
    // number path will be less than or equal to kMaxFilePathLength.
    if (!GetSafePureFileName(saved_main_directory_path_, file_name_ext,
        max_path - kMaxFileOrdinalNumberPartLength, &base_file_name))
      return false;

    // Prepare the new ordinal number.
    uint32_t ordinal_number;
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
      base::FilePath temp_file;
      base::CreateTemporaryFile(&temp_file);
      file_name = temp_file.RemoveExtension().BaseName().value();
      // Get safe pure file name.
      if (!GetSafePureFileName(saved_main_directory_path_,
                               base::FilePath::StringType(),
                               max_path, &file_name))
        return false;
    } else {
      for (int i = ordinal_number; i < kMaxFileOrdinalNumber; ++i) {
        base::FilePath::StringType new_name = base_file_name +
            base::StringPrintf(FILE_PATH_LITERAL("(%d)"), i) + file_name_ext;
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
// find a SaveItem and store it in our in_progress list.
void SavePackage::StartSave(const SaveFileCreateInfo* info) {
  DCHECK(info);

  SaveItemIdMap::iterator it = in_progress_items_.find(info->save_item_id);
  if (it == in_progress_items_.end()) {
    // If not found, we must have cancel action.
    DCHECK(canceled());
    return;
  }
  SaveItem* save_item = it->second;

  DCHECK(!saved_main_file_path_.empty());

  save_item->SetTotalBytes(info->total_bytes);

  // Determine the proper path for a saving job, by choosing either the default
  // save directory, or prompting the user.
  DCHECK(!save_item->has_final_name());
  if (info->url != page_url_) {
    base::FilePath::StringType generated_name;
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
        SaveFinished(save_item->id(), 0, false);
      return;
    }

    // When saving page as only-HTML, we only have a SaveItem whose url
    // must be page_url_.
    DCHECK_EQ(SAVE_PAGE_TYPE_AS_COMPLETE_HTML, save_type_);
    DCHECK(!saved_main_directory_path_.empty());

    // Now we get final name retrieved from GenerateFileName, we will use it
    // rename the SaveItem.
    base::FilePath final_name =
        saved_main_directory_path_.Append(generated_name);
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
        base::Bind(&SaveFileManager::SaveLocalFile, file_manager_,
                   save_item->url(), save_item->id(), id()));
    return;
  }

  // Check whether we begin to require serialized HTML data.
  if (save_type_ == SAVE_PAGE_TYPE_AS_COMPLETE_HTML &&
      wait_state_ == HTML_DATA) {
    // Inform backend to serialize the all frames' DOM and send serialized
    // HTML data back.
    GetSerializedHtmlWithLocalLinks();
  }
}

SaveItem* SavePackage::LookupSaveItemInProcess(SaveItemId save_item_id) {
  auto it = in_progress_items_.find(save_item_id);
  if (it != in_progress_items_.end()) {
    SaveItem* save_item = it->second;
    DCHECK_EQ(SaveItem::IN_PROGRESS, save_item->state());
    return save_item;
  }
  return nullptr;
}

void SavePackage::PutInProgressItemToSavedMap(SaveItem* save_item) {
  SaveItemIdMap::iterator it = in_progress_items_.find(save_item->id());
  DCHECK(it != in_progress_items_.end());
  DCHECK(save_item == it->second);
  in_progress_items_.erase(it);

  if (save_item->success()) {
    // Add it to saved_success_items_.
    DCHECK(saved_success_items_.find(save_item->id()) ==
           saved_success_items_.end());
    saved_success_items_[save_item->id()] = save_item;
  } else {
    // Add it to saved_failed_items_.
    DCHECK(saved_failed_items_.find(save_item->id()) ==
           saved_failed_items_.end());
    saved_failed_items_[save_item->id()] = save_item;
  }
}

// Called for updating saving state.
bool SavePackage::UpdateSaveProgress(SaveItemId save_item_id,
                                     int64_t size,
                                     bool write_success) {
  // Because we might have canceled this saving job before,
  // so we might not find corresponding SaveItem.
  SaveItem* save_item = LookupSaveItemInProcess(save_item_id);
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
    for (const auto& it : in_progress_items_) {
      SaveItem* save_item = it.second;
      DCHECK_EQ(SaveItem::IN_PROGRESS, save_item->state());
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
  std::vector<SaveItemId> save_item_ids;
  for (const auto& it : saved_success_items_)
    save_item_ids.push_back(it.first);
  for (const auto& it : saved_failed_items_)
    save_item_ids.push_back(it.first);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::RemoveSavedFileFromFileMap, file_manager_,
                 save_item_ids));

  finished_ = true;
  wait_state_ = FAILED;

  // Inform the DownloadItem we have canceled whole save page job.
  if (download_) {
    download_->Cancel(false);
    FinalizeDownloadEntry();
  }
}

void SavePackage::CheckFinish() {
  if (in_process_count() || finished_)
    return;

  base::FilePath dir = (save_type_ == SAVE_PAGE_TYPE_AS_COMPLETE_HTML &&
                        saved_success_items_.size() > 1) ?
                        saved_main_directory_path_ : base::FilePath();

  FinalNamesMap final_names;
  for (const auto& it : saved_success_items_)
    final_names.insert(std::make_pair(it.first, it.second->full_path()));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::RenameAllFiles,
                 file_manager_,
                 final_names,
                 dir,
                 web_contents()->GetRenderProcessHost()->GetID(),
                 web_contents()->GetMainFrame()->GetRoutingID(),
                 id()));
}

// Successfully finished all items of this SavePackage.
void SavePackage::Finish() {
  // User may cancel the job when we're moving files to the final directory.
  if (canceled())
    return;

  wait_state_ = SUCCESSFUL;
  finished_ = true;

  // Record finish.
  RecordSavePackageEvent(SAVE_PACKAGE_FINISHED);

  // Record any errors that occurred.
  if (wrote_to_completed_file_)
    RecordSavePackageEvent(SAVE_PACKAGE_WRITE_TO_COMPLETED);

  if (wrote_to_failed_file_)
    RecordSavePackageEvent(SAVE_PACKAGE_WRITE_TO_FAILED);

  // This vector contains the save ids of the save files which SaveFileManager
  // needs to remove from its save_file_map_.
  std::vector<SaveItemId> list_of_failed_save_item_ids;
  for (const auto& it : saved_failed_items_) {
    SaveItem* save_item = it.second;
    DCHECK_EQ(it.first, save_item->id());
    list_of_failed_save_item_ids.push_back(save_item->id());
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::RemoveSavedFileFromFileMap, file_manager_,
                 list_of_failed_save_item_ids));

  if (download_) {
    // Hack to avoid touching download_ after user cancel.
    // TODO(rdsmith/benjhayden): Integrate canceling on DownloadItem
    // with SavePackage flow.
    if (download_->GetState() == DownloadItem::IN_PROGRESS) {
      if (save_type_ != SAVE_PAGE_TYPE_AS_MHTML) {
        download_->DestinationUpdate(
            all_save_items_count_, CurrentSpeed(), std::string());
        download_->OnAllDataSaved(DownloadItem::kEmptyFileHash);
      }
      download_->MarkAsComplete();
    }
    FinalizeDownloadEntry();
  }
}

// Called for updating end state.
void SavePackage::SaveFinished(SaveItemId save_item_id,
                               int64_t size,
                               bool is_success) {
  // Because we might have canceled this saving job before,
  // so we might not find corresponding SaveItem. Just ignore it.
  SaveItem* save_item = LookupSaveItemInProcess(save_item_id);
  if (!save_item)
    return;

  // Let SaveItem set end state.
  save_item->Finish(size, is_success);
  // Remove the associated save id and SavePackage.
  file_manager_->RemoveSaveFile(save_item->id(), this);

  PutInProgressItemToSavedMap(save_item);

  // Inform the DownloadItem to update UI.
  // We use the received bytes as number of saved files.
  // Hack to avoid touching download_ after user cancel.
  // TODO(rdsmith/benjhayden): Integrate canceling on DownloadItem
  // with SavePackage flow.
  if (download_ && (download_->GetState() == DownloadItem::IN_PROGRESS)) {
    download_->DestinationUpdate(
        completed_count(), CurrentSpeed(), std::string());
  }

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

void SavePackage::SaveCanceled(SaveItem* save_item) {
  // Call the RemoveSaveFile in UI thread.
  file_manager_->RemoveSaveFile(save_item->id(), this);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::CancelSave, file_manager_, save_item->id()));
}

// Initiate a saving job of a specific URL. We send the request to
// SaveFileManager, which will dispatch it to different approach according to
// the save source. Parameter process_all_remaining_items indicates whether
// we need to save all remaining items.
void SavePackage::SaveNextFile(bool process_all_remaining_items) {
  DCHECK(web_contents());
  DCHECK(waiting_item_queue_.size());

  do {
    // Pop SaveItem from waiting list.
    SaveItem* save_item = waiting_item_queue_.front();
    waiting_item_queue_.pop_front();

    // Add the item to in_progress_items_.
    SaveItemIdMap::iterator it = in_progress_items_.find(save_item->id());
    DCHECK(it == in_progress_items_.end());
    in_progress_items_[save_item->id()] = save_item;
    save_item->Start();
    file_manager_->SaveURL(
        save_item->id(), save_item->url(), save_item->referrer(),
        web_contents()->GetRenderProcessHost()->GetID(), routing_id(),
        web_contents()->GetMainFrame()->GetRoutingID(),
        save_item->save_source(), save_item->full_path(),
        web_contents()->GetBrowserContext()->GetResourceContext(), this);
  } while (process_all_remaining_items && waiting_item_queue_.size());
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

int64_t SavePackage::CurrentSpeed() const {
  base::TimeDelta diff = base::TimeTicks::Now() - start_tick_;
  int64_t diff_ms = diff.InMilliseconds();
  return diff_ms == 0 ? 0 : completed_count() * 1000 / diff_ms;
}

// Continue processing the save page job after one SaveItem has been
// finished.
void SavePackage::DoSavingProcess() {
  if (save_type_ == SAVE_PAGE_TYPE_AS_COMPLETE_HTML) {
    // We guarantee that images and JavaScripts must be downloaded first.
    // So when finishing all those sub-resources, we will know which
    // sub-resource's link can be replaced with local file path, which
    // sub-resource's link need to be replaced with absolute URL which
    // point to its internet address because it got error when saving its data.

    // Start a new SaveItem job if we still have job in waiting queue.
    if (waiting_item_queue_.size()) {
      DCHECK_EQ(NET_FILES, wait_state_);
      SaveItem* save_item = waiting_item_queue_.front();
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
      DCHECK_EQ(HTML_DATA, wait_state_);
    }
  } else {
    // Save as HTML only or MHTML.
    DCHECK_EQ(NET_FILES, wait_state_);
    DCHECK((save_type_ == SAVE_PAGE_TYPE_AS_ONLY_HTML) ||
           (save_type_ == SAVE_PAGE_TYPE_AS_MHTML))
        << save_type_;
    if (waiting_item_queue_.size()) {
      DCHECK_EQ(all_save_items_count_, waiting_item_queue_.size());
      SaveNextFile(false);
    }
  }
}

bool SavePackage::OnMessageReceived(const IPC::Message& message,
                                    RenderFrameHost* render_frame_host) {
  bool handled = true;
  auto* rfhi = static_cast<RenderFrameHostImpl*>(render_frame_host);
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(SavePackage, message, rfhi)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SavableResourceLinksResponse,
                        OnSavableResourceLinksResponse)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SavableResourceLinksError,
                        OnSavableResourceLinksError)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SerializedHtmlWithLocalLinksResponse,
                        OnSerializedHtmlWithLocalLinksResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// After finishing all SaveItems which need to get data from net.
// We collect all URLs which have local storage and send the
// map:(originalURL:currentLocalPath) to render process (backend).
// Then render process will serialize DOM and send data to us.
void SavePackage::GetSerializedHtmlWithLocalLinks() {
  if (wait_state_ != HTML_DATA)
    return;

  // First collect the status of all the resource files and check whether they
  // have created local files (although they have not been completely saved).
  int successful_started_items_count = 0;
  for (const auto& item : in_progress_items_) {
    DCHECK_EQ(SaveFileCreateInfo::SAVE_FILE_FROM_DOM,
              item.second->save_source());
    if (item.second->has_final_name())
      successful_started_items_count++;
  }
  // If not all file of HTML resource have been started, then wait.
  if (successful_started_items_count != in_process_count())
    return;

  // Ask all frames for their serialized data.
  DCHECK_EQ(0, number_of_frames_pending_response_);
  FrameTree* frame_tree =
      static_cast<RenderFrameHostImpl*>(web_contents()->GetMainFrame())
          ->frame_tree_node()->frame_tree();
  for (const auto& item : frame_tree_node_id_to_save_item_) {
    DCHECK(item.second);  // SaveItem* != nullptr.
    int frame_tree_node_id = item.first;
    FrameTreeNode* frame_tree_node = frame_tree->FindByID(frame_tree_node_id);
    if (frame_tree_node) {
      GetSerializedHtmlWithLocalLinksForFrame(frame_tree_node);
      number_of_frames_pending_response_++;
    }
  }
  if (number_of_frames_pending_response_ == 0) {
    // All frames disappeared since gathering of savable resources?
    // Treat this as cancellation.
    Cancel(false);
  }
}

void SavePackage::GetSerializedHtmlWithLocalLinksForFrame(
    FrameTreeNode* target_tree_node) {
  DCHECK(target_tree_node);
  int target_frame_tree_node_id = target_tree_node->frame_tree_node_id();
  RenderFrameHostImpl* target = target_tree_node->current_frame_host();

  // Collect all saved success items.
  // SECURITY NOTE: We don't send *all* urls / local paths, but only
  // those that the given frame had access to already (because it contained
  // the savable resources / subframes associated with save items).
  std::map<GURL, base::FilePath> url_to_local_path;
  std::map<int, base::FilePath> routing_id_to_local_path;
  auto it = frame_tree_node_id_to_contained_save_items_.find(
      target_frame_tree_node_id);
  if (it != frame_tree_node_id_to_contained_save_items_.end()) {
    for (SaveItem* save_item : it->second) {
      // Calculate the local link to use for this |save_item|.
      DCHECK(save_item->has_final_name());
      base::FilePath local_path(base::FilePath::kCurrentDirectory);
      if (target_tree_node->IsMainFrame()) {
        local_path = local_path.Append(saved_main_directory_path_.BaseName());
      }
      local_path = local_path.Append(save_item->file_name());

      // Insert the link into |url_to_local_path| or |routing_id_to_local_path|.
      if (save_item->save_source() != SaveFileCreateInfo::SAVE_FILE_FROM_DOM) {
        DCHECK_EQ(FrameTreeNode::kFrameTreeNodeInvalidId,
                  save_item->frame_tree_node_id());
        url_to_local_path[save_item->url()] = local_path;
      } else {
        FrameTreeNode* save_item_frame_tree_node =
            target_tree_node->frame_tree()->FindByID(
                save_item->frame_tree_node_id());
        if (!save_item_frame_tree_node) {
          // crbug.com/541354: Raciness when saving a dynamically changing page.
          continue;
        }

        int routing_id =
            save_item_frame_tree_node->render_manager()
                ->GetRoutingIdForSiteInstance(target->GetSiteInstance());
        DCHECK_NE(MSG_ROUTING_NONE, routing_id);

        routing_id_to_local_path[routing_id] = local_path;
      }
    }
  }

  // Ask target frame to serialize itself.
  target->Send(new FrameMsg_GetSerializedHtmlWithLocalLinks(
      target->GetRoutingID(), url_to_local_path, routing_id_to_local_path));
}

// Process the serialized HTML content data of a specified frame
// retrieved from the renderer process.
void SavePackage::OnSerializedHtmlWithLocalLinksResponse(
    RenderFrameHostImpl* sender,
    const std::string& data,
    bool end_of_data) {
  // Check current state.
  if (wait_state_ != HTML_DATA)
    return;

  int frame_tree_node_id = sender->frame_tree_node()->frame_tree_node_id();
  auto it = frame_tree_node_id_to_save_item_.find(frame_tree_node_id);
  if (it == frame_tree_node_id_to_save_item_.end()) {
    // This is parimarily sanitization of IPC (renderer shouldn't send
    // OnSerializedHtmlFragment IPC without being asked to), but it might also
    // occur in the wild (if old renderer response reaches a new SavePackage).
    return;
  }
  SaveItem* save_item = it->second;
  DCHECK_EQ(SaveFileCreateInfo::SAVE_FILE_FROM_DOM, save_item->save_source());
  if (save_item->state() != SaveItem::IN_PROGRESS) {
    for (const auto& saved_it : saved_success_items_) {
      if (saved_it.second->url() == save_item->url()) {
        wrote_to_completed_file_ = true;
        break;
      }
    }

    auto it2 = saved_failed_items_.find(save_item->id());
    if (it2 != saved_failed_items_.end())
      wrote_to_failed_file_ = true;

    return;
  }

  if (!data.empty()) {
    // Prepare buffer for saving HTML data.
    scoped_refptr<net::IOBuffer> new_data(new net::IOBuffer(data.size()));
    memcpy(new_data->data(), data.data(), data.size());

    // Call write file functionality in file thread.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&SaveFileManager::UpdateSaveProgress, file_manager_,
                   save_item->id(), new_data, static_cast<int>(data.size())));
  }

  // Current frame is completed saving, call finish in file thread.
  if (end_of_data) {
    DVLOG(20) << " " << __FUNCTION__ << "()"
              << " save_item_id = " << save_item->id() << " url = \""
              << save_item->url().spec() << "\"";
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&SaveFileManager::SaveFinished, file_manager_,
                   save_item->id(), id(), true));
    number_of_frames_pending_response_--;
    DCHECK_LE(0, number_of_frames_pending_response_);
  }
}

// Ask for all savable resource links from backend, include main frame and
// sub-frame.
void SavePackage::GetSavableResourceLinks() {
  if (wait_state_ != START_PROCESS)
    return;

  wait_state_ = RESOURCES_LIST;

  DCHECK_EQ(0, number_of_frames_pending_response_);
  number_of_frames_pending_response_ = web_contents()->SendToAllFrames(
      new FrameMsg_GetSavableResourceLinks(MSG_ROUTING_NONE));
  DCHECK_LT(0, number_of_frames_pending_response_);

  // Enqueue the main frame separately (because this frame won't show up in any
  // of OnSavableResourceLinksResponse callbacks).
  FrameTreeNode* main_frame_tree_node =
      static_cast<RenderFrameHostImpl*>(web_contents()->GetMainFrame())
          ->frame_tree_node();
  EnqueueFrame(FrameTreeNode::kFrameTreeNodeInvalidId,  // No container.
               main_frame_tree_node->frame_tree_node_id(),
               main_frame_tree_node->current_url());
}

void SavePackage::OnSavableResourceLinksResponse(
    RenderFrameHostImpl* sender,
    const std::vector<GURL>& resources_list,
    const Referrer& referrer,
    const std::vector<SavableSubframe>& subframes) {
  if (wait_state_ != RESOURCES_LIST)
    return;

  // Add all sub-resources to wait list.
  int container_frame_tree_node_id =
      sender->frame_tree_node()->frame_tree_node_id();
  for (const GURL& u : resources_list) {
    EnqueueSavableResource(container_frame_tree_node_id, u, referrer);
  }
  for (const SavableSubframe& subframe : subframes) {
    FrameTreeNode* subframe_tree_node =
        sender->frame_tree_node()->frame_tree()->FindByRoutingID(
            sender->GetProcess()->GetID(), subframe.routing_id);

    if (!subframe_tree_node) {
      // crbug.com/541354 - Raciness when saving a dynamically changing page.
      continue;
    }
    if (subframe_tree_node->parent() != sender->frame_tree_node()) {
      // Only reachable if the renderer has a bug or has been compromised.
      ReceivedBadMessage(
          sender->GetProcess(),
          bad_message::DWNLD_INVALID_SAVABLE_RESOURCE_LINKS_RESPONSE);
      continue;
    }

    EnqueueFrame(container_frame_tree_node_id,
                 subframe_tree_node->frame_tree_node_id(),
                 subframe.original_url);
  }

  CompleteSavableResourceLinksResponse();
}

SaveItem* SavePackage::CreatePendingSaveItem(
    int container_frame_tree_node_id,
    int save_item_frame_tree_node_id,
    const GURL& url,
    const Referrer& referrer,
    SaveFileCreateInfo::SaveFileSource save_source) {
  SaveItem* save_item;
  Referrer sanitized_referrer = Referrer::SanitizeForRequest(url, referrer);
  save_item = new SaveItem(url, sanitized_referrer, this, save_source,
                           save_item_frame_tree_node_id);
  waiting_item_queue_.push_back(save_item);

  frame_tree_node_id_to_contained_save_items_[container_frame_tree_node_id]
      .push_back(save_item);
  return save_item;
}

SaveItem* SavePackage::CreatePendingSaveItemDeduplicatingByUrl(
    int container_frame_tree_node_id,
    int save_item_frame_tree_node_id,
    const GURL& url,
    const Referrer& referrer,
    SaveFileCreateInfo::SaveFileSource save_source) {
  DCHECK(url.is_valid());  // |url| should be validated by the callers.

  // Frames should not be deduplicated by URL.
  DCHECK_NE(SaveFileCreateInfo::SAVE_FILE_FROM_DOM, save_source);

  SaveItem* save_item;
  auto it = url_to_save_item_.find(url);
  if (it != url_to_save_item_.end()) {
    save_item = it->second;
    frame_tree_node_id_to_contained_save_items_[container_frame_tree_node_id]
        .push_back(save_item);
  } else {
    save_item = CreatePendingSaveItem(container_frame_tree_node_id,
                                      save_item_frame_tree_node_id, url,
                                      referrer, save_source);
    url_to_save_item_[url] = save_item;
  }

  return save_item;
}

void SavePackage::EnqueueSavableResource(int container_frame_tree_node_id,
                                         const GURL& url,
                                         const Referrer& referrer) {
  if (!url.is_valid())
    return;

  SaveFileCreateInfo::SaveFileSource save_source =
      url.SchemeIsFile() ? SaveFileCreateInfo::SAVE_FILE_FROM_FILE
                         : SaveFileCreateInfo::SAVE_FILE_FROM_NET;
  CreatePendingSaveItemDeduplicatingByUrl(
      container_frame_tree_node_id, FrameTreeNode::kFrameTreeNodeInvalidId, url,
      referrer, save_source);
}

void SavePackage::EnqueueFrame(int container_frame_tree_node_id,
                               int frame_tree_node_id,
                               const GURL& frame_original_url) {
  SaveItem* save_item = CreatePendingSaveItem(
      container_frame_tree_node_id, frame_tree_node_id, frame_original_url,
      Referrer(), SaveFileCreateInfo::SAVE_FILE_FROM_DOM);
  DCHECK(save_item);
  frame_tree_node_id_to_save_item_[frame_tree_node_id] = save_item;
}

void SavePackage::OnSavableResourceLinksError(RenderFrameHostImpl* sender) {
  CompleteSavableResourceLinksResponse();
}

void SavePackage::CompleteSavableResourceLinksResponse() {
  --number_of_frames_pending_response_;
  DCHECK_LE(0, number_of_frames_pending_response_);
  if (number_of_frames_pending_response_ != 0)
    return;  // Need to wait for more responses from RenderFrames.

  // Sort |waiting_item_queue_| so that frames go last (frames are identified by
  // SAVE_FILE_FROM_DOM in the comparison function below).
  std::stable_sort(
      waiting_item_queue_.begin(), waiting_item_queue_.end(),
      [](SaveItem* x, SaveItem* y) {
        DCHECK(x);
        DCHECK(y);
        return (x->save_source() != SaveFileCreateInfo::SAVE_FILE_FROM_DOM) &&
               (y->save_source() == SaveFileCreateInfo::SAVE_FILE_FROM_DOM);
      });

  all_save_items_count_ = static_cast<int>(waiting_item_queue_.size());

  // We use total bytes as the total number of files we want to save.
  // Hack to avoid touching download_ after user cancel.
  // TODO(rdsmith/benjhayden): Integrate canceling on DownloadItem
  // with SavePackage flow.
  if (download_ && (download_->GetState() == DownloadItem::IN_PROGRESS))
    download_->SetTotalBytes(all_save_items_count_);

  if (all_save_items_count_) {
    wait_state_ = NET_FILES;

    // Give backend the lists which contain all resource links that have local
    // storage, after which, render process will serialize DOM for generating
    // HTML data.
    DoSavingProcess();
  } else {
    // No savable frames and/or resources - treat it as user cancel.
    Cancel(true);
  }
}

base::FilePath SavePackage::GetSuggestedNameForSaveAs(
    bool can_save_as_complete,
    const std::string& contents_mime_type,
    const std::string& accept_langs) {
  base::FilePath name_with_proper_ext = base::FilePath::FromUTF16Unsafe(title_);

  // If the page's title matches its URL, use the URL. Try to use the last path
  // component or if there is none, the domain as the file name.
  // Normally we want to base the filename on the page title, or if it doesn't
  // exist, on the URL. It's not easy to tell if the page has no title, because
  // if the page has no title, WebContents::GetTitle() will return the page's
  // URL (adjusted for display purposes). Therefore, we convert the "title"
  // back to a URL, and if it matches the original page URL, we know the page
  // had no title (or had a title equal to its URL, which is fine to treat
  // similarly).
  if (title_ == url_formatter::FormatUrl(page_url_, accept_langs)) {
    std::string url_path;
    if (!page_url_.SchemeIs(url::kDataScheme)) {
      std::vector<std::string> url_parts = base::SplitString(
          page_url_.path(), "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      if (!url_parts.empty()) {
        for (int i = static_cast<int>(url_parts.size()) - 1; i >= 0; --i) {
          url_path = url_parts[i];
          if (!url_path.empty())
            break;
        }
      }
      if (url_path.empty())
        url_path = page_url_.host();
    } else {
      url_path = "dataurl";
    }
    name_with_proper_ext = base::FilePath::FromUTF8Unsafe(url_path);
  }

  // Ask user for getting final saving name.
  name_with_proper_ext = EnsureMimeExtension(name_with_proper_ext,
                                             contents_mime_type);
  // Adjust extension for complete types.
  if (can_save_as_complete)
    name_with_proper_ext = EnsureHtmlExtension(name_with_proper_ext);

  base::FilePath::StringType file_name = name_with_proper_ext.value();
  base::i18n::ReplaceIllegalCharactersInPath(&file_name, '_');
  return base::FilePath(file_name);
}

base::FilePath SavePackage::EnsureHtmlExtension(const base::FilePath& name) {
  // If the file name doesn't have an extension suitable for HTML files,
  // append one.
  base::FilePath::StringType ext = name.Extension();
  if (!ext.empty())
    ext.erase(ext.begin());  // Erase preceding '.'.
  std::string mime_type;
  if (!net::GetMimeTypeFromExtension(ext, &mime_type) ||
      !CanSaveAsComplete(mime_type)) {
    return base::FilePath(name.value() + FILE_PATH_LITERAL(".") +
                          kDefaultHtmlExtension);
  }
  return name;
}

base::FilePath SavePackage::EnsureMimeExtension(const base::FilePath& name,
    const std::string& contents_mime_type) {
  // Start extension at 1 to skip over period if non-empty.
  base::FilePath::StringType ext = name.Extension().length() ?
      name.Extension().substr(1) : name.Extension();
  base::FilePath::StringType suggested_extension =
      ExtensionForMimeType(contents_mime_type);
  std::string mime_type;
  if (!suggested_extension.empty() &&
      !net::GetMimeTypeFromExtension(ext, &mime_type)) {
    // Extension is absent or needs to be updated.
    return base::FilePath(name.value() + FILE_PATH_LITERAL(".") +
                    suggested_extension);
  }
  return name;
}

const base::FilePath::CharType* SavePackage::ExtensionForMimeType(
    const std::string& contents_mime_type) {
  static const struct {
    const base::FilePath::CharType *mime_type;
    const base::FilePath::CharType *suggested_extension;
  } extensions[] = {
    { FILE_PATH_LITERAL("text/html"), kDefaultHtmlExtension },
    { FILE_PATH_LITERAL("text/xml"), FILE_PATH_LITERAL("xml") },
    { FILE_PATH_LITERAL("application/xhtml+xml"), FILE_PATH_LITERAL("xhtml") },
    { FILE_PATH_LITERAL("text/plain"), FILE_PATH_LITERAL("txt") },
    { FILE_PATH_LITERAL("text/css"), FILE_PATH_LITERAL("css") },
  };
#if defined(OS_POSIX)
  base::FilePath::StringType mime_type(contents_mime_type);
#elif defined(OS_WIN)
  base::FilePath::StringType mime_type(base::UTF8ToWide(contents_mime_type));
#endif  // OS_WIN
  for (uint32_t i = 0; i < arraysize(extensions); ++i) {
    if (mime_type == extensions[i].mime_type)
      return extensions[i].suggested_extension;
  }
  return FILE_PATH_LITERAL("");
}

void SavePackage::GetSaveInfo() {
  // Can't use web_contents_ in the file thread, so get the data that we need
  // before calling to it.
  base::FilePath website_save_dir, download_save_dir;
  bool skip_dir_check = false;
  DCHECK(download_manager_);
  if (download_manager_->GetDelegate()) {
    download_manager_->GetDelegate()->GetSaveDir(
        web_contents()->GetBrowserContext(), &website_save_dir,
        &download_save_dir, &skip_dir_check);
  }
  std::string mime_type = web_contents()->GetContentsMimeType();
  std::string accept_languages =
      GetContentClient()->browser()->GetAcceptLangs(
          web_contents()->GetBrowserContext());

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SavePackage::CreateDirectoryOnFileThread, this,
          website_save_dir, download_save_dir, skip_dir_check,
          mime_type, accept_languages));
}

void SavePackage::CreateDirectoryOnFileThread(
    const base::FilePath& website_save_dir,
    const base::FilePath& download_save_dir,
    bool skip_dir_check,
    const std::string& mime_type,
    const std::string& accept_langs) {
  base::FilePath save_dir;
  // If the default html/websites save folder doesn't exist...
  // We skip the directory check for gdata directories on ChromeOS.
  if (!skip_dir_check && !base::DirectoryExists(website_save_dir)) {
    // If the default download dir doesn't exist, create it.
    if (!base::DirectoryExists(download_save_dir)) {
      bool res = base::CreateDirectory(download_save_dir);
      DCHECK(res);
    }
    save_dir = download_save_dir;
  } else {
    // If it does exist, use the default save dir param.
    save_dir = website_save_dir;
  }

  bool can_save_as_complete = CanSaveAsComplete(mime_type);
  base::FilePath suggested_filename = GetSuggestedNameForSaveAs(
      can_save_as_complete, mime_type, accept_langs);
  base::FilePath::StringType pure_file_name =
      suggested_filename.RemoveExtension().BaseName().value();
  base::FilePath::StringType file_name_ext = suggested_filename.Extension();

  // Need to make sure the suggested file name is not too long.
  uint32_t max_path = GetMaxPathLengthForDirectory(save_dir);

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
      base::Bind(&SavePackage::ContinueGetSaveInfo, this, save_dir,
                 can_save_as_complete));
}

void SavePackage::ContinueGetSaveInfo(const base::FilePath& suggested_path,
                                      bool can_save_as_complete) {

  // The WebContents which owns this SavePackage may have disappeared during
  // the UI->FILE->UI thread hop of
  // GetSaveInfo->CreateDirectoryOnFileThread->ContinueGetSaveInfo.
  if (!web_contents() || !download_manager_->GetDelegate())
    return;

  base::FilePath::StringType default_extension;
  if (can_save_as_complete)
    default_extension = kDefaultHtmlExtension;

  download_manager_->GetDelegate()->ChooseSavePath(
      web_contents(),
      suggested_path,
      default_extension,
      can_save_as_complete,
      base::Bind(&SavePackage::OnPathPicked, AsWeakPtr()));
}

void SavePackage::OnPathPicked(
    const base::FilePath& final_name,
    SavePageType type,
    const SavePackageDownloadCreatedCallback& download_created_callback) {
  DCHECK((type == SAVE_PAGE_TYPE_AS_ONLY_HTML) ||
         (type == SAVE_PAGE_TYPE_AS_MHTML) ||
         (type == SAVE_PAGE_TYPE_AS_COMPLETE_HTML)) << type;
  // Ensure the filename is safe.
  saved_main_file_path_ = final_name;
  // TODO(asanka): This call may block on IO and shouldn't be made
  // from the UI thread.  See http://crbug.com/61827.
  net::GenerateSafeFileName(web_contents()->GetContentsMimeType(), false,
                            &saved_main_file_path_);

  saved_main_directory_path_ = saved_main_file_path_.DirName();
  save_type_ = type;
  if (save_type_ == SAVE_PAGE_TYPE_AS_COMPLETE_HTML) {
    // Make new directory for saving complete file.
    saved_main_directory_path_ = saved_main_directory_path_.Append(
        saved_main_file_path_.RemoveExtension().BaseName().value() +
        FILE_PATH_LITERAL("_files"));
  }

  Init(download_created_callback);
}

void SavePackage::StopObservation() {
  DCHECK(download_);
  DCHECK(download_manager_);

  download_->RemoveObserver(this);
  download_ = NULL;
  download_manager_ = NULL;
}

void SavePackage::OnDownloadDestroyed(DownloadItem* download) {
  StopObservation();
}

void SavePackage::FinalizeDownloadEntry() {
  DCHECK(download_);
  DCHECK(download_manager_);

  download_manager_->OnSavePackageSuccessfullyFinished(download_);
  StopObservation();
}

}  // namespace content
