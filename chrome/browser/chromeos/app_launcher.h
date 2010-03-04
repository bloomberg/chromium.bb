// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_APP_LAUNCHER_H_

#include <gtk/gtk.h>

#include "app/active_window_watcher_x.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/renderer_preferences.h"
#include "views/view.h"

class Browser;
class RenderWidgetHostViewGtk;
class SiteInstance;

namespace gfx {
class Size;
}
namespace views {
class NativeViewHost;
class View;
class WidgetGtk;
}

namespace chromeos {

class NavigationBar;

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
class AppLauncher : public RenderViewHostDelegate,
                    public RenderViewHostDelegate::View,
                    public ActiveWindowWatcherX::Observer,
                    public views::AcceleratorTarget {
 public:
  AppLauncher();
  ~AppLauncher();

  // Shows the menu.
  void Show(Browser* browser);

 private:
  // TabContentsDelegate and RenderViewHostDelegate::View have some methods
  // in common (with differing signatures). The TabContentsDelegate methods are
  // implemented by this class.
  class TabContentsDelegateImpl : public TabContentsDelegate {
   public:
    explicit TabContentsDelegateImpl(AppLauncher* app_launcher);

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
                                bool user_gesture) {}
    virtual void ActivateContents(TabContents* contents) {}
    virtual void LoadingStateChanged(TabContents* source) {}
    virtual void CloseContents(TabContents* source) {}
    virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
    virtual bool IsPopup(TabContents* source) { return false; }
    virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
    virtual void URLStarredChanged(TabContents* source, bool starred) {}
    virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}

   private:
    AppLauncher* app_launcher_;

    DISALLOW_COPY_AND_ASSIGN(TabContentsDelegateImpl);
  };

  class TopContainer : public views::View {
   public:
    explicit TopContainer(AppLauncher* app_launcher);
    virtual ~TopContainer() {}

    // views::View overrides.
    virtual void Layout();
    virtual bool OnMousePressed(const views::MouseEvent& event);

   private:
    AppLauncher* app_launcher_;

    DISALLOW_COPY_AND_ASSIGN(TopContainer);
  };

  class BubbleContainer : public views::View {
   public:
    explicit BubbleContainer(AppLauncher* app_launcher);

    // views::View overrides.
    virtual void Layout();

   private:
    AppLauncher* app_launcher_;

    DISALLOW_COPY_AND_ASSIGN(BubbleContainer);
  };

  friend class BubbleContainer;
  friend class NavigationBar;
  friend class TabContentsDelegateImpl;
  friend class TopContainer;

  // Hides the app launcher.
  void Hide();

  // Cleans up state. This is invoked before showing and after a delay when
  // hidden.
  void Cleanup();

  // Updates the app launcher's state and layout with the |browser|.
  void Update(Browser* browser);

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
                             WebKit::WebDragOperationsMask allowed_ops);
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

  // ActiveWindowWatcherX::Observer implementation.
  virtual void ActiveWindowChanged(GdkWindow* active_window);

  // views::AcceleratorTarget implementation:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  void AddTabWithURL(const GURL& url, PageTransition::Type transition);

  // The currently active browser. We use this to open urls.
  Browser* browser_;

  // The widget displaying the rwvh_.
  views::WidgetGtk* popup_;

  // SiteInstance for the RenderViewHosts we create.
  SiteInstance* site_instance_;

  // RenderViewHost for the contents.
  RenderViewHost* contents_rvh_;

  // RenderWidgetHostView from the contents_rvh_.
  RenderWidgetHostViewGtk* rwhv_;

  // Handles creating the child TabContents.
  RenderViewHostDelegateViewHelper helper_;

  // Delegate of the TabContents created by helper_.
  TabContentsDelegateImpl tab_contents_delegate_;

  // TabContents created when the user clicks a link.
  scoped_ptr<TabContents> pending_contents_;

  ScopedRunnableMethodFactory<AppLauncher> method_factory_;

  // Container of the background, NavigationBar and Renderer.
  views::View* top_container_;

  // Container of the NavigationBar and Renderer (shown in a bubble).
  views::View* bubble_container_;

  // The navigation bar. Only shown in compact navigation bar mode.
  NavigationBar* navigation_bar_;

  // The view containing the renderer view.
  views::NativeViewHost* render_view_container_;

  // True if the popup has ever been shown.
  bool has_shown_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_LAUNCHER_H_
