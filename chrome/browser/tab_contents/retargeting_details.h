// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RETARGETING_DETAILS_H_
#define CHROME_BROWSER_TAB_CONTENTS_RETARGETING_DETAILS_H_

#include "url/gurl.h"

namespace content {
class WebContents;
}

// Details sent for NOTIFICATION_RETARGETING.
struct RetargetingDetails {
  // The source tab contents.
  content::WebContents* source_web_contents;

  // The routing id of the source render frame from which the retargeting was
  // triggered.
  int64 source_render_frame_id;

  // The target URL.
  GURL target_url;

  // The target tab contents.
  content::WebContents* target_web_contents;

  // True if the target_web_contents is not yet inserted into a tab strip.
  bool not_yet_in_tabstrip;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RETARGETING_DETAILS_H_
