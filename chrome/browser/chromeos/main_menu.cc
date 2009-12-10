// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/main_menu.h"

#include <string>
#include <vector>

#include "app/gfx/insets.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/background.h"
#include "views/painter.h"
#include "views/screen.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

namespace chromeos {

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

// Command line switch for specifying url of the page.
static const wchar_t kURLSwitch[] = L"main-menu-url";

// Command line switch for specifying the size of the main menu. The default is
// full screen.
static const wchar_t kMenuSizeSwitch[] = L"main-menu-size";

// URL of the page to load. This is ignored if kURLSwitch is specified.
static const char kMenuURL[] = "http://goto.ext.google.com/crux-home";

// Returns the size of the popup. By default the popup is sized slightly
// larger than full screen, but can be overriden by the command line switch
// kMenuSizeSwitch.
static gfx::Size GetPopupSize() {
  std::wstring cl_size =
      CommandLine::ForCurrentProcess()->GetSwitchValue(kMenuSizeSwitch);
  if (!cl_size.empty()) {
    std::vector<std::string> chunks;
    SplitString(WideToUTF8(cl_size), 'x', &chunks);
    if (chunks.size() == 2)
      return gfx::Size(StringToInt(chunks[0]), StringToInt(chunks[1]));
  }

  gfx::Size size =
      views::Screen::GetMonitorAreaNearestPoint(gfx::Point(0, 0)).size();
  // When full screen we don't want to see the drop shadow. Adjust the width
  // so the drop shadow ends up off screen.
  size.Enlarge(kBackgroundImageRight, kBackgroundImageBottom);
  return size;
}

// Returns the size for the renderer widget host view given the specified
// size of the popup.
static gfx::Size CalculateRWHVSize(const gfx::Size& popup_size) {
  return gfx::Size(popup_size.width() - kRendererX - kBackgroundImageRight,
                   popup_size.height() - kRendererY - kBackgroundImageBottom);
}

// Returns the URL of the menu.
static GURL GetMenuURL() {
  std::wstring url_string =
      CommandLine::ForCurrentProcess()->GetSwitchValue(kURLSwitch);
  if (!url_string.empty())
    return GURL(WideToUTF8(url_string));
  return GURL(kMenuURL);
}

// static
void MainMenu::Show(Browser* browser) {
  MainMenu::Get()->ShowImpl(browser);
}

// static
void MainMenu::ScheduleCreation() {
  MessageLoop::current()->PostDelayedTask(FROM_HERE, new LoadTask(), 5000);
}

MainMenu::~MainMenu() {
  // NOTE: we leak the menu_rvh_ and popup_ as by the time we get here the
  // message loop and notification services have been shutdown.
  // TODO(sky): fix this.
  // menu_rvh_->Shutdown();
  // popup_->CloseNow();
}

MainMenu::MainMenu()
    : browser_(NULL),
      popup_(NULL),
      site_instance_(NULL),
      menu_rvh_(NULL),
      rwhv_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(tab_contents_delegate_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      has_shown_(false) {
  SkBitmap* drop_down_image = ResourceBundle::GetSharedInstance().
      GetBitmapNamed(IDR_MAIN_MENU_BUTTON_DROP_DOWN);

  views::WidgetGtk* menu_popup =
      new views::WidgetGtk(views::WidgetGtk::TYPE_POPUP);
  popup_ = menu_popup;
  // The background image has transparency, so we make the window transparent.
  menu_popup->MakeTransparent();
  gfx::Size popup_size = GetPopupSize();
  menu_popup->Init(NULL, gfx::Rect(0, 0, popup_size.width(),
                                   popup_size.height()));

  views::Painter* painter = views::Painter::CreateImagePainter(
      *drop_down_image,
      gfx::Insets(kBackgroundImageTop, kBackgroundImageLeft,
                  kBackgroundImageBottom, kBackgroundImageRight),
      false);
  menu_popup->GetRootView()->set_background(
      views::Background::CreateBackgroundPainter(true, painter));

  GURL menu_url(GetMenuURL());
  DCHECK(BrowserList::begin() != BrowserList::end());
  // TODO(sky): this shouldn't pick a random profile to use.
  Profile* profile = (*BrowserList::begin())->profile();
  site_instance_ = SiteInstance::CreateSiteInstanceForURL(profile, menu_url);
  menu_rvh_ = new RenderViewHost(site_instance_, this, MSG_ROUTING_NONE);

  rwhv_ = new RenderWidgetHostViewGtk(menu_rvh_);
  rwhv_->InitAsChild();
  menu_rvh_->CreateRenderView(profile->GetRequestContext());
  menu_popup->AddChild(rwhv_->GetNativeView());
  gfx::Size rwhv_size = CalculateRWHVSize(popup_size);
  menu_popup->PositionChild(rwhv_->GetNativeView(), kRendererX, kRendererY,
                            rwhv_size.width(), rwhv_size.height());
  rwhv_->SetSize(rwhv_size);
  menu_rvh_->NavigateToURL(menu_url);
}

// static
MainMenu* MainMenu::Get() {
  return Singleton<MainMenu>::get();
}

void MainMenu::ShowImpl(Browser* browser) {
  Cleanup();

  browser_ = browser;

  popup_->Show();

  GtkWidget* rwhv_widget = rwhv_->GetNativeView();

  if (!has_shown_) {
    has_shown_ = true;
    gtk_widget_realize(rwhv_widget);
    g_signal_connect(rwhv_widget, "button-press-event",
                     G_CALLBACK(CallButtonPressEvent), this);
  }

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

void MainMenu::Hide() {
  gdk_keyboard_ungrab(GDK_CURRENT_TIME);
  gdk_pointer_ungrab(GDK_CURRENT_TIME);

  popup_->Hide();

  // The stack may have pending_contents_ on it. Delay deleting the
  // pending_contents_ as TabContents doesn't deal well with being deleted
  // while on the stack.
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&MainMenu::Cleanup));
}

void MainMenu::Cleanup() {
  pending_contents_.reset(NULL);
  method_factory_.RevokeAll();
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
    Hide();
  }
  return FALSE;
}

void MainMenu::RequestMove(const gfx::Rect& new_bounds) {
  // Invoking PositionChild results in a gtk signal that triggers attempting to
  // to resize the window. We need to set the size request so that it resizes
  // correctly when this happens.
  gtk_widget_set_size_request(popup_->GetNativeView(),
                              new_bounds.width(), new_bounds.height());
  gfx::Size rwhv_size = CalculateRWHVSize(new_bounds.size());
  popup_->PositionChild(rwhv_->GetNativeView(), kRendererX, kRendererY,
                        rwhv_size.width(), rwhv_size.height());
  popup_->SetBounds(new_bounds);
  rwhv_->SetSize(rwhv_size);
}

RendererPreferences MainMenu::GetRendererPrefs() const {
  return renderer_preferences_util::GetInitedRendererPreferences(
      browser_->profile());
}

void MainMenu::CreateNewWindow(int route_id) {
  if (pending_contents_.get()) {
    NOTREACHED();
    return;
  }

  helper_.CreateNewWindow(route_id, browser_->profile(), site_instance_,
                          DOMUIFactory::GetDOMUIType(GURL(GetMenuURL())),
                          NULL);
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
    Hide();
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
  menu_->Hide();
}

// LoadTask -------------------------------------------------------------------

void MainMenu::LoadTask::Run() {
  MainMenu::Get();
}

}  // namespace chromeos
