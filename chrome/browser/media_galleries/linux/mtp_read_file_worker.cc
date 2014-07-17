// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/linux/mtp_read_file_worker.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/media_galleries/linux/snapshot_file_details.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;
using storage_monitor::StorageMonitor;

namespace {

// Appends |data| to the snapshot file specified by the |snapshot_file_path| on
// the file thread.
// Returns the number of bytes written to the snapshot file. In case of failure,
// returns zero.
uint32 WriteDataChunkIntoSnapshotFileOnFileThread(
    const base::FilePath& snapshot_file_path,
    const std::string& data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
  int bytes_written =
      base::AppendToFile(snapshot_file_path, data.data(),
                         base::checked_cast<int>(data.size()));
  return (static_cast<int>(data.size()) == bytes_written) ?
      base::checked_cast<uint32>(bytes_written) : 0;
}

}  // namespace

MTPReadFileWorker::MTPReadFileWorker(const std::string& device_handle)
    : device_handle_(device_handle),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!device_handle_.empty());
}

MTPReadFileWorker::~MTPReadFileWorker() {
}

void MTPReadFileWorker::WriteDataIntoSnapshotFile(
    const SnapshotRequestInfo& request_info,
    const base::File::Info& snapshot_file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReadDataChunkFromDeviceFile(
      make_scoped_ptr(new SnapshotFileDetails(request_info,
                                              snapshot_file_info)));
}

void MTPReadFileWorker::ReadDataChunkFromDeviceFile(
    scoped_ptr<SnapshotFileDetails> snapshot_file_details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(snapshot_file_details.get());

  // To avoid calling |snapshot_file_details| methods and passing ownership of
  // |snapshot_file_details| in the same_line.
  SnapshotFileDetails* snapshot_file_details_ptr = snapshot_file_details.get();

  device::MediaTransferProtocolManager* mtp_device_manager =
      StorageMonitor::GetInstance()->media_transfer_protocol_manager();
  mtp_device_manager->ReadFileChunkById(
      device_handle_,
      snapshot_file_details_ptr->file_id(),
      snapshot_file_details_ptr->bytes_written(),
      snapshot_file_details_ptr->BytesToRead(),
      base::Bind(&MTPReadFileWorker::OnDidReadDataChunkFromDeviceFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&snapshot_file_details)));
}

void MTPReadFileWorker::OnDidReadDataChunkFromDeviceFile(
    scoped_ptr<SnapshotFileDetails> snapshot_file_details,
    const std::string& data,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(snapshot_file_details.get());
  snapshot_file_details->set_error_occurred(
      error || (data.size() != snapshot_file_details->BytesToRead()));
  if (snapshot_file_details->error_occurred()) {
    OnDidWriteIntoSnapshotFile(snapshot_file_details.Pass());
    return;
  }

  // To avoid calling |snapshot_file_details| methods and passing ownership of
  // |snapshot_file_details| in the same_line.
  SnapshotFileDetails* snapshot_file_details_ptr = snapshot_file_details.get();
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WriteDataChunkIntoSnapshotFileOnFileThread,
                 snapshot_file_details_ptr->snapshot_file_path(),
                 data),
      base::Bind(&MTPReadFileWorker::OnDidWriteDataChunkIntoSnapshotFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&snapshot_file_details)));
}

void MTPReadFileWorker::OnDidWriteDataChunkIntoSnapshotFile(
    scoped_ptr<SnapshotFileDetails> snapshot_file_details,
    uint32 bytes_written) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(snapshot_file_details.get());
  if (snapshot_file_details->AddBytesWritten(bytes_written)) {
    if (!snapshot_file_details->IsSnapshotFileWriteComplete()) {
      ReadDataChunkFromDeviceFile(snapshot_file_details.Pass());
      return;
    }
  } else {
    snapshot_file_details->set_error_occurred(true);
  }
  OnDidWriteIntoSnapshotFile(snapshot_file_details.Pass());
}

void MTPReadFileWorker::OnDidWriteIntoSnapshotFile(
    scoped_ptr<SnapshotFileDetails> snapshot_file_details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(snapshot_file_details.get());

  if (snapshot_file_details->error_occurred()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(snapshot_file_details->error_callback(),
                   base::File::FILE_ERROR_FAILED));
    return;
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(snapshot_file_details->success_callback(),
                 snapshot_file_details->file_info(),
                 snapshot_file_details->snapshot_file_path()));
}
