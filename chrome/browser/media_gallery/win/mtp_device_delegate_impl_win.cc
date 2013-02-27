// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MTPDeviceDelegateImplWin implementation.

#include "chrome/browser/media_gallery/win/mtp_device_delegate_impl_win.h"

#include <portabledevice.h>

#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/media_gallery/win/mtp_device_object_entry.h"
#include "chrome/browser/media_gallery/win/mtp_device_object_enumerator.h"
#include "chrome/browser/media_gallery/win/mtp_device_operations_util.h"
#include "chrome/browser/media_gallery/win/recursive_mtp_device_object_enumerator.h"
#include "chrome/browser/storage_monitor/removable_device_notifications_window_win.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

namespace {

// Gets the details of the MTP partition storage specified by the
// |storage_path| on the UI thread. If the storage details are valid, creates a
// MTP device delegate on the media task runner.
void GetStorageInfoAndMaybeCreateDelegate(
    const string16& storage_path,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    const CreateMTPDeviceDelegateCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!storage_path.empty());
  DCHECK(media_task_runner.get());
  string16 storage_device_id;
  RemoveChars(storage_path, L"\\\\", &storage_device_id);
  DCHECK(!storage_device_id.empty());
  // TODO(gbillock): Take the StorageMonitor as an argument.
  StorageMonitor* monitor = StorageMonitor::GetInstance();
  DCHECK(monitor);
  string16 pnp_device_id;
  string16 storage_object_id;
  if ((!monitor->GetMTPStorageInfoFromDeviceId(
          UTF16ToUTF8(storage_device_id), &pnp_device_id,
          &storage_object_id)) ||
       pnp_device_id.empty() ||
       storage_object_id.empty())
    return;
  media_task_runner->PostTask(FROM_HERE,
                              base::Bind(&OnGotStorageInfoCreateDelegate,
                                         storage_path,
                                         media_task_runner,
                                         callback,
                                         pnp_device_id,
                                         storage_object_id));
}

}  // namespace

// Used by GetStorageInfoAndMaybeCreateDelegate() to create the MTP device
// delegate on the media task runner.
void OnGotStorageInfoCreateDelegate(
    const string16& device_location,
    base::SequencedTaskRunner* media_task_runner,
    const CreateMTPDeviceDelegateCallback& callback,
    const string16& pnp_device_id,
    const string16& storage_object_id) {
  DCHECK(media_task_runner);
  DCHECK(media_task_runner->RunsTasksOnCurrentThread());
  callback.Run(new MTPDeviceDelegateImplWin(device_location,
                                            pnp_device_id,
                                            storage_object_id,
                                            media_task_runner));
}

void CreateMTPDeviceDelegate(const string16& device_location,
                             base::SequencedTaskRunner* media_task_runner,
                             const CreateMTPDeviceDelegateCallback& callback) {
  DCHECK(media_task_runner);
  DCHECK(media_task_runner->RunsTasksOnCurrentThread());
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&GetStorageInfoAndMaybeCreateDelegate,
                 device_location,
                 make_scoped_refptr(media_task_runner),
                 callback));
}

// MTPDeviceDelegateImplWin ---------------------------------------------------

MTPDeviceDelegateImplWin::MTPDeviceDelegateImplWin(
    const string16& fs_root_path,
    const string16& pnp_device_id,
    const string16& storage_object_id,
    base::SequencedTaskRunner* media_task_runner)
    : registered_device_path_(fs_root_path),
      pnp_device_id_(pnp_device_id),
      storage_object_id_(storage_object_id),
      media_task_runner_(media_task_runner),
      cancel_pending_tasks_(false) {
  DCHECK(!pnp_device_id_.empty());
  DCHECK(!storage_object_id_.empty());
  DCHECK(media_task_runner_.get());
}

MTPDeviceDelegateImplWin::~MTPDeviceDelegateImplWin() {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());
}

base::PlatformFileError MTPDeviceDelegateImplWin::GetFileInfo(
    const base::FilePath& file_path,
    base::PlatformFileInfo* file_info) {
  if (!LazyInit())
    return base::PLATFORM_FILE_ERROR_FAILED;
  string16 object_id = GetFileObjectIdFromPath(file_path.value());
  if (object_id.empty())
    return base::PLATFORM_FILE_ERROR_FAILED;
  return media_transfer_protocol::GetFileEntryInfo(device_.get(), object_id,
                                                   file_info);
}

scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator>
    MTPDeviceDelegateImplWin::CreateFileEnumerator(const base::FilePath& root,
                                                   bool recursive) {
  scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator>
      file_enumerator(new fileapi::FileSystemFileUtil::EmptyFileEnumerator());
  if (root.empty() || !LazyInit())
    return file_enumerator.Pass();

  string16 object_id = GetFileObjectIdFromPath(root.value());
  if (object_id.empty())
    return file_enumerator.Pass();

  MTPDeviceObjectEntries entries;
  if (!media_transfer_protocol::GetDirectoryEntries(device_.get(), object_id,
                                                    &entries) ||
      entries.empty())
    return file_enumerator.Pass();

  if (recursive) {
    file_enumerator.reset(
        new RecursiveMTPDeviceObjectEnumerator(device_.get(), entries));
  } else {
    file_enumerator.reset(new MTPDeviceObjectEnumerator(entries));
  }
  return file_enumerator.Pass();
}

base::PlatformFileError MTPDeviceDelegateImplWin::CreateSnapshotFile(
    const base::FilePath& device_file_path,
    const base::FilePath& local_path,
    base::PlatformFileInfo* file_info) {
  if (!LazyInit())
    return base::PLATFORM_FILE_ERROR_FAILED;
  string16 file_object_id = GetFileObjectIdFromPath(device_file_path.value());
  if (file_object_id.empty())
    return base::PLATFORM_FILE_ERROR_FAILED;
  base::PlatformFileError error = GetFileInfo(device_file_path, file_info);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (file_info->is_directory)
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  if (!media_transfer_protocol::WriteFileObjectContentToPath(device_.get(),
                                                             file_object_id,
                                                             local_path))
    return base::PLATFORM_FILE_ERROR_FAILED;

  // LocalFileStreamReader is used to read the contents of the snapshot file.
  // Snapshot file modification time does not match the last modified time
  // of the original media file. Therefore, set the last modified time to null
  // in order to avoid the verification in LocalFileStreamReader.
  //
  // Users will use HTML5 FileSystem Entry getMetadata() interface to get the
  // actual last modified time of the media file.
  file_info->last_modified = base::Time();
  return error;
}

void MTPDeviceDelegateImplWin::CancelPendingTasksAndDeleteDelegate() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  {
    base::AutoLock locked(cancel_tasks_lock_);
    cancel_pending_tasks_ = true;
  }
  media_task_runner_->DeleteSoon(FROM_HERE, this);
}

bool MTPDeviceDelegateImplWin::LazyInit() {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());
  // Both OpenDevice() (which we call below) and any operations our callers do
  // may take some time. Abort them immediately if we're in the process of
  // shutting down.
  {
    base::AutoLock locked(cancel_tasks_lock_);
    if (cancel_pending_tasks_)
      return false;
  }
  if (device_.get())
    return true;  // Already successfully initialized.
  device_ = media_transfer_protocol::OpenDevice(pnp_device_id_);
  return (device_.get() != NULL);
}

string16 MTPDeviceDelegateImplWin::GetFileObjectIdFromPath(
    const string16& file_path) {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!file_path.empty());
  if (registered_device_path_ == file_path)
    return storage_object_id_;

  string16 actual_file_path(file_path);
  ReplaceFirstSubstringAfterOffset(&actual_file_path, 0,
                                   registered_device_path_, string16());
  DCHECK(!actual_file_path.empty());
  typedef std::vector<string16> PathComponents;
  PathComponents path_components;
  base::SplitString(actual_file_path, L'\\', &path_components);
  DCHECK(!path_components.empty());
  string16 parent_id(storage_object_id_);
  string16 file_object_id;
  for (PathComponents::const_iterator path_iter = path_components.begin() + 1;
       path_iter != path_components.end(); ++path_iter) {
    file_object_id = media_transfer_protocol::GetObjectIdFromName(device_,
                                                                  parent_id,
                                                                  *path_iter);
    if (file_object_id.empty())
      break;
    parent_id = file_object_id;
  }
  return file_object_id;
}

}  // namespace chrome
