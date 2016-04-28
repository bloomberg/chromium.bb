// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/chromevox_panel.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "ui/chromeos/accessibility_types.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_animations.h"

namespace {

const int kPanelHeight = 35;
const char kChromeVoxPanelRelativeUrl[] = "/cvox2/background/panel.html";
const char kFullscreenURLFragment[] = "fullscreen";
const char kDisableSpokenFeedbackURLFragment[] = "close";
const char kFocusURLFragment[] = "focus";

}  // namespace

class ChromeVoxPanelWebContentsObserver : public content::WebContentsObserver {
 public:
  ChromeVoxPanelWebContentsObserver(content::WebContents* web_contents,
                                    ChromeVoxPanel* panel)
      : content::WebContentsObserver(web_contents), panel_(panel) {}
  ~ChromeVoxPanelWebContentsObserver() override {}

  // content::WebContentsObserver overrides.
  void DidFirstVisuallyNonEmptyPaint() override {
    panel_->DidFirstVisuallyNonEmptyPaint();
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    // The ChromeVox panel uses the URL fragment to communicate state
    // to this panel host.
    std::string fragment = web_contents()->GetLastCommittedURL().ref();
    if (fragment == kDisableSpokenFeedbackURLFragment)
      panel_->DisableSpokenFeedback();
    else if (fragment == kFullscreenURLFragment)
      panel_->EnterFullscreen();
    else if (fragment == kFocusURLFragment)
      panel_->Focus();
    else
      panel_->ExitFullscreen();
  }

 private:
  ChromeVoxPanel* panel_;

  DISALLOW_COPY_AND_ASSIGN(ChromeVoxPanelWebContentsObserver);
};

ChromeVoxPanel::ChromeVoxPanel(content::BrowserContext* browser_context)
    : widget_(nullptr), web_view_(nullptr), panel_fullscreen_(false) {
  std::string url("chrome-extension://");
  url += extension_misc::kChromeVoxExtensionId;
  url += kChromeVoxPanelRelativeUrl;

  views::WebView* web_view = new views::WebView(browser_context);
  content::WebContents* contents = web_view->GetWebContents();
  web_contents_observer_.reset(
      new ChromeVoxPanelWebContentsObserver(contents, this));
  extensions::SetViewType(contents, extensions::VIEW_TYPE_COMPONENT);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      contents);
  web_view->LoadInitialURL(GURL(url));
  web_view_ = web_view;

  widget_ = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  aura::Window* root_window = ash::Shell::GetPrimaryRootWindow();
  params.parent = ash::Shell::GetContainer(
      root_window, ash::kShellWindowId_SettingBubbleContainer);
  params.delegate = this;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.bounds = gfx::Rect(0, 0, root_window->bounds().width(),
                            root_window->bounds().height());
  widget_->Init(params);
  SetShadowType(widget_->GetNativeWindow(), wm::SHADOW_TYPE_RECTANGULAR);

  display::Screen::GetScreen()->AddObserver(this);
}

ChromeVoxPanel::~ChromeVoxPanel() {
  display::Screen::GetScreen()->RemoveObserver(this);
}

aura::Window* ChromeVoxPanel::GetRootWindow() {
  return GetWidget()->GetNativeWindow()->GetRootWindow();
}

void ChromeVoxPanel::Close() {
  widget_->Close();
}

void ChromeVoxPanel::DidFirstVisuallyNonEmptyPaint() {
  widget_->Show();
  UpdatePanelHeight();
}

void ChromeVoxPanel::UpdatePanelHeight() {
  ash::Shelf* shelf = ash::Shelf::ForWindow(GetRootWindow());
  if (!shelf)
    return;

  ash::ShelfLayoutManager* shelf_layout_manager = shelf->shelf_layout_manager();
  if (shelf_layout_manager)
    shelf_layout_manager->SetChromeVoxPanelHeight(kPanelHeight);
}

void ChromeVoxPanel::EnterFullscreen() {
  Focus();
  panel_fullscreen_ = true;
  UpdateWidgetBounds();
}

void ChromeVoxPanel::ExitFullscreen() {
  widget_->Deactivate();
  widget_->widget_delegate()->set_can_activate(false);
  panel_fullscreen_ = false;
  UpdateWidgetBounds();
}

void ChromeVoxPanel::DisableSpokenFeedback() {
  chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ui::A11Y_NOTIFICATION_NONE);
}

void ChromeVoxPanel::Focus() {
  widget_->widget_delegate()->set_can_activate(true);
  widget_->Activate();
  web_view_->RequestFocus();
}

const views::Widget* ChromeVoxPanel::GetWidget() const {
  return widget_;
}

views::Widget* ChromeVoxPanel::GetWidget() {
  return widget_;
}

void ChromeVoxPanel::DeleteDelegate() {
  delete this;
}

views::View* ChromeVoxPanel::GetContentsView() {
  return web_view_;
}

void ChromeVoxPanel::OnDisplayMetricsChanged(const display::Display& display,
                                             uint32_t changed_metrics) {
  UpdateWidgetBounds();
}

void ChromeVoxPanel::UpdateWidgetBounds() {
  gfx::Rect bounds(GetRootWindow()->bounds().size());
  if (!panel_fullscreen_)
    bounds.set_height(kPanelHeight);

  // If we're in full-screen mode, give the panel a height of 0 unless
  // it's active.
  if (ash::GetRootWindowController(GetRootWindow())
          ->GetWindowForFullscreenMode() &&
      !widget_->IsActive()) {
    bounds.set_height(0);
  }

  widget_->SetBounds(bounds);
}
