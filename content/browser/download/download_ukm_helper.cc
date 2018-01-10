// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_ukm_helper.h"

namespace content {

void DownloadUkmHelper::UpdateSourceURL(ukm::UkmRecorder* ukm_recorder,
                                        ukm::SourceId source_id,
                                        WebContents* web_contents) {
  if (ukm_recorder) {
    ukm_recorder->UpdateSourceURL(source_id,
                                  web_contents->GetLastCommittedURL());
  }
}

}  // namespace content
