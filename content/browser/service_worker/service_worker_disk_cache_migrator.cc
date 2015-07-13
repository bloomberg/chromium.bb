// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_disk_cache_migrator.h"

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"

namespace content {

namespace {

// Disk cache entry data indices (Copied from appcache_diskcache.cc).
enum { kResponseInfoIndex, kResponseContentIndex, kResponseMetadataIndex };

}  // namespace

// A task to move a cached resource from the src DiskCache to the dest
// DiskCache. This is owned by ServiceWorkerDiskCacheMigrator.
class ServiceWorkerDiskCacheMigrator::Task {
 public:
  using MigrationStatusCallback = base::Callback<void(MigrationStatus)>;

  Task(InflightTaskMap::KeyType task_id,
       int64 resource_id,
       int32 data_size,
       ServiceWorkerDiskCache* src,
       ServiceWorkerDiskCache* dest,
       const MigrationStatusCallback& callback);
  ~Task();

  void Run();

  InflightTaskMap::KeyType task_id() const { return task_id_; }

 private:
  void ReadResponseInfo();
  void OnReadResponseInfo(
      const scoped_refptr<HttpResponseInfoIOBuffer>& info_buffer,
      int result);
  void OnWriteResponseInfo(
      const scoped_refptr<HttpResponseInfoIOBuffer>& info_buffer,
      int result);
  void WriteResponseMetadata(
      const scoped_refptr<HttpResponseInfoIOBuffer>& info_buffer);
  void OnWriteResponseMetadata(
      const scoped_refptr<HttpResponseInfoIOBuffer>& info_buffer,
      int result);
  void ReadResponseData();
  void OnReadResponseData(const scoped_refptr<net::IOBuffer>& buffer,
                          int result);
  void OnWriteResponseData(int result);

  InflightTaskMap::KeyType task_id_;
  int64 resource_id_;
  int32 data_size_;
  MigrationStatusCallback callback_;

  scoped_ptr<ServiceWorkerResponseReader> reader_;
  scoped_ptr<ServiceWorkerResponseWriter> writer_;
  scoped_ptr<ServiceWorkerResponseMetadataWriter> metadata_writer_;

  base::WeakPtrFactory<Task> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Task);
};

// A wrapper class for disk_cache::Entry. This is used for holding an open entry
// and ensuring that the entry gets closed on the dtor.
class ServiceWorkerDiskCacheMigrator::WrappedEntry {
 public:
  WrappedEntry() {}

  ~WrappedEntry() {
    if (entry_)
      entry_->Close();
  }

  disk_cache::Entry* Unwrap() {
    disk_cache::Entry* entry = entry_;
    entry_ = nullptr;
    return entry;
  }

  disk_cache::Entry** GetPtr() { return &entry_; }

 private:
  disk_cache::Entry* entry_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WrappedEntry);
};

ServiceWorkerDiskCacheMigrator::Task::Task(
    InflightTaskMap::KeyType task_id,
    int64 resource_id,
    int32 data_size,
    ServiceWorkerDiskCache* src,
    ServiceWorkerDiskCache* dest,
    const MigrationStatusCallback& callback)
    : task_id_(task_id),
      resource_id_(resource_id),
      data_size_(data_size),
      callback_(callback),
      weak_factory_(this) {
  DCHECK_LE(0, data_size_);
  reader_.reset(new ServiceWorkerResponseReader(resource_id, src));
  writer_.reset(new ServiceWorkerResponseWriter(resource_id, dest));
  metadata_writer_.reset(
      new ServiceWorkerResponseMetadataWriter(resource_id, dest));
}

ServiceWorkerDiskCacheMigrator::Task::~Task() {
}

void ServiceWorkerDiskCacheMigrator::Task::Run() {
  ReadResponseInfo();
}

void ServiceWorkerDiskCacheMigrator::Task::ReadResponseInfo() {
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer(
      new HttpResponseInfoIOBuffer);
  reader_->ReadInfo(info_buffer.get(),
                    base::Bind(&Task::OnReadResponseInfo,
                               weak_factory_.GetWeakPtr(), info_buffer));
}

void ServiceWorkerDiskCacheMigrator::Task::OnReadResponseInfo(
    const scoped_refptr<HttpResponseInfoIOBuffer>& info_buffer,
    int result) {
  if (result < 0) {
    LOG(ERROR) << "Failed to read the response info";
    callback_.Run(MigrationStatus::ERROR_READ_RESPONSE_INFO);
    return;
  }
  writer_->WriteInfo(info_buffer.get(),
                     base::Bind(&Task::OnWriteResponseInfo,
                                weak_factory_.GetWeakPtr(), info_buffer));
}

