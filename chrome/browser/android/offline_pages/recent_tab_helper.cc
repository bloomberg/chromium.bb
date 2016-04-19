// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/recent_tab_helper.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(offline_pages::RecentTabHelper);

namespace offline_pages {

RecentTabHelper::RecentTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

RecentTabHelper::~RecentTabHelper() {
}

}  // namespace offline_pages
