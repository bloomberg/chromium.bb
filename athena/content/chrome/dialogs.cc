// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/dialogs.h"

#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/ui/browser_dialogs.h"

namespace athena {

content::ColorChooser* OpenColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color,
    const std::vector<content::ColorSuggestion>& suggestions) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void OpenFileChooser(content::WebContents* web_contents,
                     const content::FileChooserParams& params) {
  return FileSelectHelper::RunFileChooser(web_contents, params);
}

}  // namespace athena
