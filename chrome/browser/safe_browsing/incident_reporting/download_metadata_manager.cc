// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/download_metadata_manager.h"

#include <list>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"

namespace safe_browsing {

namespace {

// Histogram bucket values for metadata read operations. Do not reorder.
enum MetadataReadResult {
  READ_SUCCESS = 0,
  OPEN_FAILURE = 1,
  NOT_FOUND = 2,
  GET_INFO_FAILURE = 3,
  FILE_TOO_BIG = 4,
  READ_FAILURE = 5,
  PARSE_FAILURE = 6,
  MALFORMED_DATA = 7,
  NUM_READ_RESULTS
};

// Histogram bucket values for metadata write operations. Do not reorder.
enum MetadataWriteResult {
  WRITE_SUCCESS = 0,
  SERIALIZATION_FAILURE = 1,
  WRITE_FAILURE = 2,
  NUM_WRITE_RESULTS
};

// The name of the metadata file in the profile directory.
const base::FilePath::CharType kDownloadMetadataBasename[] =
    FILE_PATH_LITERAL("DownloadMetadata");

// Returns the path to the metadata file for |browser_context|.
base::FilePath GetMetadataPath(content::BrowserContext* browser_context) {
  return browser_context->GetPath().Append(kDownloadMetadataBasename);
}

// Returns true if |metadata| appears to be valid.
bool MetadataIsValid(const DownloadMetadata& metadata) {
  return (metadata.has_download_id() &&
          metadata.has_download() &&
          metadata.download().has_download() &&
          metadata.download().download().has_url());
}

// Reads and parses a DownloadMetadata message from |metadata_path| into
// |metadata|.
void ReadMetadataOnWorkerPool(const base::FilePath& metadata_path,
                              DownloadMetadata* metadata) {
  using base::File;
  DCHECK(metadata);
  MetadataReadResult result = NUM_READ_RESULTS;
  File metadata_file(metadata_path, File::FLAG_OPEN | File::FLAG_READ);
  if (metadata_file.IsValid()) {
    base::File::Info info;
    if (metadata_file.GetInfo(&info)) {
      if (info.size <= INT_MAX) {
        const int size = static_cast<int>(info.size);
        scoped_ptr<char[]> file_data(new char[info.size]);
        if (metadata_file.Read(0, file_data.get(), size)) {
          if (!metadata->ParseFromArray(file_data.get(), size))
            result = PARSE_FAILURE;
          else if (!MetadataIsValid(*metadata))
            result = MALFORMED_DATA;
          else
            result = READ_SUCCESS;
        } else {
          result = READ_FAILURE;
        }
      } else {
        result = FILE_TOO_BIG;
      }
    } else {
      result = GET_INFO_FAILURE;
    }
  } else if (metadata_file.error_details() != File::FILE_ERROR_NOT_FOUND) {
    result = OPEN_FAILURE;
  } else {
    result = NOT_FOUND;
  }
  if (result != READ_SUCCESS)
    metadata->Clear();
  UMA_HISTOGRAM_ENUMERATION(
      "SBIRS.DownloadMetadata.ReadResult", result, NUM_READ_RESULTS);
}

// Writes |download_metadata| to |metadata_path|.
void WriteMetadataOnWorkerPool(const base::FilePath& metadata_path,
                               DownloadMetadata* download_metadata) {
  MetadataWriteResult result = NUM_WRITE_RESULTS;
  std::string file_data;
  if (download_metadata->SerializeToString(&file_data)) {
    result =
        base::ImportantFileWriter::WriteFileAtomically(metadata_path, file_data)
            ? WRITE_SUCCESS
            : WRITE_FAILURE;
  } else {
    result = SERIALIZATION_FAILURE;
  }
  UMA_HISTOGRAM_ENUMERATION(
      "SBIRS.DownloadMetadata.WriteResult", result, NUM_WRITE_RESULTS);
}

// Deletes |metadata_path|.
void DeleteMetadataOnWorkerPool(const base::FilePath& metadata_path) {
  bool success = base::DeleteFile(metadata_path, false /* not recursive */);
  UMA_HISTOGRAM_BOOLEAN("SBIRS.DownloadMetadata.DeleteSuccess", success);
}

// Runs |callback| with the DownloadDetails in |download_metadata|.
void ReturnResults(
    const DownloadMetadataManager::GetDownloadDetailsCallback& callback,
    scoped_ptr<DownloadMetadata> download_metadata) {
  if (!download_metadata->has_download_id())
    callback.Run(scoped_ptr<ClientIncidentReport_DownloadDetails>());
  else
    callback.Run(make_scoped_ptr(download_metadata->release_download()).Pass());
}

}  // namespace

// Applies operations to the profile's persistent DownloadMetadata as they occur
// on its corresponding download item. An instance can be in one of three
// states: waiting for metatada load, waiting for metadata to load after its
// corresponding DownloadManager has gone down, and not waiting for metadata to
// load. While it is waiting for metadata to load, it observes all download
// items and records operations on them. Once the metadata is ready, recorded
// operations are applied and observers are removed from all items but the one
// corresponding to the metadata (if it exists). The instance continues to
// observe its related item, and applies operations on it accordingly. While
// waiting for metadata to load, an instance also tracks callbacks to be run to
// provide consumers with persisted details of a download.
class DownloadMetadataManager::ManagerContext
    : public content::DownloadItem::Observer {
 public:
  ManagerContext(const scoped_refptr<base::SequencedTaskRunner>& read_runner,
                 const scoped_refptr<base::SequencedTaskRunner>& write_runner,
                 content::DownloadManager* download_manager);

  // Detaches this context from its owner. The owner must not access the context
  // following this call. The context will be deleted immediately if it is not
  // waiting for a metadata load with either recorded operations or pending
  // callbacks.
  void Detach();

  // Notifies the context that |download| has been added to its manager.
  void OnDownloadCreated(content::DownloadItem* download);

  // Sets |request| as the relevant metadata to persist for |download|.
  void SetRequest(content::DownloadItem* download,
                  const ClientDownloadRequest& request);

  // Removes metadata for the context from memory and posts a task in the worker
  // pool to delete it on disk.
  void RemoveMetadata();

  // Gets the persisted DownloadDetails. |callback| will be run immediately if
  // the data is available. Otherwise, it will be run later on the caller's
  // thread.
  void GetDownloadDetails(const GetDownloadDetailsCallback& callback);

 protected:
  // content::DownloadItem::Observer methods.
  void OnDownloadOpened(content::DownloadItem* download) override;
  void OnDownloadRemoved(content::DownloadItem* download) override;
  void OnDownloadDestroyed(content::DownloadItem* download) override;

 private:
  enum State {
    // The context is waiting for the metadata file to be loaded.
    WAITING_FOR_LOAD,

    // The context is waiting for the metadata file to be loaded and its
    // corresponding DownloadManager has gone away.
    DETACHED_WAIT,

    // The context has loaded the metadata file.
    LOAD_COMPLETE,
  };

  struct ItemData {
    ItemData() : item(), removed() {}
    // null if the download has been destroyed. If non-null, the item is being
    // observed.
    content::DownloadItem* item;
    base::Time last_opened_time;
    bool removed;
  };

  // A mapping of download IDs to their corresponding data.
  typedef std::map<uint32_t, ItemData> ItemDataMap;

  ~ManagerContext() override;

  // Clears the |pending_items_| mapping, removing observers in the process.
  void ClearPendingItems();

  // Runs all |get_details_callbacks_| with the current metadata.
  void RunCallbacks();

  // Returns the id of the download corresponding to the loaded metadata, or
  // kInvalidId if metadata has not finished loading or is not present.
  uint32_t GetDownloadId() const;

  // Posts a task in the worker pool to read the metadata from disk.
  void ReadMetadata();

  // A callback run on the main thread with the results from reading the
  // metadata file from disk.
  void OnMetadataReady(scoped_ptr<DownloadMetadata> download_metadata);

  // Updates the last opened time in the metadata and writes it to disk.
  void UpdateLastOpenedTime(const base::Time& last_opened_time);

  // Posts a task in the worker pool to write the metadata to disk.
  void WriteMetadata();

  // A task runner to which read tasks are posted.
  scoped_refptr<base::SequencedTaskRunner> read_runner_;

  // A task runner to which write tasks are posted.
  scoped_refptr<base::SequencedTaskRunner> write_runner_;

  // The path to the metadata file for this context.
  base::FilePath metadata_path_;

  // When not LOAD_COMPLETE, the context is waiting for a pending read operation
  // to complete. While this is the case, all added download items are observed
  // and events are temporarily recorded in |pending_items_|. Once the read
  // completes, pending operations for the item correponding to the metadata
  // file are applied to the file and all other recorded data are dropped (and
  // corresponding observers are removed). Queued GetDownloadDetailsCallbacks
  // are run upon read completion as well. The context is moved to the
  // DETACHED_WAIT state if the corresponding DownloadManager goes away while a
  // read operation is outstanding. When the read subsequently completes, the
  // context is destroyed after the processing described above is performed.
  State state_;

  // The current metadata for the context. May be supplied either by reading
  // from the file or by having been set via |SetRequest|.
  scoped_ptr<DownloadMetadata> download_metadata_;

  // The operation data that accumulates for added download items while the
  // metadata file is being read.
  ItemDataMap pending_items_;

  // The download item corresponding to the download_metadata_. When non-null,
  // the context observes events on this item only.
  content::DownloadItem* item_;

  // Pending callbacks in response to GetDownloadDetails. The callbacks are run
  // in order when a pending read operation completes.
  std::list<GetDownloadDetailsCallback> get_details_callbacks_;

  base::WeakPtrFactory<ManagerContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagerContext);
};

