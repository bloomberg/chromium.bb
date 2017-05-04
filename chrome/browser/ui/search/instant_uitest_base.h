// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_UITEST_BASE_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_UITEST_BASE_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/search/instant_test_base.h"

class OmniboxView;

namespace content {
class RenderViewHost;
}  // namespace content

// This utility class is meant to be used in a "mix-in" fashion, giving the
// derived test class additional Instant-related UI test functionality.
class InstantUITestBase : public InstantTestBase {
 protected:
  InstantUITestBase();
  ~InstantUITestBase() override;

 protected:
  OmniboxView* omnibox();

  void FocusOmnibox();

  void SetOmniboxText(const std::string& text);

  void PressEnterAndWaitForNavigation();
  void PressEnterAndWaitForFrameLoad();

  std::string GetOmniboxText();

  // Loads a named image from url |image| from the given |rvh| host.  |loaded|
  // returns whether the image was able to load without error.
  // The method returns true if the JavaScript executed cleanly.
  bool LoadImage(content::RenderViewHost* rvh,
                 const std::string& image,
                 bool* loaded);

 private:
  DISALLOW_COPY_AND_ASSIGN(InstantUITestBase);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_UITEST_BASE_H_
