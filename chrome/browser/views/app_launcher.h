// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_APP_LAUNCHER_H_
#define CHROME_BROWSER_VIEWS_APP_LAUNCHER_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/common/renderer_preferences.h"
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
// AppLauncher creates a RenderViewHost and corresponding RenderWidgetHostView
// to display the html page. AppLauncher acts as the RenderViewHostDelegate for
// the RenderViewHost. Clicking on a link results in creating a new
// TabContents (assigned to pending_contents_). One of two things can then
// happen:
// . If the page is a popup (ShowCreatedWindow passed NEW_POPUP), the
//   TabContents is added to the Browser.
// . If the page requests a URL to be open (OpenURLFromTab), OpenURL is
//   invoked on the browser.
//
// When a new url is opened, or the user clicks outsides the bounds of the
// widget the app launcher is closed.
class AppLauncher : public InfoBubbleDelegate,
                    public RenderViewHostDelegate,
                    public RenderViewHostDelegate::View {
 public:
  // Shows an application launcher bubble pointing to the new tab button.
  // The caller DOES NOT OWN the AppLauncher returned.  It is deleted
  // automatically when the AppLauncher is closed.
  static AppLauncher* Show(Browser* browser);

  // Returns the browser this AppLauncher is associated with.
  Browser* browser() const { return browser_; }

  // Hides the app launcher.
  void Hide();

  // InfoBubbleDelegate overrides.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape() { return true; }

  // RenderViewHostDelegate overrides.
  virtual int GetBrowserWindowID() const {
    return -1;
  }
  virtual ViewType::Type GetRenderViewType() const {
    return ViewType::INVALID;
  }
  virtual RenderViewHostDelegate::View* GetViewDelegate() {
    return this;
  }
  virtual void RequestMove(const gfx::Rect& new_bounds);
  virtual RendererPreferences GetRendererPrefs(Profile* profile) const;

  // RenderViewHostDelegate::View overrides.
  virtual void CreateNewWindow(int route_id);
  virtual void CreateNewWidget(int route_id, bool activatable) {}
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture);
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) {}
  virtual void ShowContextMenu(const ContextMenuParams& params) {}
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops,
                             const SkBitmap& image,
                             const gfx::Point& image_offset);
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) {}
  virtual void GotFocus() {}
  virtual void TakeFocus(bool reverse) {}
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) {
    return false;
  }
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}
  virtual void HandleMouseEvent() {}
  virtual void HandleMouseLeave() {}
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) {}

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

  // SiteInstance for the RenderViewHosts we create.
  scoped_refptr<SiteInstance> site_instance_;

  // RenderViewHost for the contents.
  RenderViewHost* contents_rvh_;

  // RenderWidgetHostView from the contents_rvh_.
  RenderWidgetHostView* rwhv_;

  // Handles creating the child TabContents.
  RenderViewHostDelegateViewHelper helper_;

  // Delegate of the TabContents created by helper_.
  scoped_ptr<TabContentsDelegateImpl> tab_contents_delegate_;

  // TabContents created when the user clicks a link.
  scoped_ptr<TabContents> pending_contents_;

  // The view with the navigation bar and render view, shown in the info-bubble.
  InfoBubbleContentsView* info_bubble_content_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncher);
};

#endif  // CHROME_BROWSER_VIEWS_APP_LAUNCHER_H_
