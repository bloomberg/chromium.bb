// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_H_
#pragma once

namespace content {
class WebContents;
class WebContentsViewMacDelegate;
}

namespace browser {

content::WebContentsViewDelegate* CreateWebContentsViewDelegate(
    content::WebContents* web_contents);

}  // namespace

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_H_
