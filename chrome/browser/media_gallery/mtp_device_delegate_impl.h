// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MTP_DEVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MTP_DEVICE_DELEGATE_IMPL_H_

#include "webkit/fileapi/media/mtp_device_file_system_config.h"

#if !defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
#error "Media file system is not supported for this platform."
#endif

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "webkit/fileapi/media/mtp_device_async_delegate.h"
#include "webkit/fileapi/media/mtp_device_delegate.h"

namespace base {
class SequencedTaskRunner;
}

namespace chrome {

// TODO(kmadhusu): Remove CreateMTPDeviceDelegateCallback after fixing
// crbug.com/154835.
typedef base::Callback<void(fileapi::MTPDeviceDelegate*)>
    CreateMTPDeviceDelegateCallback;

typedef base::Callback<void(fileapi::MTPDeviceAsyncDelegate*)>
    CreateMTPDeviceAsyncDelegateCallback;

// TODO(kmadhusu): Remove CreateMTPDeviceDelegate() after fixing
// crbug.com/154835.
void CreateMTPDeviceDelegate(const base::FilePath::StringType& device_location,
                             base::SequencedTaskRunner* media_task_runner,
                             const CreateMTPDeviceDelegateCallback& callback);

void CreateMTPDeviceAsyncDelegate(
    const base::FilePath::StringType& device_location,
    const CreateMTPDeviceAsyncDelegateCallback& callback);

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MTP_DEVICE_DELEGATE_IMPL_H_
