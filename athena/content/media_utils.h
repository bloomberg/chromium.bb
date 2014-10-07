// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_MEDIA_UTILS_H_
#define ATHENA_CONTENT_MEDIA_UTILS_H_

#include "athena/activity/public/activity.h"

namespace content {
class WebContents;
}

namespace athena {

// Get the media state of a given web content.
Activity::ActivityMediaState GetActivityMediaState(
    content::WebContents* content);

}  // namespace athena

#endif  // ATHENA_CONTENT_MEDIA_UTILS_H_
