// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/download/save_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

class GURL;

namespace content {
class DownloadItemImpl;
class DownloadManagerImpl;
class FrameTreeNode;
class RenderFrameHostImpl;
struct SavableSubframe;
class SaveFileManager;
class SaveItem;
class SavePackage;
class WebContents;

// The SavePackage object manages the process of saving a page as only-html or
// complete-html or MHTML and providing the information for displaying saving
// status.  Saving page as only-html means means that we save web page to a
// single HTML file regardless internal sub resources and sub frames.  Saving
// page as complete-html page means we save not only the main html file the user
// told it to save but also a directory for the auxiliary files such as all
// sub-frame html files, image files, css files and js files.  Saving page as
// MHTML means the same thing as complete-html, but it uses the MHTML format to
// contain the html and all auxiliary files in a single text file.
//
// Each page saving job may include one or multiple files which need to be
// saved. Each file is represented by a SaveItem, and all SaveItems are owned
// by the SavePackage. SaveItems are created when a user initiates a page
// saving job, and exist for the duration of one contents's life time.
class CONTENT_EXPORT SavePackage
    : public base::RefCountedThreadSafe<SavePackage>,
      public WebContentsObserver,
      public DownloadItem::Observer,
      public base::SupportsWeakPtr<SavePackage> {
 public:
  enum WaitState {
    // State when created but not initialized.
    INITIALIZE = 0,
    // State when after initializing, but not yet saving.
    START_PROCESS,
    // Waiting on a list of savable resources from the backend.
    RESOURCES_LIST,
    // Waiting for data sent from net IO or from file system.
    NET_FILES,
    // Waiting for html DOM data sent from render process.
    HTML_DATA,
    // Saving page finished successfully.
    SUCCESSFUL,
    // Failed to save page.
    FAILED
  };

  static const base::FilePath::CharType kDefaultHtmlExtension[];

  // Constructor for user initiated page saving. This constructor results in a
  // SavePackage that will generate and sanitize a suggested name for the user
  // in the "Save As" dialog box.
  explicit SavePackage(WebContents* web_contents);

  // This contructor is used only for testing. We can bypass the file and
  // directory name generation / sanitization by providing well known paths
  // better suited for tests.
  SavePackage(WebContents* web_contents,
              SavePageType save_type,
              const base::FilePath& file_full_path,
              const base::FilePath& directory_full_path);

  // Initialize the SavePackage. Returns true if it initializes properly.  Need
  // to make sure that this method must be called in the UI thread because using
  // g_browser_process on a non-UI thread can cause crashes during shutdown.
  // |cb| will be called when the DownloadItem is created, before data is
  // written to disk.
  bool Init(const SavePackageDownloadCreatedCallback& cb);

  // Cancel all in progress request, might be called by user or internal error.
  void Cancel(bool user_action);

  void Finish();

  // Notifications sent from the file thread to the UI thread.
  void StartSave(const SaveFileCreateInfo* info);
  bool UpdateSaveProgress(SaveItemId save_item_id,
                          int64_t size,
                          bool write_success);
  void SaveFinished(SaveItemId save_item_id, int64_t size, bool is_success);
  void SaveCanceled(SaveItem* save_item);

  // Rough percent complete, -1 means we don't know (since we didn't receive a
  // total size).
  int PercentComplete();

  bool canceled() const { return user_canceled_ || disk_error_occurred_; }
  bool finished() const { return finished_; }
  SavePageType save_type() const { return save_type_; }

  SavePackageId id() const { return unique_id_; }

  void GetSaveInfo();

 private:
  friend class base::RefCountedThreadSafe<SavePackage>;

  void InitWithDownloadItem(
      const SavePackageDownloadCreatedCallback& download_created_callback,
      DownloadItemImpl* item);

  // Callback for WebContents::GenerateMHTML().
  void OnMHTMLGenerated(int64_t size);

  // For testing only.
  SavePackage(WebContents* web_contents,
              const base::FilePath& file_full_path,
              const base::FilePath& directory_full_path);

  ~SavePackage() override;

  // Notes from Init() above applies here as well.
  void InternalInit();

  void Stop();
  void CheckFinish();
  void SaveNextFile(bool process_all_remainder_items);
  void DoSavingProcess();

  // WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;

  // DownloadItem::Observer implementation.
  void OnDownloadDestroyed(DownloadItem* download) override;

  // Update the download history of this item upon completion.
  void FinalizeDownloadEntry();

  // Detach from DownloadManager.
  void StopObservation();

  // Return max length of a path for a specific base directory.
  // This is needed on POSIX, which restrict the length of file names in
  // addition to the restriction on the length of path names.
  // |base_dir| is assumed to be a directory name with no trailing slash.
  static uint32_t GetMaxPathLengthForDirectory(const base::FilePath& base_dir);

  static bool GetSafePureFileName(
      const base::FilePath& dir_path,
      const base::FilePath::StringType& file_name_ext,
      uint32_t max_file_path_len,
      base::FilePath::StringType* pure_file_name);

  // Create a file name based on the response from the server.
  bool GenerateFileName(const std::string& disposition,
                        const GURL& url,
                        bool need_html_ext,
                        base::FilePath::StringType* generated_name);

  // Main routine that initiates asking all frames for their savable resources.
  //
  // Responses are received asynchronously by OnSavableResourceLinks... methods
  // and pending responses are counted/tracked by
  // CompleteSavableResourceLinksResponse.
  //
  // OnSavableResourceLinksResponse creates SaveItems for each savable resource
  // and each subframe - these SaveItems get enqueued into |waiting_item_queue_|
  // with the help of CreatePendingSaveItem, EnqueueSavableResource,
  // EnqueueFrame.
  void GetSavableResourceLinks();

  // Response from |sender| frame to GetSavableResourceLinks request.
  void OnSavableResourceLinksResponse(
      RenderFrameHostImpl* sender,
      const std::vector<GURL>& resources_list,
      const Referrer& referrer,
      const std::vector<SavableSubframe>& subframes);

  // Helper for finding or creating a SaveItem with the given parameters.
  SaveItem* CreatePendingSaveItem(
      int container_frame_tree_node_id,
      const GURL& url,
      const Referrer& referrer,
      SaveFileCreateInfo::SaveFileSource save_source);

  // Helper for finding a SaveItem with the given url, or falling back to
  // creating a SaveItem with the given parameters.
  SaveItem* CreatePendingSaveItemDeduplicatingByUrl(
      int container_frame_tree_node_id,
      const GURL& url,
      const Referrer& referrer,
      SaveFileCreateInfo::SaveFileSource save_source);

  // Helper to enqueue a savable resource reported by GetSavableResourceLinks.
  void EnqueueSavableResource(int container_frame_tree_node_id,
                              const GURL& url,
                              const Referrer& referrer);
  // Helper to enqueue a subframe reported by GetSavableResourceLinks.
  void EnqueueFrame(int container_frame_tree_node_id,
                    int frame_tree_node_id,
                    const GURL& frame_original_url);

  // Response to GetSavableResourceLinks that indicates an error when processing
  // the frame associated with |sender|.
  void OnSavableResourceLinksError(RenderFrameHostImpl* sender);

  // Helper tracking how many |number_of_frames_pending_response_| we have
  // left and kicking off the next phase after we got all the
  // OnSavableResourceLinksResponse messages we were waiting for.
  void CompleteSavableResourceLinksResponse();

  // For each frame in the current page, ask the renderer process associated
  // with that frame to serialize that frame into html.
  void GetSerializedHtmlWithLocalLinks();

  // Ask renderer process to serialize |target_tree_node| into html data
  // with resource links replaced with a link to a locally saved copy.
  void GetSerializedHtmlWithLocalLinksForFrame(FrameTreeNode* target_tree_node);

  // Routes html data (sent by renderer process in response to
  // GetSerializedHtmlWithLocalLinksForFrame above) to the associated local file
  // (and also keeps track of when all frames have been completed).
  void OnSerializedHtmlWithLocalLinksResponse(RenderFrameHostImpl* sender,
                                              const std::string& data,
                                              bool end_of_data);

  // Look up SaveItem by save item id from in progress map.
  SaveItem* LookupSaveItemInProcess(SaveItemId save_item_id);

  // Remove SaveItem from in progress map and put it to saved map.
  void PutInProgressItemToSavedMap(SaveItem* save_item);

  // Retrieves the URL to be saved from the WebContents.
  GURL GetUrlToBeSaved();

  void CreateDirectoryOnFileThread(const base::FilePath& website_save_dir,
                                   const base::FilePath& download_save_dir,
                                   bool skip_dir_check,
                                   const std::string& mime_type,
                                   const std::string& accept_langs);
  void ContinueGetSaveInfo(const base::FilePath& suggested_path,
                           bool can_save_as_complete);
  void OnPathPicked(
      const base::FilePath& final_name,
      SavePageType type,
      const SavePackageDownloadCreatedCallback& cb);

  // Map from SaveItem::id() (aka save_item_id) into a SaveItem.
  typedef base::hash_map<SaveItemId, SaveItem*> SaveItemIdMap;
  // in_progress_items_ is map of all saving job in in-progress state.
  SaveItemIdMap in_progress_items_;
  // saved_failed_items_ is map of all saving job which are failed.
  SaveItemIdMap saved_failed_items_;

  // The number of in process SaveItems.
  int in_process_count() const {
    return static_cast<int>(in_progress_items_.size());
  }

  // The number of all SaveItems which have completed, including success items
  // and failed items.
  int completed_count() const {
    return static_cast<int>(saved_success_items_.size() +
                            saved_failed_items_.size());
  }

  // The current speed in files per second. This is used to update the
  // DownloadItem associated to this SavePackage. The files per second is
  // presented by the DownloadItem to the UI as bytes per second, which is
  // not correct but matches the way the total and received number of files is
  // presented as the total and received bytes.
  int64_t CurrentSpeed() const;

  // Helper function for preparing suggested name for the SaveAs Dialog. The
  // suggested name is determined by the web document's title.
  base::FilePath GetSuggestedNameForSaveAs(
      bool can_save_as_complete,
      const std::string& contents_mime_type,
      const std::string& accept_langs);

  // Ensures that the file name has a proper extension for HTML by adding ".htm"
  // if necessary.
  static base::FilePath EnsureHtmlExtension(const base::FilePath& name);

  // Ensures that the file name has a proper extension for supported formats
  // if necessary.
  static base::FilePath EnsureMimeExtension(const base::FilePath& name,
      const std::string& contents_mime_type);

  // Returns extension for supported MIME types (for example, for "text/plain"
  // it returns "txt").
  static const base::FilePath::CharType* ExtensionForMimeType(
      const std::string& contents_mime_type);

  typedef std::deque<SaveItem*> SaveItemQueue;
  // A queue for items we are about to start saving.
  SaveItemQueue waiting_item_queue_;

  // Used to de-dupe urls that are being gathered into |waiting_item_queue_|
  // and also to find SaveItems to associate with a containing frame.
  // Note that |url_to_save_item_| does NOT own SaveItems - they
  // remain owned by waiting_item_queue_, in_progress_items_, etc.
  std::map<GURL, SaveItem*> url_to_save_item_;

  // Map used to route responses from a given a subframe (i.e.
  // OnSerializedHtmlWithLocalLinksResponse) to the right SaveItem.
  // Note that |frame_tree_node_id_to_save_item_| does NOT own SaveItems - they
  // remain owned by waiting_item_queue_, in_progress_items_, etc.
  base::hash_map<int, SaveItem*> frame_tree_node_id_to_save_item_;

  // Used to limit which local paths get exposed to which frames
  // (i.e. to prevent information disclosure to oop frames).
  // Note that |frame_tree_node_id_to_contained_save_items_| does NOT own
  // SaveItems - they remain owned by waiting_item_queue_, in_progress_items_,
  // etc.
  base::hash_map<int, std::vector<SaveItem*>>
      frame_tree_node_id_to_contained_save_items_;

  // Number of frames that we still need to get a response from.
  int number_of_frames_pending_response_;

  // saved_success_items_ is map of all saving job which are successfully saved.
  base::hash_map<SaveItemId, SaveItem*> saved_success_items_;

  // Non-owning pointer for handling file writing on the file thread.
  SaveFileManager* file_manager_;

  // DownloadManager owns the DownloadItem and handles history and UI.
  DownloadManagerImpl* download_manager_;
  DownloadItemImpl* download_;

  // The URL of the page the user wants to save.
  GURL page_url_;
  base::FilePath saved_main_file_path_;
  base::FilePath saved_main_directory_path_;

  // The title of the page the user wants to save.
  base::string16 title_;

  // Used to calculate package download speed (in files per second).
  base::TimeTicks start_tick_;

  // Indicates whether the actual saving job is finishing or not.
  bool finished_;

  // Indicates whether a call to Finish() has been scheduled.
  bool mhtml_finishing_;

  // Indicates whether user canceled the saving job.
  bool user_canceled_;

  // Indicates whether user get disk error.
  bool disk_error_occurred_;

  // Type about saving page as only-html or complete-html.
  SavePageType save_type_;

  // Number of all need to be saved resources.
  size_t all_save_items_count_;

  using FileNameSet =
      std::set<base::FilePath::StringType,
               bool (*)(base::FilePath::StringPieceType,
                        base::FilePath::StringPieceType)>;
  // This set is used to eliminate duplicated file names in saving directory.
  FileNameSet file_name_set_;

  typedef base::hash_map<base::FilePath::StringType, uint32_t> FileNameCountMap;
  // This map is used to track serial number for specified filename.
  FileNameCountMap file_name_count_map_;

  // Indicates current waiting state when SavePackage try to get something
  // from outside.
  WaitState wait_state_;

  // Unique ID for this SavePackage.
  const SavePackageId unique_id_;

  // Variables to record errors that happened so we can record them via
  // UMA statistics.
  bool wrote_to_completed_file_;
  bool wrote_to_failed_file_;

  friend class SavePackageTest;
  FRIEND_TEST_ALL_PREFIXES(SavePackageTest, TestSuggestedSaveNames);
  FRIEND_TEST_ALL_PREFIXES(SavePackageTest, TestLongSafePureFilename);

  DISALLOW_COPY_AND_ASSIGN(SavePackage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_H_