DownloadMetadataManager::DownloadMetadataManager(
    const scoped_refptr<base::SequencedWorkerPool>& worker_pool) {
  base::SequencedWorkerPool::SequenceToken token(
      worker_pool->GetSequenceToken());
  // Do not block shutdown on reads since nothing will come of it.
  read_runner_ = worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
      token, base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  // Block shutdown on writes only if they've already begun.
  write_runner_ = worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
      token, base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

DownloadMetadataManager::DownloadMetadataManager(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : read_runner_(task_runner), write_runner_(task_runner) {
}

DownloadMetadataManager::~DownloadMetadataManager() {
  // Destruction may have taken place before managers have gone down.
  for (const auto& value : contexts_) {
    value.first->RemoveObserver(this);
    value.second->Detach();
  }
  contexts_.clear();
}

void DownloadMetadataManager::AddDownloadManager(
    content::DownloadManager* download_manager) {
  DCHECK_EQ(contexts_.count(download_manager), 0U);
  download_manager->AddObserver(this);
  contexts_[download_manager] =
      new ManagerContext(read_runner_, write_runner_, download_manager);
}

void DownloadMetadataManager::SetRequest(content::DownloadItem* item,
                                         const ClientDownloadRequest* request) {
  content::DownloadManager* download_manager =
      GetDownloadManagerForBrowserContext(item->GetBrowserContext());
  DCHECK_EQ(contexts_.count(download_manager), 1U);
  if (request)
    contexts_[download_manager]->SetRequest(item, *request);
  else
    contexts_[download_manager]->RemoveMetadata();
}

void DownloadMetadataManager::GetDownloadDetails(
    content::BrowserContext* browser_context,
    const GetDownloadDetailsCallback& callback) {
  DCHECK(browser_context);
  // The DownloadManager for |browser_context| may not have been created yet. In
  // this case, asking for it would cause history to load in the background and
  // wouldn't really help much. Instead, scan the contexts to see if one belongs
  // to |browser_context|. If one is not found, read the metadata and return it.
  scoped_ptr<ClientIncidentReport_DownloadDetails> download_details;
  for (const auto& value : contexts_) {
    if (value.first->GetBrowserContext() == browser_context) {
      value.second->GetDownloadDetails(callback);
      return;
    }
  }

  // Fire off a task to load the details and return them to the caller.
  DownloadMetadata* metadata = new DownloadMetadata();
  read_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ReadMetadataOnWorkerPool,
                 GetMetadataPath(browser_context),
                 metadata),
      base::Bind(
          &ReturnResults, callback, base::Passed(make_scoped_ptr(metadata))));
}

