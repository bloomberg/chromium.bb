// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAIN_MENU_H_
#define CHROME_BROWSER_CHROMEOS_MAIN_MENU_H_

#include <gtk/gtk.h>

#include "chrome/browser/renderer_host/render_view_host_delegate.h"

class Browser;
class RenderWidgetHostViewGtk;
class SiteInstance;

namespace views {
class Widget;
}

// MainMenu manages shoing the main menu. The menu is currently an HTML page.
// When the user clicks a link on the page a new tab is added to the current
// browser and the menu is hidden.
//
// To show the menu invoke Show.
//
// MainMenu creates a RenderViewHost and corresponding RenderWidgetHostView
// to display the html page. MainMenu acts as the RenderViewHostDelegate for
// the RenderViewHost. Additionally when the user clicks a link a new window
// is created (child_rvh_). MainMenu is set as the RenderViewHostDelegate of
// the child_rvh_ so that it can receive the request to open the url
// (RequestOpenURL).
//
// When a new url is opened, or the user clicks outsides the bounds of the
// widget the menu is closed.
//
// MainMenu manages its own lifetime.
class MainMenu : public RenderViewHostDelegate,
                 public RenderViewHostDelegate::View {
 public:
  // Shows the menu.
  static void Show(Browser* browser);

 private:
  explicit MainMenu(Browser* browser);
  ~MainMenu();

  void ShowImpl();

  // Callback from button presses on the render widget host view. Clicks
  // outside the widget resulting in closing the menu.
  static gboolean CallButtonPressEvent(GtkWidget* widget,
                                       GdkEventButton* event,
                                       MainMenu* menu) {
    return menu->OnButtonPressEvent(widget, event);
  }
  gboolean OnButtonPressEvent(GtkWidget* widget,
                              GdkEventButton* event);

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
  virtual void RequestOpenURL(const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition);

  // RenderViewHostDelegate::View overrides.
  virtual void CreateNewWindow(int route_id,
                               base::WaitableEvent* modal_dialog_event);
  virtual void CreateNewWidget(int route_id, bool activatable) {}
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture,
                                 const GURL& creator_url) {}
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) {}
  virtual void ShowContextMenu(const ContextMenuParams& params) {}
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops) {}
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) {}
  virtual void GotFocus() {}
  virtual void TakeFocus(bool reverse) {}
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}
  virtual void HandleMouseEvent() {}
  virtual void HandleMouseLeave() {}
  virtual void UpdatePreferredWidth(int pref_width) {}

  // The currently active browser. We use this to open urls.
  Browser* browser_;

  // The widget displaying the rwvh_.
  views::Widget* popup_;

  // SiteInstance for the RenderViewHosts we create.
  SiteInstance* site_instance_;

  // RenderViewHost for the menu.
  RenderViewHost* menu_rvh_;

  // RenderWidgetHostView from the menu_rvh_.
  RenderWidgetHostViewGtk* rwhv_;

  // If the user clicks an item in the menu a child RenderViewHost is opened.
  // This is that child.
  RenderViewHost* child_rvh_;

  DISALLOW_COPY_AND_ASSIGN(MainMenu);
};

#endif  // CHROME_BROWSER_CHROMEOS_CHROMEOS_VERSION_LOADER_H_
