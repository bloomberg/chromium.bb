// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MTP_DEVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MTP_DEVICE_DELEGATE_IMPL_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"

namespace base {
class SequencedTaskRunner;
}

typedef base::Callback<void(MTPDeviceAsyncDelegate*)>
    CreateMTPDeviceAsyncDelegateCallback;

void CreateMTPDeviceAsyncDelegate(
    const base::FilePath::StringType& device_location,
    const CreateMTPDeviceAsyncDelegateCallback& callback);

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MTP_DEVICE_DELEGATE_IMPL_H_
