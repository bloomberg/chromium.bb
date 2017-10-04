// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_CONTENT_SOURCE_URL_RECORDER_H_
#define COMPONENTS_UKM_CONTENT_SOURCE_URL_RECORDER_H_

namespace content {
class WebContents;
}  // namespace content

namespace ukm {

// Initializes recording of UKM source URLs for the given WebContents.
void InitializeSourceUrlRecorderForWebContents(
    content::WebContents* web_contents);

}  // namespace ukm

#endif  // COMPONENTS_UKM_CONTENT_SOURCE_URL_RECORDER_H_
