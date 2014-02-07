// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_ORIGIN_CHIP_H_
#define CHROME_BROWSER_UI_TOOLBAR_ORIGIN_CHIP_H_

#include "base/strings/string16.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

// This class provides functions that are shared across all platforms.
class OriginChip {
 public:
  // Detects client-side or SB malware/phishing hits. Used to decide whether the
  // origin chip should indicate that the current page has malware or is a known
  // phishing page.
  static bool IsMalware(const GURL& url, content::WebContents* tab);

  // Gets the label to display on the Origin Chip for the specified URL. The
  // |profile| is needed in case the URL is from an extension.
  static base::string16 LabelFromURLForProfile(const GURL& provided_url,
                                               Profile* profile);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_ORIGIN_CHIP_H_
