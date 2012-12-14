// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_TAB_H_
#define CHROME_BROWSER_INSTANT_INSTANT_TAB_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/instant/instant_client.h"

struct InstantAutocompleteResult;
class InstantController;

namespace content {
class WebContents;
}

// InstantTab is used to communicate with a committed search results page, i.e.,
// an actual tab on the tab strip (compare: InstantLoader, which has a preview
// page that appears and disappears as the user types in the omnibox).
class InstantTab : public InstantClient::Delegate {
 public:
  // Doesn't take ownership of either |controller| or |contents|.
  InstantTab(InstantController* controller, content::WebContents* contents);
  virtual ~InstantTab();

  content::WebContents* contents() const { return contents_; }

  // Start observing |contents_| for messages. Sends a message to determine if
  // the page supports the Instant API.
  void Init();

  // Calls through to methods of the same name on InstantClient.
  void Update(const string16& text,
              size_t selection_start,
              size_t selection_end,
              bool verbatim);
  void Submit(const string16& text);
  void SendAutocompleteResults(
      const std::vector<InstantAutocompleteResult>& results);
  void SetDisplayInstantResults(bool display_instant_results);
  void UpOrDownKeyPressed(int count);
  void SetMarginSize(int start, int end);

 private:
  // Overridden from InstantClient::Delegate:
  virtual void SetSuggestions(
      const std::vector<InstantSuggestion>& suggestions) OVERRIDE;
  virtual void InstantSupportDetermined(bool supports_instant) OVERRIDE;
  virtual void ShowInstantPreview(InstantShownReason reason,
                                  int height,
                                  InstantSizeUnits units) OVERRIDE;
  virtual void StartCapturingKeyStrokes() OVERRIDE;
  virtual void StopCapturingKeyStrokes() OVERRIDE;
  virtual void RenderViewGone() OVERRIDE;
  virtual void AboutToNavigateMainFrame(const GURL& url) OVERRIDE;
  virtual void NavigateToURL(const GURL& url,
                             content::PageTransition transition) OVERRIDE;

  InstantClient client_;
  InstantController* const controller_;
  content::WebContents* const contents_;
  bool supports_instant_;

  DISALLOW_COPY_AND_ASSIGN(InstantTab);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_TAB_H_
