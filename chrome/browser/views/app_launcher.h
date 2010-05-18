// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_APP_LAUNCHER_H_
#define CHROME_BROWSER_VIEWS_APP_LAUNCHER_H_

#include <string>

#include "app/slide_animation.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/views/pinned_contents_info_bubble.h"

class Browser;
class InfoBubbleContentsView;

// AppLauncher manages showing the application launcher and optionally the
// navigation bar in compact navigation bar mode. The app launcher is
// currently an HTML page. When the user clicks a link on the page a
// new tab is added to the current browser and the app launcher is hidden.
// When the user opens a new page from the navigation bar, it opens a
// new tab on left, on right or clobbers the current tab depending on
// the configuration.
//
// To show the app launcher invoke Show.
//
// When a new url is opened, or the user clicks outsides the bounds of the
// widget the app launcher is closed.
class AppLauncher : public AnimationDelegate,
                    public InfoBubbleDelegate,
                    public TabContentsDelegate {
 public:
  // Shows an application launcher bubble pointing to the |bounds| (which should
  // be in screen coordinates). |bubble_anchor| specifies at which coordinates
  // the bubble contents should appear (in screen coordinates). The bubble will
  // be moved accordingly. Any |hash_params| are appended to the hash of the URL
  // that is opened in the launcher.
  //
  // The caller DOES NOT OWN the AppLauncher returned.  It is deleted
  // automatically when the AppLauncher is closed.
  static AppLauncher* Show(Browser* browser,
                           const gfx::Rect& bounds,
                           const gfx::Point& bubble_anchor,
                           const std::string& hash_params);

  // Shows an application launcher bubble pointing to the new tab button.
  // Any |hash_params| are appended to the hash of the URL that is opened in
  // the launcher.
  //
  // The caller DOES NOT OWN the AppLauncher returned. It is deleted
  // automatically when the AppLauncher is closed.
  static AppLauncher* ShowForNewTab(Browser* browser,
                                    const std::string& hash_params);

  // Returns the browser this AppLauncher is associated with.
  Browser* browser() const { return browser_; }

  // Returns any hash params for the page to open.
  const std::string& hash_params() const { return hash_params_; }

  // Hides the app launcher.
  void Hide();

  // AnimationDelegate overrides:
  virtual void AnimationProgressed(const Animation* animation);

  // InfoBubbleDelegate overrides.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape() { return true; }
  virtual bool FadeInOnShow() { return false; }

  // TabContentsDelegate.
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents) {}
  virtual void LoadingStateChanged(TabContents* source) {}
  virtual void CloseContents(TabContents* source) {}
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual bool IsPopup(TabContents* source) { return false; }
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  virtual void URLStarredChanged(TabContents* source, bool starred) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}
  virtual bool ShouldEnablePreferredSizeNotifications() { return true; }
  virtual void UpdatePreferredSize(const gfx::Size& pref_size);

 private:
  friend class DeleteTask<AppLauncher>;
  friend class InfoBubbleContentsView;

  explicit AppLauncher(Browser* browser);
  ~AppLauncher();

  void AddTabWithURL(const GURL& url, PageTransition::Type transition);

  // Resizes the bubble so it matches its contents's size |contents_size|.
  void Resize(const gfx::Size& contents_size);

  // The currently active browser. We use this to open urls.
  Browser* browser_;

  // The InfoBubble displaying the Omnibox and app contents.
  PinnedContentsInfoBubble* info_bubble_;

  // The view with the navigation bar and render view, shown in the info-bubble.
  InfoBubbleContentsView* info_bubble_content_;

  // An optional string containing querystring-encoded name/value pairs to be
  // sent into the launcher's HTML page via its hash.
  std::string hash_params_;

  // The preferred size of the DOM contents.
  gfx::Size contents_pref_size_;

  // The previous preferred size of the DOM contents.
  gfx::Size previous_contents_pref_size_;

  // Whether we should use an animation when showing the info-bubble.
  bool animate_;

  // The animation that grows the info-bubble.
  scoped_ptr<SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncher);
};

#endif  // CHROME_BROWSER_VIEWS_APP_LAUNCHER_H_
