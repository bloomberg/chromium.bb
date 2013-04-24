// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/scoped_mtp_device_map_entry.h"

#include "chrome/browser/media_galleries/fileapi/mtp_device_file_system_config.h"

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
#include "base/bind.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"
#include "chrome/browser/media_galleries/mtp_device_delegate_impl.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/fileapi/file_system_task_runners.h"

#endif

namespace chrome {

namespace {

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
bool IsMediaTaskRunnerThread() {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(fileapi::kMediaTaskRunnerName);
  return pool->IsRunningSequenceOnCurrentThread(media_sequence_token);
}

scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner() {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(fileapi::kMediaTaskRunnerName);
  return pool->GetSequencedTaskRunner(media_sequence_token);
}

void OnDeviceAsyncDelegateDestroyed(
    const base::FilePath::StringType& device_location) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  MTPDeviceMapService::GetInstance()->RemoveAsyncDelegate(
      device_location);
}

void RemoveDeviceDelegate(const base::FilePath::StringType& device_location) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&OnDeviceAsyncDelegateDestroyed, device_location));
}

#endif

}  // namespace

ScopedMTPDeviceMapEntry::ScopedMTPDeviceMapEntry(
    const base::FilePath::StringType& device_location,
    const base::Closure& on_destruction_callback)
    : device_location_(device_location),
      on_destruction_callback_(on_destruction_callback) {
}

void ScopedMTPDeviceMapEntry::Init() {
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  CreateMTPDeviceAsyncDelegateCallback callback =
      base::Bind(&ScopedMTPDeviceMapEntry::OnMTPDeviceAsyncDelegateCreated,
                 this);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CreateMTPDeviceAsyncDelegate,
                 device_location_,
                 callback));
#endif
}

ScopedMTPDeviceMapEntry::~ScopedMTPDeviceMapEntry() {
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  RemoveDeviceDelegate(device_location_);
  on_destruction_callback_.Run();
#endif
}

void ScopedMTPDeviceMapEntry::OnMTPDeviceAsyncDelegateCreated(
    MTPDeviceAsyncDelegate* delegate) {
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  MTPDeviceMapService::GetInstance()->AddAsyncDelegate(
      device_location_, delegate);
#endif
}

}  // namespace chrome
