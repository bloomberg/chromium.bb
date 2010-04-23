// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_APP_LAUNCHER_H_
#define CHROME_BROWSER_VIEWS_APP_LAUNCHER_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/views/info_bubble.h"
#include "views/view.h"

class Browser;
class SiteInstance;

namespace gfx {
class Size;
}
namespace views {
class NativeViewHost;
class View;
class WidgetGtk;
}

class InfoBubbleContentsView;
class NavigationBar;
class TabContentsDelegateImpl;

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
class AppLauncher : public InfoBubbleDelegate,
                    public TabContentsDelegate  {
 public:
  // Shows an application launcher bubble pointing to the |bounds| (which should
  // be in screen coordinates).
  // The caller DOES NOT OWN the AppLauncher returned.  It is deleted
  // automatically when the AppLauncher is closed.
  static AppLauncher* Show(Browser* browser, const gfx::Rect& bounds);

  // Shows an application launcher bubble pointing to the new tab button.
  // The caller DOES NOT OWN the AppLauncher returned.  It is deleted
  // automatically when the AppLauncher is closed.
  static AppLauncher* ShowForNewTab(Browser* browser);

  // Returns the browser this AppLauncher is associated with.
  Browser* browser() const { return browser_; }

  // Hides the app launcher.
  void Hide();

  // InfoBubbleDelegate overrides.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape() { return true; }

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

 private:
  friend class DeleteTask<AppLauncher>;
  friend class NavigationBar;
  friend class InfoBubbleContentsView;

  explicit AppLauncher(Browser* browser);
  ~AppLauncher();

  void AddTabWithURL(const GURL& url, PageTransition::Type transition);

  // The currently active browser. We use this to open urls.
  Browser* browser_;

  // The InfoBubble displaying the omnibox and app contents.
  InfoBubble* info_bubble_;

  // The view with the navigation bar and render view, shown in the info-bubble.
  InfoBubbleContentsView* info_bubble_content_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncher);
};

#endif  // CHROME_BROWSER_VIEWS_APP_LAUNCHER_H_
