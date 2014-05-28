// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_DISTILLER_TAB_UTILS_H_
#define CHROME_BROWSER_DOM_DISTILLER_TAB_UTILS_H_

namespace content {
class WebContents;
}  // namespace content

// Creates a new WebContents and navigates it to view the URL of the current
// page, while in the background starts distilling the current page. This method
// takes ownership over the old WebContents after swapping in the new one.
void DistillCurrentPageAndView(content::WebContents* old_web_contents);

#endif  // CHROME_BROWSER_DOM_DISTILLER_TAB_UTILS_H_
