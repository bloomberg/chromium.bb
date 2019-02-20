// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_COMMON_ANDROID_DOWNLOAD_COLLECTION_BRIDGE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_COMMON_ANDROID_DOWNLOAD_COLLECTION_BRIDGE_H_

#include <string>

#include "base/files/file.h"
#include "base/macros.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_file.h"

namespace download {

// Native class talking to the java side to publish a download to public
// download collection.
class COMPONENTS_DOWNLOAD_EXPORT DownloadCollectionBridge {
 public:
  // Creates the intermediate URI for download to write to.
  static base::FilePath CreateIntermediateUriForPublish(
      const GURL& original_url,
      const GURL& referrer_url,
      const base::FilePath& file_name,
      const std::string& mime_type);
  // Returns whether a download needs to be published.
  static bool ShouldPublishDownload(const base::FilePath& file_path);

  // Moves existing file content to the intermediate Uri, and remove
  // |source_path|.
  static DownloadInterruptReason MoveFileToIntermediateUri(
      const base::FilePath& source_path,
      const base::FilePath& destination_uri);

  // Deletes the intermediate Uri that is being written to.
  static void DeleteIntermediateUri(const base::FilePath& intermediate_uri);

  // Publishes the intermediate Uri to public download collection, and returns
  // the final Uri.
  static base::FilePath PublishDownload(const base::FilePath& intermediate_uri);

  // Opens the intermediate Uri for writing.
  static base::File OpenIntermediateUri(const base::FilePath& intermediate_uri);

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadCollectionBridge);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_COMMON_ANDROID_DOWNLOAD_COLLECTION_BRIDGE_H_
