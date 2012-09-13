// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_DELEGATE_IMPL_H_

#include "build/build_config.h"
#include "webkit/fileapi/media/media_file_system_config.h"

#if !defined(SUPPORT_MEDIA_FILESYSTEM)
#error Media file system is not supported for this platform.
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/media_gallery/media_device_delegate_impl_chromeos.h"
#endif // OS_CHROMEOS

namespace chrome {

// TODO(kmadhusu): Implement mtp device delegates on other platforms.
#if defined(OS_CHROMEOS)
typedef class chromeos::MediaDeviceDelegateImplCros MediaDeviceDelegateImpl;
#endif  // OS_CHROMEOS

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_DELEGATE_IMPL_H_
