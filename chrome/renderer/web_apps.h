// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEB_APPS_H_
#define CHROME_RENDERER_WEB_APPS_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "ui/gfx/size.h"

namespace blink {
class WebFrame;
}

struct WebApplicationInfo;

namespace web_apps {

// Parses the icon's size attribute as defined in the HTML 5 spec. Returns true
// on success, false on errors. On success either all the sizes specified in
// the attribute are added to sizes, or is_any is set to true.
//
// You shouldn't have a need to invoke this directly, it's public for testing.
bool ParseIconSizes(const base::string16& text, std::vector<gfx::Size>* sizes,
                    bool* is_any);

// Parses |web_app| information out of the document in frame. Returns true on
// success, or false and |error| on failure. Note that the document may contain
// no web application information, in which case |web_app| is unchanged and the
// function returns true.
//
// Documents can also contain a link to a application 'definition'. In this case
// web_app will have manifest_url set and nothing else. The caller must fetch
// this URL and pass the result to ParseWebAppFromDefinitionFile() for further
// processing.
bool ParseWebAppFromWebDocument(blink::WebFrame* frame,
                                WebApplicationInfo* web_app,
                                base::string16* error);

}  // namespace web_apps

#endif  // CHROME_RENDERER_WEB_APPS_H_
