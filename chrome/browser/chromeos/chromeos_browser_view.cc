// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromeos_browser_view.h"

#include "chrome/browser/chromeos/panel_browser_view.h"
#include "chrome/browser/views/frame/browser_extender.h"
#include "chrome/browser/views/frame/chrome_browser_view_layout_manager.h"
#include "chrome/browser/views/tabs/tab.h"
#include "chrome/browser/views/tabs/tab_overview_types.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "views/window/window.h"

namespace {

// A chromeos implementation of Tab that shows the compact location bar.
class ChromeosTab : public Tab {
 public:
  ChromeosTab(TabStrip* tab_strip, const BrowserView* browser_view)
      : Tab(tab_strip),
        browser_view_(browser_view) {
  }
  virtual ~ChromeosTab() {}

  // Overridden from views::View.
  virtual void OnMouseEntered(const views::MouseEvent& event) {
    TabRenderer::OnMouseEntered(event);
    browser_view_->browser_extender()->OnMouseEnteredToTab(this);
  }

  virtual void OnMouseMoved(const views::MouseEvent& event) {
    browser_view_->browser_extender()->OnMouseMovedOnTab(this);
  }

  virtual void OnMouseExited(const views::MouseEvent& event) {
    TabRenderer::OnMouseExited(event);
    browser_view_->browser_extender()->OnMouseExitedFromTab(this);
  }

 private:
  const BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(ChromeosTab);
};

// A Tabstrip that uses ChromeosTab as a Tab implementation.
class ChromeosTabStrip : public TabStrip {
 public:
  ChromeosTabStrip(TabStripModel* model, const BrowserView* browser_view)
      : TabStrip(model), browser_view_(browser_view) {
  }
  virtual ~ChromeosTabStrip() {}

 protected:
  // Overridden from TabStrip.
  virtual Tab* CreateTab() {
    return new ChromeosTab(this, browser_view_);
  }

 private:
  const BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(ChromeosTabStrip);
};

// LayoutManager for ChromeosBrowserView, which layouts extra components such as
// main menu, stataus are via BrowserExtender.
class ChromeosBrowserViewLayoutManager : public ChromeBrowserViewLayoutManager {
 public:
  ChromeosBrowserViewLayoutManager() : ChromeBrowserViewLayoutManager() {}
  virtual ~ChromeosBrowserViewLayoutManager() {}

  // Overriden from ChromeBrowserViewLayoutManager.
  virtual int LayoutTabStrip() {
    if (!browser_view_->IsTabStripVisible()) {
      tabstrip_->SetVisible(false);
      tabstrip_->SetBounds(0, 0, 0, 0);
      return 0;
    } else {
      gfx::Rect layout_bounds =
          browser_view_->frame()->GetBoundsForTabStrip(tabstrip_);
      gfx::Rect toolbar_bounds = browser_view_->GetToolbarBounds();
      tabstrip_->SetBackgroundOffset(
          gfx::Point(layout_bounds.x() - toolbar_bounds.x(),
                     layout_bounds.y()));
      gfx::Point tabstrip_origin = layout_bounds.origin();
      views::View::ConvertPointToView(browser_view_->GetParent(), browser_view_,
                                      &tabstrip_origin);
      layout_bounds.set_origin(tabstrip_origin);
      tabstrip_->SetVisible(true);
      tabstrip_->SetBounds(layout_bounds);

      int bottom = 0;
      gfx::Rect tabstrip_bounds;
      browser_view_->browser_extender()->Layout(
          layout_bounds, &tabstrip_bounds, &bottom);
      tabstrip_->SetVisible(true);
      tabstrip_->SetBounds(tabstrip_bounds);
      return bottom;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeosBrowserViewLayoutManager);
};

}  // namespace

namespace chromeos {

ChromeosBrowserView::ChromeosBrowserView(Browser* browser)
    : BrowserView(browser) {
}

void ChromeosBrowserView::Show() {
  bool was_visible = frame()->GetWindow()->IsVisible();
  BrowserView::Show();
  if (!was_visible) {
    TabOverviewTypes::instance()->SetWindowType(
        GTK_WIDGET(frame()->GetWindow()->GetNativeWindow()),
        TabOverviewTypes::WINDOW_TYPE_CHROME_TOPLEVEL,
        NULL);
  }
}

bool ChromeosBrowserView::IsToolbarVisible() const {
  if (browser_extender()->ShouldForceHideToolbar())
    return false;
  return BrowserView::IsToolbarVisible();
}

void ChromeosBrowserView::SetFocusToLocationBar() {
  if (!browser_extender()->SetFocusToCompactNavigationBar()) {
    BrowserView::SetFocusToLocationBar();
  }
}

views::LayoutManager* ChromeosBrowserView::CreateLayoutManager() const {
  return new ChromeosBrowserViewLayoutManager();
}

TabStrip* ChromeosBrowserView::CreateTabStrip(
    TabStripModel* tab_strip_model) const {
  return new ChromeosTabStrip(tab_strip_model, this);
}

}  // namespace chromeos

// static
BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser) {
  // Create a browser view for chromeos.
  BrowserView* view;
  if (browser->type() & Browser::TYPE_POPUP)
    view = new chromeos::PanelBrowserView(browser);
  else
    view = new chromeos::ChromeosBrowserView(browser);
  BrowserFrame::Create(view, browser->profile());
  return view;
}
