// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEB_CONTENTS_SET_BACKGROUND_COLOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEB_CONTENTS_SET_BACKGROUND_COLOR_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Defined in SkColor.h (32-bit ARGB color).
using SkColor = unsigned int;

namespace chromeos {

// Ensures that the background color of a given WebContents instance is always
// set to a given color value.
class WebContentsSetBackgroundColor
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebContentsSetBackgroundColor> {
 public:
  static void CreateForWebContentsWithColor(content::WebContents* web_contents,
                                            SkColor color);

  ~WebContentsSetBackgroundColor() override;

 private:
  WebContentsSetBackgroundColor(content::WebContents* web_contents,
                                SkColor color);

  // content::WebContentsObserver:
  void RenderViewReady() override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsSetBackgroundColor);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEB_CONTENTS_SET_BACKGROUND_COLOR_H_