void ServiceWorkerDiskCacheMigrator::Task::OnWriteResponseInfo(
    const scoped_refptr<HttpResponseInfoIOBuffer>& buffer,
    int result) {
  if (result < 0) {
    LOG(ERROR) << "Failed to write the response info";
    callback_.Run(MigrationStatus::ERROR_WRITE_RESPONSE_INFO);
    return;
  }

  const net::HttpResponseInfo* http_info = buffer->http_info.get();
  if (http_info->metadata && http_info->metadata->size()) {
    WriteResponseMetadata(buffer);
    return;
  }
  ReadResponseData();
}

void ServiceWorkerDiskCacheMigrator::Task::WriteResponseMetadata(
    const scoped_refptr<HttpResponseInfoIOBuffer>& info_buffer) {
  const net::HttpResponseInfo* http_info = info_buffer->http_info.get();
  DCHECK(http_info->metadata);
  DCHECK(http_info->metadata->size());

  // |wrapped_buffer| does not own the given metadata buffer, so a callback
  // for WriteMetadata keeps |info_buffer| which is the real owner of the
  // metadata buffer.
  scoped_refptr<net::WrappedIOBuffer> wrapped_buffer =
      new net::WrappedIOBuffer(http_info->metadata->data());
  metadata_writer_->WriteMetadata(
      wrapped_buffer.get(), http_info->metadata->size(),
      base::Bind(&Task::OnWriteResponseMetadata, weak_factory_.GetWeakPtr(),
                 info_buffer));
}

void ServiceWorkerDiskCacheMigrator::Task::OnWriteResponseMetadata(
    const scoped_refptr<HttpResponseInfoIOBuffer>& info_buffer,
    int result) {
  if (result < 0) {
    LOG(ERROR) << "Failed to write the response metadata";
    callback_.Run(MigrationStatus::ERROR_WRITE_RESPONSE_METADATA);
    return;
  }
  DCHECK_EQ(info_buffer->http_info->metadata->size(), result);
  ReadResponseData();
}

void ServiceWorkerDiskCacheMigrator::Task::ReadResponseData() {
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(data_size_);
  reader_->ReadData(buffer.get(), data_size_,
                    base::Bind(&Task::OnReadResponseData,
                               weak_factory_.GetWeakPtr(), buffer));
}

void ServiceWorkerDiskCacheMigrator::Task::OnReadResponseData(
    const scoped_refptr<net::IOBuffer>& buffer,
    int result) {
  if (result < 0) {
    LOG(ERROR) << "Failed to read the response data";
    callback_.Run(MigrationStatus::ERROR_READ_RESPONSE_DATA);
    return;
  }
  DCHECK_EQ(data_size_, result);
  writer_->WriteData(
      buffer.get(), result,
      base::Bind(&Task::OnWriteResponseData, weak_factory_.GetWeakPtr()));
}

void ServiceWorkerDiskCacheMigrator::Task::OnWriteResponseData(int result) {
  if (result < 0) {
    LOG(ERROR) << "Failed to write the response data";
    callback_.Run(MigrationStatus::ERROR_WRITE_RESPONSE_DATA);
    return;
  }
  DCHECK_EQ(data_size_, result);
  callback_.Run(MigrationStatus::OK);
}

ServiceWorkerDiskCacheMigrator::ServiceWorkerDiskCacheMigrator(
    const base::FilePath& src_path,
    const base::FilePath& dest_path,
    int max_disk_cache_size,
    const scoped_refptr<base::SingleThreadTaskRunner>& disk_cache_thread)
    : src_path_(src_path),
      dest_path_(dest_path),
      max_disk_cache_size_(max_disk_cache_size),
      disk_cache_thread_(disk_cache_thread),
      weak_factory_(this) {
}

ServiceWorkerDiskCacheMigrator::~ServiceWorkerDiskCacheMigrator() {
}

void ServiceWorkerDiskCacheMigrator::Start(const StatusCallback& callback) {
  callback_ = callback;
  start_time_ = base::TimeTicks::Now();

#if defined(OS_ANDROID)
  PostTaskAndReplyWithResult(
      disk_cache_thread_.get(), FROM_HERE,
      base::Bind(&MigrateForAndroid, src_path_, dest_path_),
      base::Bind(&ServiceWorkerDiskCacheMigrator::Complete,
                 weak_factory_.GetWeakPtr()));
#else
  PostTaskAndReplyWithResult(
      disk_cache_thread_.get(), FROM_HERE,
      base::Bind(&base::DeleteFile, dest_path_, true),
      base::Bind(&ServiceWorkerDiskCacheMigrator::DidDeleteDestDirectory,
                 weak_factory_.GetWeakPtr()));
#endif  // defined(OS_ANDROID)
}

