// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_tracker.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PopupTracker);

PopupTracker::~PopupTracker() = default;

PopupTracker::PopupTracker(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}
