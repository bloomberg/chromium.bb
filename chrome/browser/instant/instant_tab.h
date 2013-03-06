// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_TAB_H_
#define CHROME_BROWSER_INSTANT_INSTANT_TAB_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/instant/instant_page.h"

// InstantTab represents a committed page (i.e. an actual tab on the tab strip)
// that supports the Instant API.
class InstantTab : public InstantPage {
 public:
  explicit InstantTab(InstantPage::Delegate* delegate);
  virtual ~InstantTab();

  // Start observing |contents| for messages. Sends a message to determine if
  // the page supports the Instant API.
  void Init(content::WebContents* contents);

 private:
  // Overridden from InstantPage:
  virtual bool ShouldProcessSetSuggestions() OVERRIDE;
  virtual bool ShouldProcessFocusOmnibox() OVERRIDE;
  virtual bool ShouldProcessStartCapturingKeyStrokes() OVERRIDE;
  virtual bool ShouldProcessStopCapturingKeyStrokes() OVERRIDE;
  virtual bool ShouldProcessNavigateToURL() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(InstantTab);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_TAB_H_