#if defined(OS_ANDROID)
// static
ServiceWorkerDiskCacheMigrator::MigrationStatus
ServiceWorkerDiskCacheMigrator::MigrateForAndroid(
    const base::FilePath& src_path,
    const base::FilePath& dest_path) {
  // Continue the migration regardless of the deletion result. If the migrator
  // cannot proceed or the diskcache gets corrupted due to the failure, the
  // storage detects it and recovers by DeleteAndStartOver.
  base::DeleteFile(dest_path, true);

  if (!base::DirectoryExists(src_path))
    return MigrationStatus::OK;

  // Android has alredy used the Simple backend. Just move the existing
  // diskcache files to a new location.
  if (base::Move(src_path, dest_path))
    return MigrationStatus::OK;
  return MigrationStatus::ERROR_MOVE_DISKCACHE;
}
#endif  // defined(OS_ANDROID)

void ServiceWorkerDiskCacheMigrator::DidDeleteDestDirectory(bool deleted) {
  // Continue the migration regardless of the deletion result. If the migrator
  // cannot proceed or the diskcache gets corrupted due to the failure, the
  // storage detects it and recovers by DeleteAndStartOver.

  src_ = ServiceWorkerDiskCache::CreateWithBlockFileBackend();
  dest_ = ServiceWorkerDiskCache::CreateWithSimpleBackend();

  scoped_ptr<MigrationStatus> status(new MigrationStatus(MigrationStatus::OK));
  MigrationStatus* status_ptr = status.get();

  // This closure is called when both diskcaches are initialized.
  base::Closure barrier_closure = base::BarrierClosure(
      2, base::Bind(&ServiceWorkerDiskCacheMigrator::DidInitializeAllDiskCaches,
                    weak_factory_.GetWeakPtr(), base::Passed(status.Pass())));

  // Initialize the src DiskCache. |status_ptr| is guaranteed to be valid until
  // calling |barrier_closure| which is the owner of the object.
  net::CompletionCallback src_callback =
      base::Bind(&ServiceWorkerDiskCacheMigrator::DidInitializeSrcDiskCache,
                 weak_factory_.GetWeakPtr(), barrier_closure, status_ptr);
  int result = src_->InitWithDiskBackend(src_path_, max_disk_cache_size_,
                                         false /* force */, disk_cache_thread_,
                                         src_callback);
  if (result != net::ERR_IO_PENDING)
    src_callback.Run(result);

  // Initialize the dest DiskCache. |status_ptr| is guaranteed to be valid until
  // calling |barrier_closure| which is the owner of the object.
  net::CompletionCallback dest_callback =
      base::Bind(&ServiceWorkerDiskCacheMigrator::DidInitializeDestDiskCache,
                 weak_factory_.GetWeakPtr(), barrier_closure, status_ptr);
  result = dest_->InitWithDiskBackend(dest_path_, max_disk_cache_size_,
                                      false /* force */, disk_cache_thread_,
                                      dest_callback);
  if (result != net::ERR_IO_PENDING)
    dest_callback.Run(result);
}

void ServiceWorkerDiskCacheMigrator::DidInitializeSrcDiskCache(
    const base::Closure& barrier_closure,
    MigrationStatus* status_ptr,
    int result) {
  if (result != net::OK)
    *status_ptr = MigrationStatus::ERROR_INIT_SRC_DISKCACHE;
  barrier_closure.Run();
}

void ServiceWorkerDiskCacheMigrator::DidInitializeDestDiskCache(
    const base::Closure& barrier_closure,
    MigrationStatus* status_ptr,
    int result) {
  if (result != net::OK)
    *status_ptr = MigrationStatus::ERROR_INIT_DEST_DISKCACHE;
  barrier_closure.Run();
}

void ServiceWorkerDiskCacheMigrator::DidInitializeAllDiskCaches(
    scoped_ptr<MigrationStatus> status) {
  if (*status != MigrationStatus::OK) {
    LOG(ERROR) << "Failed to initialize the diskcache";
    Complete(*status);
    return;
  }

  // Iterate through existing entries in the src DiskCache.
  iterator_ = src_->disk_cache()->CreateIterator();
  OpenNextEntry();
}

void ServiceWorkerDiskCacheMigrator::OpenNextEntry() {
  DCHECK(!pending_task_);
  DCHECK(!is_iterating_);
  is_iterating_ = true;

  scoped_ptr<WrappedEntry> wrapped_entry(new WrappedEntry);
  disk_cache::Entry** entry_ptr = wrapped_entry->GetPtr();

  net::CompletionCallback callback = base::Bind(
      &ServiceWorkerDiskCacheMigrator::OnNextEntryOpened,
      weak_factory_.GetWeakPtr(), base::Passed(wrapped_entry.Pass()));
  int result = iterator_->OpenNextEntry(entry_ptr, callback);
  if (result == net::ERR_IO_PENDING)
    return;
  callback.Run(result);
}

