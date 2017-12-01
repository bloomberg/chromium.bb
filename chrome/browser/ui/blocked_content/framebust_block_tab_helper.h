// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_FRAMEBUST_BLOCK_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_FRAMEBUST_BLOCK_TAB_HELPER_H_

#include <vector>

#include "base/macros.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

// A tab helper that keeps track of blocked Framebusts that happened on each
// page. Only used for the desktop version of the blocked Framebust UI.
class FramebustBlockTabHelper
    : public content::WebContentsUserData<FramebustBlockTabHelper> {
 public:
  // There is expected to be at most one observer at a time.
  class Observer {
   public:
    // Called whenever a new URL was blocked.
    virtual void OnBlockedUrlAdded(const GURL& blocked_url) = 0;

   protected:
    virtual ~Observer() = default;
  };

  ~FramebustBlockTabHelper() override;

  // Shows the blocked Framebust icon in the Omnibox for the |blocked_url|.
  // If the icon is already visible, that URL is instead added to the vector of
  // currently blocked URLs and the bubble view is updated.
  void AddBlockedUrl(const GURL& blocked_url);

  // Returns true if at least one Framebust was blocked on this page.
  bool HasBlockedUrls() const;

  // Handles navigating to the blocked URL specified by |index| and clearing the
  // vector of blocked URLs.
  void OnBlockedUrlClicked(size_t index);

  // Sets and clears the current observer.
  void SetObserver(Observer* observer);
  void ClearObserver();

  // Returns all of the currently blocked URLs.
  const std::vector<GURL>& blocked_urls() const { return blocked_urls_; }

  // Remembers if the animation has run.
  void set_animation_has_run() { animation_has_run_ = true; }
  bool animation_has_run() const { return animation_has_run_; }

 private:
  friend class content::WebContentsUserData<FramebustBlockTabHelper>;

  explicit FramebustBlockTabHelper(content::WebContents* web_contents);

  content::WebContents* web_contents_;

  // Remembers all the currently blocked URLs. This is cleared on each
  // navigation.
  std::vector<GURL> blocked_urls_;

  // Remembers if the animation has run.
  bool animation_has_run_ = false;

  // Points to the unique observer of this class.
  Observer* observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FramebustBlockTabHelper);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_FRAMEBUST_BLOCK_TAB_HELPER_H_
