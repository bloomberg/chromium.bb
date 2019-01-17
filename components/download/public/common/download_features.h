// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FEATURES_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FEATURES_H_

#include "base/feature_list.h"
#include "components/download/public/common/download_export.h"

namespace download {
namespace features {

// Whether download auto-resumptions are enabled in native.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature
    kDownloadAutoResumptionNative;

// Whether a download can be handled by parallel jobs.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature kParallelDownloading;

// Whether metadata for new in-progress downloads will be be stored in download
// DB, rather than history DB.
COMPONENTS_DOWNLOAD_EXPORT extern const base::Feature
    kDownloadDBForNewDownloads;

}  // namespace features
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FEATURES_H_
