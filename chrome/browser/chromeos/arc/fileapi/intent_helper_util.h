// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_INTENT_HELPER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_INTENT_HELPER_UTIL_H_

#include "components/arc/common/intent_helper.mojom.h"
#include "url/gurl.h"

namespace arc {
namespace intent_helper_util {

using GetFileSizeCallback =
    mojom::IntentHelperInstance::GetFileSizeDeprecatedCallback;
using OpenFileToReadCallback =
    mojom::IntentHelperInstance::OpenFileToReadDeprecatedCallback;

// Utility which posts a task to run GetFileSize.
// This function must be called on the IO thread.
void GetFileSizeOnIOThread(const GURL& arc_url,
                           const GetFileSizeCallback& callback);

// Utility which posts a task to run OpenFileToRead.
// This function must be called on the IO thread.
void OpenFileToReadOnIOThread(const GURL& arc_url,
                              const OpenFileToReadCallback& callback);

}  // namespace intent_helper_util
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_INTENT_HELPER_UTIL_H_
