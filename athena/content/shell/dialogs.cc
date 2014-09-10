// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/dialogs.h"
#include "base/logging.h"

namespace athena {

content::ColorChooser* OpenColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color,
    const std::vector<content::ColorSuggestion>& suggestions) {
  NOTIMPLEMENTED();
  return NULL;
}

void OpenFileChooser(content::WebContents* web_contents,
                     const content::FileChooserParams& params) {
  NOTIMPLEMENTED();
}

}  // namespace athena