content::DownloadManager*
DownloadMetadataManager::GetDownloadManagerForBrowserContext(
    content::BrowserContext* context) {
  return content::BrowserContext::GetDownloadManager(context);
}

void DownloadMetadataManager::OnDownloadCreated(
    content::DownloadManager* download_manager,
    content::DownloadItem* item) {
  DCHECK_EQ(contexts_.count(download_manager), 1U);
  contexts_[download_manager]->OnDownloadCreated(item);
}

void DownloadMetadataManager::ManagerGoingDown(
    content::DownloadManager* download_manager) {
  DCHECK_EQ(contexts_.count(download_manager), 1U);
  auto iter = contexts_.find(download_manager);
  iter->second->Detach();
  contexts_.erase(iter);
}

DownloadMetadataManager::ManagerContext::ManagerContext(
    const scoped_refptr<base::SequencedTaskRunner>& read_runner,
    const scoped_refptr<base::SequencedTaskRunner>& write_runner,
    content::DownloadManager* download_manager)
    : read_runner_(read_runner),
      write_runner_(write_runner),
      metadata_path_(GetMetadataPath(download_manager->GetBrowserContext())),
      state_(WAITING_FOR_LOAD),
      item_(),
      weak_factory_(this) {
  // Start the asynchronous task to read the persistent metadata.
  ReadMetadata();
}

