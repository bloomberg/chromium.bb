// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_PUBLIC_DIALOGS_H_
#define ATHENA_CONTENT_PUBLIC_DIALOGS_H_

#include <vector>

#include "third_party/skia/include/core/SkColor.h"

namespace content {
class ColorChooser;
class JavaScriptDialogManager;
class WebContents;
struct ColorSuggestion;
struct FileChooserParams;
}

namespace athena {

// Open Color chooser window.
content::ColorChooser* OpenColorChooser(
    content::WebContents* web_contents,
    SkColor color,
    const std::vector<content::ColorSuggestion>& suggestions);

// Open FileChooser window.
void OpenFileChooser(content::WebContents* web_contents,
                     const content::FileChooserParams& params);

}  // namespace athena

#endif  //  ATHENA_CONTENT_PUBLIC_DIALOGS_H_
