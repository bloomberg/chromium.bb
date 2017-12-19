// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_VR_TRANSITION_UTILS_H_
#define CHROME_BROWSER_VR_TEST_VR_TRANSITION_UTILS_H_

#include "content/public/browser/web_contents.h"

namespace vr {

void EnterPresentation(content::WebContents* web_contents);
void EnterPresentationAndWait(content::WebContents* web_contents);
void EnterPresentationOrFail(content::WebContents* web_contents);

void ExitPresentation(content::WebContents* web_contents);
void ExitPresentationAndWait(content::WebContents* web_contents);
void ExitPresentationOrFail(content::WebContents* web_contents);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_VR_TRANSITION_UTILS_H_
