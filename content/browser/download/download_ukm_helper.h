// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Holds helpers for gathering UKM stats about downloads.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UKM_HELPER_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UKM_HELPER_H_

#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace content {

class CONTENT_EXPORT DownloadUkmHelper {
 public:
  // Friended Helper for recording main frame URLs to UKM.
  static void UpdateSourceURL(ukm::UkmRecorder* ukm_recorder,
                              ukm::SourceId source_id,
                              WebContents* web_contents);

 private:
  DownloadUkmHelper();
  ~DownloadUkmHelper();

  DISALLOW_COPY_AND_ASSIGN(DownloadUkmHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UKM_HELPER_H_
