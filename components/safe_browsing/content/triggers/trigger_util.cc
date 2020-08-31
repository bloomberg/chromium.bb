// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some useful utilities for the safe_browsing/triggers
// classes

#include "components/safe_browsing/content/triggers/trigger_util.h"

#include <string>

#include "content/public/browser/render_frame_host.h"

namespace safe_browsing {

bool DetectGoogleAd(content::RenderFrameHost* render_frame_host,
                    const GURL& frame_url) {
  // In case the navigation aborted, look up the RFH by the Frame Tree Node
  // ID. It returns the committed frame host or the initial frame host for the
  // frame if no committed host exists. Using a previous host is fine because
  // once a frame has an ad we always consider it to have an ad.
  // We use the unsafe method of FindFrameByFrameTreeNodeId because we're not
  // concerned with which process the frame lives on (we only want to know if an
  // ad could be present on the page right now).
  if (render_frame_host) {
    const std::string& frame_name = render_frame_host->GetFrameName();
    if (base::StartsWith(frame_name, "google_ads_iframe",
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(frame_name, "google_ads_frame",
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }

  return frame_url.host_piece() == "tpc.googlesyndication.com" &&
         base::StartsWith(frame_url.path_piece(), "/safeframe",
                          base::CompareCase::SENSITIVE);
}

}  // namespace safe_browsing