void DownloadMetadataManager::ManagerContext::Detach() {
  // Delete the instance immediately if there's no work to process after a
  // pending read completes.
  if (get_details_callbacks_.empty() && pending_items_.empty()) {
    delete this;
  } else {
    // delete the instance in OnMetadataReady.
    state_ = DETACHED_WAIT;
  }
}

void DownloadMetadataManager::ManagerContext::OnDownloadCreated(
    content::DownloadItem* download) {
  const uint32_t id = download->GetId();
  if (state_ != LOAD_COMPLETE) {
    // Observe all downloads and record operations while waiting for metadata to
    // load.
    download->AddObserver(this);
    pending_items_[id].item = download;
  } else if (id == GetDownloadId()) {
    // Observe this one item if it is the important one.
    DCHECK_EQ(item_, static_cast<content::DownloadItem*>(nullptr));
    item_ = download;
    download->AddObserver(this);
  }
}

void DownloadMetadataManager::ManagerContext::SetRequest(
    content::DownloadItem* download,
    const ClientDownloadRequest& request) {
  if (state_ != LOAD_COMPLETE) {
    // Abandon the read task since |download| is the new top dog.
    weak_factory_.InvalidateWeakPtrs();
    state_ = LOAD_COMPLETE;
    // Stop observing all items and drop any recorded operations.
    ClearPendingItems();
  }
  // Observe this item from here on out.
  if (item_ != download) {
    if (item_)
      item_->RemoveObserver(this);
    download->AddObserver(this);
    item_ = download;
  }
  // Take the request.
  download_metadata_.reset(new DownloadMetadata);
  download_metadata_->set_download_id(download->GetId());
  download_metadata_->mutable_download()->mutable_download()->CopyFrom(request);
  base::Time end_time = download->GetEndTime();
  // The item may have yet to be marked as complete, so consider the current
  // time as being its download time.
  if (end_time.is_null())
    end_time = base::Time::Now();
  download_metadata_->mutable_download()->set_download_time_msec(
      end_time.ToJavaTime());
  // Persist it.
  WriteMetadata();
  // Run callbacks (only present in case of a transition to LOAD_COMPLETE).
  RunCallbacks();
}

void DownloadMetadataManager::ManagerContext::RemoveMetadata() {
  if (state_ != LOAD_COMPLETE) {
    // Abandon the read task since the file is to be removed.
    weak_factory_.InvalidateWeakPtrs();
    state_ = LOAD_COMPLETE;
    // Stop observing all items and drop any recorded operations.
    ClearPendingItems();
  }
  // Remove any metadata.
  download_metadata_.reset();
  // Stop observing this item.
  if (item_) {
    item_->RemoveObserver(this);
    item_= nullptr;
  }
  write_runner_->PostTask(
      FROM_HERE, base::Bind(&DeleteMetadataOnWorkerPool, metadata_path_));
  // Run callbacks (only present in case of a transition to LOAD_COMPLETE).
  RunCallbacks();
}

void DownloadMetadataManager::ManagerContext::GetDownloadDetails(
    const GetDownloadDetailsCallback& callback) {
  if (state_ != LOAD_COMPLETE) {
    get_details_callbacks_.push_back(callback);
  } else {
    if (download_metadata_) {
      callback.Run(make_scoped_ptr(new ClientIncidentReport_DownloadDetails(
                                       download_metadata_->download())).Pass());
    } else {
      callback.Run(scoped_ptr<ClientIncidentReport_DownloadDetails>());
    }
  }
}

void DownloadMetadataManager::ManagerContext::OnDownloadOpened(
    content::DownloadItem* download) {
  const base::Time now = base::Time::Now();
  if (state_ != LOAD_COMPLETE)
    pending_items_[download->GetId()].last_opened_time = now;
  else
    UpdateLastOpenedTime(now);
}

void DownloadMetadataManager::ManagerContext::OnDownloadRemoved(
    content::DownloadItem* download) {
  if (state_ != LOAD_COMPLETE)
    pending_items_[download->GetId()].removed = true;
  else
    RemoveMetadata();
}

