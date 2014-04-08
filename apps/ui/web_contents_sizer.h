// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_UI_WEB_CONTENTS_SIZER_H_
#define APPS_UI_WEB_CONTENTS_SIZER_H_

namespace content {
class WebContents;
}

namespace gfx {
class Size;
}

namespace apps {
// A platform-agnostic function to resize a WebContents. The top-left corner of
// the WebContents does not move during the resizing.
void ResizeWebContents(content::WebContents* web_contents,
                       const gfx::Size& size);

}  // namespace apps

#endif  // APPS_UI_WEB_CONTENTS_SIZER_H_
