// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_SIGN_IN_DELEGATE_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_SIGN_IN_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_delegate.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/host_desktop.h"

class Browser;
class Profile;

// Delegate of the bookmark bubble to load the sign in page in a browser
// when the sign in link is clicked.
class BookmarkBubbleSignInDelegate : public BookmarkBubbleDelegate,
                                     public chrome::BrowserListObserver {
 public:
  explicit BookmarkBubbleSignInDelegate(Browser* browser);

 private:
  virtual ~BookmarkBubbleSignInDelegate();

  // BookmarkBubbleDelegate:
  virtual void OnSignInLinkClicked() OVERRIDE;

  // chrome::BrowserListObserver:
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;

  // Makes sure |browser_| points to a valid browser.
  void EnsureBrowser();

  // The browser in which the sign in page must be loaded.
  Browser* browser_;

  // The profile associated with |browser_|.
  Profile* profile_;

  // The host desktop of |browser_|.
  chrome::HostDesktopType desktop_type_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleSignInDelegate);
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_SIGN_IN_DELEGATE_H_
