// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_SINGLE_TAB_MODE_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_SINGLE_TAB_MODE_TAB_HELPER_H_

#include <stdint.h>

#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/blocked_content/blocked_window_params.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class RenderViewHost;
class WebContents;
}  // namespace content

// Registers and unregisters the IDs of renderers in single tab mode, which
// are disallowed from opening new windows via
// ChromeContentBrowserClient::CanCreateWindow().
class SingleTabModeTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SingleTabModeTabHelper> {
 public:
  // Checks if the ID pair is blocked from creating new windows. IO-thread only.
  static bool IsRegistered(int32_t process_id, int32_t routing_id);

  ~SingleTabModeTabHelper() override;

  // Handles opening the given URL through the TabModel.
  void HandleOpenUrl(const BlockedWindowParams& params);

  // Permanently block this WebContents from creating new windows -- there is no
  // current need to allow toggling this flag on or off.
  void PermanentlyBlockAllNewWindows();

  // content::WebContentsObserver
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderViewDeleted(content::RenderViewHost* render_view_host) override;

 private:
  explicit SingleTabModeTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SingleTabModeTabHelper>;

  bool block_all_new_windows_;

  DISALLOW_COPY_AND_ASSIGN(SingleTabModeTabHelper);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_SINGLE_TAB_MODE_TAB_HELPER_H_
