// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_NTP_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_NTP_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/search/instant_loader.h"
#include "chrome/browser/ui/search/instant_page.h"

class Profile;

// InstantNTP is used to preload an Instant page that will be swapped in when a
// user navigates to a New Tab Page (NTP). The InstantNTP contents are never
// shown in an un-committed state.
class InstantNTP : public InstantPage,
                   public InstantLoader::Delegate {
 public:
  InstantNTP(InstantPage::Delegate* delegate, const std::string& instant_url);
  virtual ~InstantNTP();

  // Creates a new WebContents and loads |instant_url_| into it. Uses
  // |active_tab|, if non-NULL, to initialize the size of the WebContents.
  // |on_stale_callback| will be called when |loader_| determines the page to
  // be stale.
  void InitContents(Profile* profile,
                    const content::WebContents* active_tab,
                    const base::Closure& on_stale_callback);

  // Releases the WebContents for the Instant page.  This should be called when
  // the page is about to be committed.
  scoped_ptr<content::WebContents> ReleaseContents();

 private:
  // Overriden from InstantLoader::Delegate:
  virtual void OnSwappedContents() OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnMouseDown() OVERRIDE;
  virtual void OnMouseUp() OVERRIDE;
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void LoadCompletedMainFrame() OVERRIDE;

  // Overridden from InstantPage:
  virtual bool ShouldProcessRenderViewCreated() OVERRIDE;
  virtual bool ShouldProcessRenderViewGone() OVERRIDE;

  InstantLoader loader_;

  DISALLOW_COPY_AND_ASSIGN(InstantNTP);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_NTP_H_
