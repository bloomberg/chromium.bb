// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_OVERLAY_H_
#define CHROME_BROWSER_INSTANT_INSTANT_OVERLAY_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/instant/instant_page.h"

class Profile;

namespace content {
class WebContents;
}

// InstantOverlay is used to communicate with an overlay WebContents that it
// owns and loads the "Instant URL" into. This overlay can appear and disappear
// at will as the user types in the omnibox.
class InstantOverlay : public InstantPage,
                       public InstantLoader::Delegate {
 public:
  // Returns the InstantOverlay for |contents| if it's used for Instant.
  static InstantOverlay* FromWebContents(const content::WebContents* contents);

  InstantOverlay(InstantController* controller,
                 const std::string& instant_url);
  virtual ~InstantOverlay();

  // Creates a new WebContents and loads |instant_url_| into it. Uses
  // |active_tab|, if non-NULL, to initialize the size of the WebContents.
  void InitContents(Profile* profile,
                    const content::WebContents* active_tab);

  // Releases the overlay WebContents. This should be called when the overlay
  // is committed.
  scoped_ptr<content::WebContents> ReleaseContents() WARN_UNUSED_RESULT;

  // Returns the URL that we're loading.
  const std::string& instant_url() const { return instant_url_; }

  // Returns whether the underlying contents is stale (i.e. was loaded too long
  // ago).
  bool is_stale() const { return is_stale_; }

  // Returns true if the mouse or a touch pointer is down due to activating the
  // overlay contents.
  bool is_pointer_down_from_activate() const {
    return is_pointer_down_from_activate_;
  }

  // Returns info about the last navigation by the Instant page. If the page
  // hasn't navigated since the last Update(), the URL is empty.
  const history::HistoryAddPageArgs& last_navigation() const {
    return last_navigation_;
  }

  // Called by the history tab helper with information that it would have added
  // to the history service had this WebContents not been used for Instant.
  void DidNavigate(const history::HistoryAddPageArgs& add_page_args);

  // Returns true if the overlay is using
  // InstantController::kLocalOmniboxPopupURL as the |instant_url_|.
  bool IsUsingLocalOverlay() const;

  // Overridden from InstantPage:
  virtual void Update(const string16& text,
                      size_t selection_start,
                      size_t selection_end,
                      bool verbatim) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(InstantTest, InstantOverlayRefresh);

  // Helper to access delegate() as an InstantController object.
  InstantController* instant_controller() const {
    return static_cast<InstantController*>(delegate());
  }

  // Overridden from InstantPage:
  virtual bool ShouldProcessRenderViewCreated() OVERRIDE;
  virtual bool ShouldProcessRenderViewGone() OVERRIDE;
  virtual bool ShouldProcessAboutToNavigateMainFrame() OVERRIDE;
  virtual bool ShouldProcessSetSuggestions() OVERRIDE;
  virtual bool ShouldProcessShowInstantOverlay() OVERRIDE;
  virtual bool ShouldProcessNavigateToURL() OVERRIDE;

  // Overriden from InstantLoader::Delegate:
  virtual void OnSwappedContents() OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnMouseDown() OVERRIDE;
  virtual void OnMouseUp() OVERRIDE;
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;

  // Called when the underlying page becomes stale.
  void HandleStalePage();

  InstantLoader loader_;
  const std::string instant_url_;
  bool is_stale_;
  bool is_pointer_down_from_activate_;
  history::HistoryAddPageArgs last_navigation_;

  DISALLOW_COPY_AND_ASSIGN(InstantOverlay);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_OVERLAY_H_
