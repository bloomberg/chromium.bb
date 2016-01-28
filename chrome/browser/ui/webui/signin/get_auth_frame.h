// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_GET_AUTH_FRAME_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_GET_AUTH_FRAME_H_

#include <string>

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace signin {

// Gets a webview within an auth page that has the specified parent frame name
// (i.e. <webview name="foobar"></webview>).
content::RenderFrameHost* GetAuthFrame(content::WebContents* web_contents,
                                       const std::string& parent_frame_name);

content::WebContents* GetAuthFrameWebContents(
    content::WebContents* web_contents,
    const std::string& parent_frame_name);

}  // namespace signin

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_GET_AUTH_FRAME_H_
