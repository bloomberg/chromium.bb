// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_PAGE_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_PAGE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/page_transition_types.h"

class GURL;
class Profile;

namespace content {
struct FrameNavigateParams;
struct LoadCommittedDetails;
class WebContents;
}

namespace gfx {
class Rect;
}

// InstantPage is used to exchange messages with a page that implements the
// Instant/Embedded Search API (http://dev.chromium.org/embeddedsearch).
// InstantPage is not used directly but via one of its derived classes,
// InstantNTP and InstantTab.
class InstantPage : public content::WebContentsObserver,
                    public SearchModelObserver {
 public:
  // InstantPage calls its delegate in response to messages received from the
  // page. Each method is called with the |contents| corresponding to the page
  // we are observing.
  class Delegate {
   public:
    // Called upon determination of Instant API support. Either in response to
    // the page loading or because we received some other message.
    virtual void InstantSupportDetermined(const content::WebContents* contents,
                                          bool supports_instant) = 0;

    // Called when the page is about to navigate to |url|.
    virtual void InstantPageAboutToNavigateMainFrame(
        const content::WebContents* contents,
        const GURL& url) = 0;

   protected:
    virtual ~Delegate();
  };

  virtual ~InstantPage();

  // The WebContents corresponding to the page we're talking to. May be NULL.
  content::WebContents* contents() const { return web_contents(); }

  // Returns the Instant URL that was loaded for this page. Returns the empty
  // string if no URL was explicitly loaded as is the case for InstantTab.
  virtual const std::string& instant_url() const;

  // Returns true if the page is known to support the Instant API. This starts
  // out false, and is set to true whenever we get any message from the page.
  // Once true, it never becomes false (the page isn't expected to drop API
  // support suddenly).
  virtual bool supports_instant() const;

  // Returns true if the page is the local NTP (i.e. its URL is
  // chrome::kChromeSearchLocalNTPURL).
  virtual bool IsLocal() const;

 protected:
  InstantPage(Delegate* delegate, const std::string& instant_url,
              Profile* profile, bool is_incognito);

  // Sets |web_contents| as the page to communicate with. |web_contents| may be
  // NULL, which effectively stops all communication.
  void SetContents(content::WebContents* web_contents);

  Delegate* delegate() const { return delegate_; }

  Profile* profile() const { return profile_; }

  // These functions are called before processing messages received from the
  // page. By default, all messages are handled, but any derived classes may
  // choose to ignore some or all of the received messages by overriding these
  // methods.
  virtual bool ShouldProcessAboutToNavigateMainFrame();

 private:
  FRIEND_TEST_ALL_PREFIXES(InstantPageTest, IsLocal);
  FRIEND_TEST_ALL_PREFIXES(InstantPageTest,
                           DetermineIfPageSupportsInstant_Local);
  FRIEND_TEST_ALL_PREFIXES(InstantPageTest,
                           DetermineIfPageSupportsInstant_NonLocal);
  FRIEND_TEST_ALL_PREFIXES(InstantPageTest,
                           PageURLDoesntBelongToInstantRenderer);
  FRIEND_TEST_ALL_PREFIXES(InstantPageTest, PageSupportsInstant);

  // Overridden from content::WebContentsObserver:
  virtual void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      content::PageTransition transition_type) OVERRIDE;

  // Overridden from SearchModelObserver:
  virtual void ModelChanged(const SearchModel::State& old_state,
                            const SearchModel::State& new_state) OVERRIDE;

  // Update the status of Instant support.
  void InstantSupportDetermined(bool supports_instant);

  void ClearContents();

  // TODO(kmadhusu): Remove |profile_| from here and update InstantNTP to get
  // |profile| from InstantNTPPrerenderer.
  Profile* profile_;

  Delegate* const delegate_;
  const std::string instant_url_;
  const bool is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(InstantPage);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_PAGE_H_
