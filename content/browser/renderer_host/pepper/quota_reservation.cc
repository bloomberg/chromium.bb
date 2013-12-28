// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/quota_reservation.h"

#include "base/bind.h"
#include "base/callback.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/quota/open_file_handle.h"
#include "webkit/browser/fileapi/quota/quota_reservation.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace content {

// static
scoped_refptr<QuotaReservation> QuotaReservation::Create(
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    const GURL& origin_url,
    fileapi::FileSystemType type) {
  return scoped_refptr<QuotaReservation>(new QuotaReservation(
      file_system_context, origin_url, type));
}

QuotaReservation::QuotaReservation(
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    const GURL& origin_url,
    fileapi::FileSystemType file_system_type)
    : file_system_context_(file_system_context) {
  quota_reservation_ =
      file_system_context->CreateQuotaReservationOnFileTaskRunner(
          origin_url,
          file_system_type);
}

// For unit testing only.
QuotaReservation::QuotaReservation(
    scoped_refptr<fileapi::QuotaReservation> quota_reservation,
    const GURL& /* origin_url */,
    fileapi::FileSystemType /* file_system_type */)
    : quota_reservation_(quota_reservation) {
}

QuotaReservation::~QuotaReservation() {
  // We should have no open files at this point.
  DCHECK(files_.size() == 0);
  for (FileMap::iterator it = files_.begin(); it != files_.end(); ++ it)
    delete it->second;
}

int64_t QuotaReservation::OpenFile(int32_t id,
                                   const base::FilePath& file_path) {
  scoped_ptr<fileapi::OpenFileHandle> file_handle =
      quota_reservation_->GetOpenFileHandle(file_path);
  std::pair<FileMap::iterator, bool> insert_result =
      files_.insert(std::make_pair(id, file_handle.get()));
  if (insert_result.second) {
    int64_t max_written_offset = file_handle->base_file_size();
    ignore_result(file_handle.release());
    return max_written_offset;
  }
  NOTREACHED();
  return 0;
}

void QuotaReservation::CloseFile(int32_t id,
                                 int64_t max_written_offset) {
  FileMap::iterator it = files_.find(id);
  if (it != files_.end()) {
    it->second->UpdateMaxWrittenOffset(max_written_offset);
    files_.erase(it);
  } else {
    NOTREACHED();
  }
}

void QuotaReservation::ReserveQuota(
    int64_t amount,
    const OffsetMap& max_written_offsets,
    const ReserveQuotaCallback& callback) {
  for (FileMap::iterator it = files_.begin(); it != files_.end(); ++ it) {
    OffsetMap::const_iterator offset_it = max_written_offsets.find(it->first);
    if (offset_it != max_written_offsets.end())
      it->second->UpdateMaxWrittenOffset(offset_it->second);
    else
      NOTREACHED();
  }

  quota_reservation_->RefreshReservation(
      amount,
      base::Bind(&QuotaReservation::GotReservedQuota,
                 this,
                 callback));
}

void QuotaReservation::GotReservedQuota(
    const ReserveQuotaCallback& callback,
    base::PlatformFileError error) {
  OffsetMap max_written_offsets;
  for (FileMap::iterator it = files_.begin(); it != files_.end(); ++ it) {
    max_written_offsets.insert(
        std::make_pair(it->first, it->second->base_file_size()));
  }

  if (file_system_context_) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback,
                   quota_reservation_->remaining_quota(),
                   max_written_offsets));
  } else {
    // Unit testing code path.
    callback.Run(quota_reservation_->remaining_quota(), max_written_offsets);
  }
}

void QuotaReservation::DeleteOnCorrectThread() const {
  if (file_system_context_ &&
      !file_system_context_->
          default_file_task_runner()->RunsTasksOnCurrentThread()) {
    file_system_context_->default_file_task_runner()->DeleteSoon(
        FROM_HERE,
        this);
  } else {
    // We're on the right thread to delete, or unit test.
    delete this;
  }
}

}  // namespace content
