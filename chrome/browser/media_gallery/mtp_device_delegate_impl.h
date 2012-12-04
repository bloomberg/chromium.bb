// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MTP_DEVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MTP_DEVICE_DELEGATE_IMPL_H_

#include "build/build_config.h"
#include "webkit/fileapi/media/mtp_device_file_system_config.h"

#if !defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
#error "Media file system is not supported for this platform."
#endif

#if defined(OS_LINUX)  // Implies OS_CHROMEOS
#include "chrome/browser/media_gallery/linux/mtp_device_delegate_impl_linux.h"
#endif

namespace chrome {

// TODO(kmadhusu): Implement MTP device delegates on other platforms.
#if defined(OS_LINUX)  // Implies OS_CHROMEOS
typedef class MTPDeviceDelegateImplLinux MTPDeviceDelegateImpl;
#endif

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MTP_DEVICE_DELEGATE_IMPL_H_
