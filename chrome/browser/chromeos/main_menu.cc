// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/main_menu.h"

#include "app/gfx/insets.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/background.h"
#include "views/painter.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

// Initial size of the renderer. This is contained within a window whose size
// is set to the size of the image IDR_MAIN_MENU_BUTTON_DROP_DOWN.
static const int kRendererX = 0;
static const int kRendererY = 25;
static const int kRendererWidth = 250;
static const int kRendererHeight = 400;

// Insets defining the regions that are stretched and titled to create the
// background of the popup. These constants are fed into
// Painter::CreateImagePainter as the insets, see it for details.
static const int kBackgroundImageTop = 27;
static const int kBackgroundImageLeft = 85;
static const int kBackgroundImageBottom = 10;
static const int kBackgroundImageRight = 8;

// URL of the page to load.
static const char kMenuURL[] = "http://goto.ext.google.com/crux-menu";

// static
void MainMenu::Show(Browser* browser) {
  (new MainMenu(browser))->ShowImpl();
}

MainMenu::~MainMenu() {
  popup_->Close();
  menu_rvh_->Shutdown();
}

MainMenu::MainMenu(Browser* browser)
    : browser_(browser),
      popup_(NULL),
      site_instance_(NULL),
      menu_rvh_(NULL),
      rwhv_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(tab_contents_delegate_(this)) {
}

void MainMenu::ShowImpl() {
  SkBitmap* drop_down_image = ResourceBundle::GetSharedInstance().
      GetBitmapNamed(IDR_MAIN_MENU_BUTTON_DROP_DOWN);

  views::WidgetGtk* menu_popup =
      new views::WidgetGtk(views::WidgetGtk::TYPE_POPUP);
  popup_ = menu_popup;
  // The background image has transparency, so we make the window transparent.
  menu_popup->MakeTransparent();
  menu_popup->Init(NULL, gfx::Rect(0, 0, drop_down_image->width(),
                                   drop_down_image->height()));

  views::Painter* painter = views::Painter::CreateImagePainter(
      *drop_down_image,
      gfx::Insets(kBackgroundImageTop, kBackgroundImageLeft,
                  kBackgroundImageBottom, kBackgroundImageRight));
  menu_popup->GetRootView()->set_background(
      views::Background::CreateBackgroundPainter(true, painter));

  GURL menu_url(kMenuURL);
  site_instance_ = SiteInstance::CreateSiteInstanceForURL(browser_->profile(),
                                                          menu_url);
  menu_rvh_ = new RenderViewHost(site_instance_, this, MSG_ROUTING_NONE);

  rwhv_ = new RenderWidgetHostViewGtk(menu_rvh_);
  rwhv_->InitAsChild();
  menu_rvh_->CreateRenderView();
  menu_popup->AddChild(rwhv_->GetNativeView());
  menu_popup->PositionChild(rwhv_->GetNativeView(), kRendererX, kRendererY,
                            kRendererWidth, kRendererHeight);
  rwhv_->SetSize(gfx::Size(kRendererWidth, kRendererHeight));
  menu_rvh_->NavigateToURL(menu_url);
  menu_popup->Show();

  GtkWidget* rwhv_widget = rwhv_->GetNativeView();
  gtk_widget_realize(rwhv_widget);
  g_signal_connect(rwhv_widget, "button-press-event",
                   G_CALLBACK(CallButtonPressEvent), this);
  // Do a mouse grab on the renderer widget host view's widget so that we can
  // close the popup if the user clicks anywhere else. And do a keyboard
  // grab so that we get all key events.
  gdk_pointer_grab(rwhv_widget->window, FALSE,
                   static_cast<GdkEventMask>(
                         GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                         GDK_POINTER_MOTION_MASK),
                   NULL, NULL, GDK_CURRENT_TIME);
  gdk_keyboard_grab(rwhv_widget->window, FALSE, GDK_CURRENT_TIME);
}

void MainMenu::Delete(bool now) {
  gdk_keyboard_ungrab(GDK_CURRENT_TIME);
  gdk_pointer_ungrab(GDK_CURRENT_TIME);
  // Hide the popup immediately. We don't close it as it contains the
  // renderwidgethostview, which hasn't been shutdown yet.
  popup_->Hide();
  if (now)
    delete this;
  else
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

// static
gboolean MainMenu::CallButtonPressEvent(GtkWidget* widget,
                                        GdkEventButton* event,
                                        MainMenu* menu) {
  return menu->OnButtonPressEvent(widget, event);
}

gboolean MainMenu::OnButtonPressEvent(GtkWidget* widget,
                                      GdkEventButton* event) {
  if (event->x < 0 || event->y < 0 ||
      event->x >= widget->allocation.width ||
      event->y >= widget->allocation.height) {
    // The user clicked outside the bounds of the menu, delete the main which
    // results in closing it.
    Delete(true);
  }
  return FALSE;
}

void MainMenu::RequestMove(const gfx::Rect& new_bounds) {
  SkBitmap* drop_down_image = ResourceBundle::GetSharedInstance().
      GetBitmapNamed(IDR_MAIN_MENU_BUTTON_DROP_DOWN);
  int new_w = drop_down_image->width() + (new_bounds.width() - kRendererWidth);
  int new_h = drop_down_image->height() +
              (new_bounds.height() - kRendererHeight);
  // Invoking PositionChild results in a gtk signal that triggers attempting to
  // to resize the window. We need to set the size request so that it resizes
  // correctly when this happens.
  gtk_widget_set_size_request(popup_->GetNativeView(), new_w, new_h);
  popup_->PositionChild(rwhv_->GetNativeView(), kRendererX, kRendererY,
                        new_bounds.width(), new_bounds.height());
  popup_->SetBounds(gfx::Rect(new_bounds.x(), new_bounds.y(), new_w, new_h));
  rwhv_->SetSize(new_bounds.size());
}

void MainMenu::CreateNewWindow(int route_id) {
  if (pending_contents_.get()) {
    NOTREACHED();
    return;
  }

  helper_.CreateNewWindow(route_id, browser_->profile(), site_instance_,
                          DOMUIFactory::GetDOMUIType(GURL(kMenuURL)), NULL);
  pending_contents_.reset(helper_.GetCreatedWindow(route_id));
  pending_contents_->set_delegate(&tab_contents_delegate_);
}

void MainMenu::ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture,
                                 const GURL& creator_url) {
  if (disposition == NEW_POPUP) {
    pending_contents_->set_delegate(NULL);
    browser_->GetSelectedTabContents()->AddNewContents(
        pending_contents_.release(), disposition, initial_pos, user_gesture,
        creator_url);
    Delete(false);
  }
}

void MainMenu::TabContentsDelegateImpl::OpenURLFromTab(
    TabContents* source,
    const GURL& url,
    const GURL& referrer,
    WindowOpenDisposition disposition,
    PageTransition::Type transition) {
  menu_->browser_->OpenURL(url, referrer, NEW_FOREGROUND_TAB,
                           PageTransition::LINK);
  menu_->Delete(true);
}
