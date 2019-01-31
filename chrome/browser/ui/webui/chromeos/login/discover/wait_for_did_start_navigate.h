// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_WAIT_FOR_DID_START_NAVIGATE_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_WAIT_FOR_DID_START_NAVIGATE_H_

#include "base/macros.h"
#include "base/run_loop.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace chromeos {
namespace test {

class WaitForDidStartNavigate : public content::WebContentsObserver {
 public:
  WaitForDidStartNavigate(content::WebContents* web_contents, const GURL& url);
  ~WaitForDidStartNavigate() override = default;

  // Blocks until DidStartNavigate with expected URL is called.
  void Wait();

 private:
  // content::WebContentsObserver
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  GURL url_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WaitForDidStartNavigate);
};

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_WAIT_FOR_DID_START_NAVIGATE_H_