void DownloadMetadataManager::ManagerContext::OnDownloadDestroyed(
    content::DownloadItem* download) {
  if (state_ != LOAD_COMPLETE) {
    // Erase the data for this item if nothing of import happened. Otherwise
    // clear its item pointer since it is now invalid.
    auto iter = pending_items_.find(download->GetId());
    DCHECK(iter != pending_items_.end());
    if (!iter->second.removed && iter->second.last_opened_time.is_null())
      pending_items_.erase(iter);
    else
      iter->second.item = nullptr;
  } else if (item_) {
    // This item is no longer being observed.
    DCHECK_EQ(item_, download);
    item_ = nullptr;
  }
}

DownloadMetadataManager::ManagerContext::~ManagerContext() {
  // A context should not be deleted while waiting for a load to complete.
  DCHECK(pending_items_.empty());
  DCHECK(get_details_callbacks_.empty());

  // The context may have detached while still observing the item of interest
  // since a DownloadManager announces that it's going down before it destroyes
  // its items.
  if (item_) {
    item_->RemoveObserver(this);
    item_ = nullptr;
  }
}

void DownloadMetadataManager::ManagerContext::ClearPendingItems() {
  for (const auto& value : pending_items_) {
    if (value.second.item)
      value.second.item->RemoveObserver(this);
  }
  pending_items_.clear();
}

void DownloadMetadataManager::ManagerContext::RunCallbacks() {
  while (!get_details_callbacks_.empty()) {
    const auto& callback = get_details_callbacks_.front();
    if (download_metadata_) {
      callback.Run(make_scoped_ptr(new ClientIncidentReport_DownloadDetails(
                                       download_metadata_->download())).Pass());
    } else {
      callback.Run(scoped_ptr<ClientIncidentReport_DownloadDetails>());
    }
    get_details_callbacks_.pop_front();
  }
}

uint32_t DownloadMetadataManager::ManagerContext::GetDownloadId() const {
  if (state_ != LOAD_COMPLETE || !download_metadata_)
    return content::DownloadItem::kInvalidId;
  return download_metadata_->download_id();
}

void DownloadMetadataManager::ManagerContext::ReadMetadata() {
  DCHECK_NE(state_, LOAD_COMPLETE);

  DownloadMetadata* metadata = new DownloadMetadata();
  // Do not block shutdown on this read since nothing will come of it.
  read_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ReadMetadataOnWorkerPool, metadata_path_, metadata),
      base::Bind(&DownloadMetadataManager::ManagerContext::OnMetadataReady,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(make_scoped_ptr(metadata))));
}

void DownloadMetadataManager::ManagerContext::OnMetadataReady(
    scoped_ptr<DownloadMetadata> download_metadata) {
  DCHECK_NE(state_, LOAD_COMPLETE);

  const bool is_detached = (state_ == DETACHED_WAIT);

  // Note that any available data has been read.
  state_ = LOAD_COMPLETE;
  if (download_metadata->has_download_id())
    download_metadata_ = download_metadata.Pass();
  else
    download_metadata_.reset();

  // Process all operations that had been held while waiting for the metadata.
  content::DownloadItem* download = nullptr;
  {
    const auto& iter = pending_items_.find(GetDownloadId());
    if (iter != pending_items_.end()) {
      const ItemData& item_data = iter->second;
      download = item_data.item;  // non-null if not destroyed.
      if (item_data.removed) {
        RemoveMetadata();
        download = nullptr;
      } else if (!item_data.last_opened_time.is_null()) {
        UpdateLastOpenedTime(item_data.last_opened_time);
      }
    }
  }

  // Stop observing all items.
  ClearPendingItems();

  // If the download was known and not removed, observe it from here on out.
  if (download) {
    download->AddObserver(this);
    item_ = download;
  }

  // Run callbacks.
  RunCallbacks();

  // Delete the context now if it has been detached.
  if (is_detached)
    delete this;
}

void DownloadMetadataManager::ManagerContext::UpdateLastOpenedTime(
    const base::Time& last_opened_time) {
  download_metadata_->mutable_download()->set_open_time_msec(
      last_opened_time.ToJavaTime());
  WriteMetadata();
}

void DownloadMetadataManager::ManagerContext::WriteMetadata() {
  write_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WriteMetadataOnWorkerPool,
                 metadata_path_,
                 base::Owned(new DownloadMetadata(*download_metadata_))));
}

}  // namespace safe_browsing