void ServiceWorkerDiskCacheMigrator::OnNextEntryOpened(
    scoped_ptr<WrappedEntry> wrapped_entry,
    int result) {
  DCHECK(!pending_task_);
  is_iterating_ = false;

  if (result == net::ERR_FAILED) {
    // ERR_FAILED means the iterator reached the end of the enumeration.
    if (inflight_tasks_.IsEmpty())
      Complete(MigrationStatus::OK);
    return;
  }

  if (result != net::OK) {
    LOG(ERROR) << "Failed to open the next entry";
    inflight_tasks_.Clear();
    Complete(MigrationStatus::ERROR_OPEN_NEXT_ENTRY);
    return;
  }

  disk_cache::ScopedEntryPtr scoped_entry(wrapped_entry->Unwrap());
  DCHECK(scoped_entry);

  int64 resource_id = kInvalidServiceWorkerResourceId;
  if (!base::StringToInt64(scoped_entry->GetKey(), &resource_id)) {
    LOG(ERROR) << "Failed to read the resource id";
    inflight_tasks_.Clear();
    Complete(MigrationStatus::ERROR_READ_ENTRY_KEY);
    return;
  }

  InflightTaskMap::KeyType task_id = next_task_id_++;
  pending_task_.reset(new Task(
      task_id, resource_id, scoped_entry->GetDataSize(kResponseContentIndex),
      src_.get(), dest_.get(),
      base::Bind(&ServiceWorkerDiskCacheMigrator::OnEntryMigrated,
                 weak_factory_.GetWeakPtr(), task_id)));
  if (inflight_tasks_.size() < max_number_of_inflight_tasks_) {
    RunPendingTask();
    OpenNextEntry();
    return;
  }
  // |pending_task_| will run when an inflight task is completed.
}

void ServiceWorkerDiskCacheMigrator::RunPendingTask() {
  DCHECK(pending_task_);
  DCHECK_GT(max_number_of_inflight_tasks_, inflight_tasks_.size());
  InflightTaskMap::KeyType task_id = pending_task_->task_id();
  pending_task_->Run();
  inflight_tasks_.AddWithID(pending_task_.release(), task_id);
}

void ServiceWorkerDiskCacheMigrator::OnEntryMigrated(
    InflightTaskMap::KeyType task_id,
    MigrationStatus status) {
  DCHECK(inflight_tasks_.Lookup(task_id));
  inflight_tasks_.Remove(task_id);

  if (status != MigrationStatus::OK) {
    inflight_tasks_.Clear();
    pending_task_.reset();
    Complete(status);
    return;
  }

  ++number_of_migrated_resources_;

  if (pending_task_) {
    RunPendingTask();
    OpenNextEntry();
    return;
  }

  if (is_iterating_)
    return;

  if (inflight_tasks_.IsEmpty())
    Complete(MigrationStatus::OK);
}

void ServiceWorkerDiskCacheMigrator::Complete(MigrationStatus status) {
  DCHECK(inflight_tasks_.IsEmpty());
  DCHECK(!pending_task_);

  if (status == MigrationStatus::OK) {
    RecordMigrationTime(base::TimeTicks().Now() - start_time_);
    RecordNumberOfMigratedResources(number_of_migrated_resources_);
  }
  RecordMigrationResult(status);

  // Invalidate weakptrs to ensure that other running operations do not call
  // OnEntryMigrated().
  weak_factory_.InvalidateWeakPtrs();

  // Use PostTask to avoid deleting AppCacheDiskCache in the middle of the
  // execution (see http://crbug.com/502420).
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ServiceWorkerDiskCacheMigrator::RunUserCallback,
                 weak_factory_.GetWeakPtr(),
                 status == MigrationStatus::OK ? SERVICE_WORKER_OK
                                               : SERVICE_WORKER_ERROR_FAILED));
}

void ServiceWorkerDiskCacheMigrator::RunUserCallback(
    ServiceWorkerStatusCode status) {
  src_.reset();
  dest_.reset();
  callback_.Run(status);
}

void ServiceWorkerDiskCacheMigrator::RecordMigrationResult(
    MigrationStatus status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.DiskCacheMigrator.MigrationResult",
                            static_cast<int>(status),
                            static_cast<int>(MigrationStatus::NUM_TYPES));
}

void ServiceWorkerDiskCacheMigrator::RecordNumberOfMigratedResources(
    size_t migrated_resources) {
  UMA_HISTOGRAM_COUNTS_1000(
      "ServiceWorker.DiskCacheMigrator.NumberOfMigratedResources",
      migrated_resources);
}

void ServiceWorkerDiskCacheMigrator::RecordMigrationTime(
    const base::TimeDelta& time) {
  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.DiskCacheMigrator.MigrationTime",
                             time);
}

}  // namespace content
