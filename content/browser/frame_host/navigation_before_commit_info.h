// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_BEFORE_COMMIT_INFO_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_BEFORE_COMMIT_INFO_H_

#include <string>

#include "base/basictypes.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "url/gurl.h"

namespace content {

// PlzNavigate:
// Holds the parameters determined during the load of a navigation request until
// the response is received. Initialized on the IO thread, then passed to the UI
// thread where it should be used to send a FrameMsg_CommitNavigation message to
// the renderer.
struct NavigationBeforeCommitInfo {
  NavigationBeforeCommitInfo() {};

  // The url that is actually being loaded.
  GURL navigation_url;

  // A unique blob url to be subsequently requested by the renderer as the main
  // resource for the frame, that will be used to find the associated stream of
  // data.
  GURL stream_url;

  // TODO(clamy): Maybe add the redirect chain if needed by the renderer.

  // The navigationStart time to expose to JS for this navigation.
  // TODO(clamy): Add the other values that matter to the Navigation Timing API.
  base::TimeTicks browser_navigation_start;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_BEFORE_COMMIT_INFO_H_
