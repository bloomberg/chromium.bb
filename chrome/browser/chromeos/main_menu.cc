// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/main_menu.h"

#include <string>
#include <vector>

#include "app/gfx/canvas.h"
#include "app/gfx/insets.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/bubble_positioner.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/in_process_webkit/dom_storage_context.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/background.h"
#include "views/controls/native/native_view_host.h"
#include "views/painter.h"
#include "views/screen.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"


namespace {

// Padding & margins for the navigation entry.
const int kNavigationEntryPadding = 2;
const int kNavigationEntryXMargin = 3;
const int kNavigationEntryYMargin = 1;

// NavigationBar size.
const int kNavigationBarWidth = 300;
const int kNavigationBarHeight = 25;

// Insets defining the regions that are stretched and titled to create the
// background of the popup. These constants are fed into
// Painter::CreateImagePainter as the insets, see it for details.
const int kBackgroundImageTop = 27;
const int kBackgroundImageLeft = 85;
const int kBackgroundImageBottom = 10;
const int kBackgroundImageRight = 8;

// Command line switch for specifying url of the page.
const wchar_t kURLSwitch[] = L"main-menu-url";

// Command line switch for specifying the size of the main menu. The default is
// full screen.
const wchar_t kMenuSizeSwitch[] = L"main-menu-size";

// URL of the page to load. This is ignored if kURLSwitch is specified.
const char kMenuURL[] = "http://goto.ext.google.com/crux-home";

// Returns the URL of the menu.
static GURL GetMenuURL() {
  std::wstring url_string =
      CommandLine::ForCurrentProcess()->GetSwitchValue(kURLSwitch);
  if (!url_string.empty())
    return GURL(WideToUTF8(url_string));
  return GURL(kMenuURL);
}

}  // namspace

namespace chromeos {

// A navigation bar that is shown in the main menu in
// compact navigation bar mode.
class NavigationBar : public views::View,
                      public AutocompleteEditController,
                      public BubblePositioner {
 public:
  explicit NavigationBar(MainMenu* main_menu)
      : views::View(),
        main_menu_(main_menu),
        location_entry_view_(NULL) {
    SetFocusable(true);
    location_entry_view_ = new views::NativeViewHost;
    AddChildView(location_entry_view_);
  }

  virtual ~NavigationBar() {
  }

  // views::View overrides.
  virtual void Focus() {
    location_entry_->SetFocus();
    location_entry_->SelectAll(true);
  }

  virtual void Layout() {
    const int horizontal_margin =
        kNavigationEntryPadding + kNavigationEntryYMargin;

    location_entry_view_->SetBounds(
        kNavigationEntryXMargin + kNavigationEntryPadding, horizontal_margin,
        kNavigationBarWidth, height() - horizontal_margin * 2);
  }

  virtual void Paint(gfx::Canvas* canvas) {
    const int padding = kNavigationEntryPadding;
    canvas->FillRectInt(SK_ColorWHITE, 0, 0, width(), height());
    // Draw border around the entry.
    gfx::Rect bounds = location_entry_view_->GetBounds(
        views::View::APPLY_MIRRORING_TRANSFORMATION);
    canvas->DrawRectInt(SK_ColorGRAY,
                        bounds.x() - padding,
                        bounds.y() - padding,
                        bounds.width() + padding * 2,
                        bounds.height() + padding * 2);
  }

  // BubblePositioner implementation.
  virtual gfx::Rect GetLocationStackBounds() const {
    gfx::Rect bounds = location_entry_view_->GetBounds(
        views::View::APPLY_MIRRORING_TRANSFORMATION);
    gfx::Point origin(bounds.x(), bounds.bottom() + kNavigationEntryPadding);
    views::View::ConvertPointToScreen(this, &origin);
    gfx::Rect rect = gfx::Rect(origin, gfx::Size(500, 0));
    if (UILayoutIsRightToLeft()) {
      // Align the window to the right side of the entry view when
      // UI is RTL mode.
      rect.set_x(rect.x() - (rect.width() - location_entry_view_->width()));
    }
    return rect;
  }

  // AutocompleteController implementation.
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    PageTransition::Type transition,
                                    const GURL& alternate_nav_url) {
    main_menu_->AddTabWithURL(url, transition);
    main_menu_->Hide();
  }
  virtual void OnChanged() {}
  virtual void OnInputInProgress(bool in_progress) {}
  virtual void OnKillFocus() {}
  virtual void OnSetFocus() {
    views::FocusManager* focus_manager = GetFocusManager();
    if (!focus_manager) {
      NOTREACHED();
      return;
    }
    focus_manager->SetFocusedView(this);
  }
  virtual SkBitmap GetFavIcon() const {
    return SkBitmap();
  }
  virtual std::wstring GetTitle() const {
    return std::wstring();
  }

  // AutocompleteEditView depends on the browser instance.
  // Create new one when the browser instance changes.
  void Update(Browser* browser) {
    // Detach the native view if any.
    if (location_entry_view_ && location_entry_view_->native_view())
      location_entry_view_->Detach();

    location_entry_.reset(new AutocompleteEditViewGtk(
        this, browser->toolbar_model(), browser->profile(),
        browser->command_updater(), false, this));
    location_entry_->Init();
    gtk_widget_show_all(location_entry_->widget());
    gtk_widget_hide(location_entry_->widget());

    location_entry_view_->set_focus_view(this);
    location_entry_view_->Attach(location_entry_->widget());
  }

 private:
  MainMenu* main_menu_;
  views::NativeViewHost* location_entry_view_;
  scoped_ptr<AutocompleteEditViewGtk> location_entry_;

  DISALLOW_COPY_AND_ASSIGN(NavigationBar);
};

// A container for the main menu's contents (navigation bar and renderer).
class AppMenuContainer : public views::View {
 public:
  explicit AppMenuContainer(MainMenu* main_menu)
      : View(),
        main_menu_(main_menu) {
  }

  virtual ~AppMenuContainer() {
  }

  // views::View overrides.
  virtual void Layout() {
    if (GetChildViewCount() == 2 && !bounds().IsEmpty()) {
      int render_width = width() - kBackgroundImageRight;
      if (main_menu_->navigation_bar_->IsVisible()) {
        main_menu_->navigation_bar_->SetBounds(
            0, kBackgroundImageTop, render_width, kNavigationBarHeight);
      } else {
        main_menu_->navigation_bar_->SetBounds(
            gfx::Rect(0, kBackgroundImageTop, 0, 0));
      }
      int render_y = main_menu_->navigation_bar_->bounds().bottom();
      int render_height =
          std::max(0, height() - kBackgroundImageBottom - render_y);
      gfx::Size rwhv_size = gfx::Size(render_width, render_height);

      main_menu_->menu_content_view_->SetBounds(
          0, render_y, rwhv_size.width(), rwhv_size.height());
      main_menu_->rwhv_->SetSize(rwhv_size);
    }
  }

  virtual gfx::Size GetPreferredSize() {
    // Not really used.
    return gfx::Size();
  }

  // Hide if a mouse is clicked outside of the content area.
  virtual bool OnMousePressed(const views::MouseEvent& event) {
    if (HitTest(event.location()) &&
        event.y() > main_menu_->navigation_bar_->y()) {
      return false;
    }
    main_menu_->Hide();
    return false;
  }

 private:
  MainMenu* main_menu_;

  DISALLOW_COPY_AND_ASSIGN(AppMenuContainer);
};

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
  // if (location_entry_view_->native_view())
  //   location_entry_view_->Detach();
  // etc
  ActiveWindowWatcherX::RemoveObserver(this);
}

MainMenu::MainMenu()
    : browser_(NULL),
      popup_(NULL),
      site_instance_(NULL),
      menu_rvh_(NULL),
      rwhv_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(tab_contents_delegate_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      menu_container_(NULL),
      navigation_bar_(NULL),
      menu_content_view_(NULL),
      has_shown_(false) {
  SkBitmap* drop_down_image = ResourceBundle::GetSharedInstance().
      GetBitmapNamed(IDR_MAIN_MENU_BUTTON_DROP_DOWN);
  views::WidgetGtk* menu_popup =
      new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
  popup_ = menu_popup;
  // The background image has transparency, so we make the window transparent.
  menu_popup->MakeTransparent();
  popup_->Init(NULL, gfx::Rect());
  WmIpc::instance()->SetWindowType(
      popup_->GetNativeView(),
      WmIpc::WINDOW_TYPE_CHROME_INFO_BUBBLE,
      NULL);

  views::Painter* painter = views::Painter::CreateImagePainter(
      *drop_down_image,
      gfx::Insets(kBackgroundImageTop, kBackgroundImageLeft,
                  kBackgroundImageBottom, kBackgroundImageRight),
      false);
  menu_popup->GetRootView()->set_background(
      views::Background::CreateBackgroundPainter(true, painter));

  menu_container_ = new AppMenuContainer(this);
  menu_popup->SetContentsView(menu_container_);

  navigation_bar_ = new NavigationBar(this);
  menu_container_->AddChildView(navigation_bar_);

  GURL menu_url(GetMenuURL());
  DCHECK(BrowserList::begin() != BrowserList::end());
  // TODO(sky): this shouldn't pick a random profile to use.
  Profile* profile = (*BrowserList::begin())->profile();
  int64 session_storage_namespace_id = profile->GetWebKitContext()->
      dom_storage_context()->AllocateSessionStorageNamespaceId();
  site_instance_ = SiteInstance::CreateSiteInstanceForURL(profile, menu_url);
  menu_rvh_ = new RenderViewHost(site_instance_, this, MSG_ROUTING_NONE,
                                 session_storage_namespace_id);

  rwhv_ = new RenderWidgetHostViewGtk(menu_rvh_);
  rwhv_->InitAsChild();
  menu_rvh_->CreateRenderView(profile->GetRequestContext());

  menu_content_view_ = new views::NativeViewHost;
  menu_container_->AddChildView(menu_content_view_);
  menu_content_view_->Attach(rwhv_->GetNativeView());
  menu_rvh_->NavigateToURL(menu_url);

  ActiveWindowWatcherX::AddObserver(this);
}

// static
MainMenu* MainMenu::Get() {
  return Singleton<MainMenu>::get();
}

void MainMenu::Update(Browser* browser) {
  if (browser_ != browser) {
    browser_ = browser;
    navigation_bar_->Update(browser);
    // Set the transient window so that ChromeOS WM treat this
    // as if a popup window.
    gtk_window_set_transient_for(
        GTK_WINDOW(popup_->GetNativeView()),
        GTK_WINDOW(browser_->window()->GetNativeHandle()));
  }

  BrowserView* bview = static_cast<BrowserView*>(browser_->window());
  navigation_bar_->SetVisible(bview->is_compact_style());
  popup_->SetBounds(GetPopupBounds());
  menu_container_->Layout();
}

gfx::Rect MainMenu::GetPopupBounds() {
  gfx::Rect window_bounds = browser_->window()->GetRestoredBounds();
  std::wstring cl_size =
      CommandLine::ForCurrentProcess()->GetSwitchValue(kMenuSizeSwitch);
  if (!cl_size.empty()) {
    std::vector<std::string> chunks;
    SplitString(WideToUTF8(cl_size), 'x', &chunks);
    if (chunks.size() == 2) {
      return gfx::Rect(window_bounds.origin(),
                       gfx::Size(StringToInt(chunks[0]),
                                 StringToInt(chunks[1])));
    }
  }

  gfx::Size window_size = window_bounds.size();
  // We don't want to see the drop shadow. Adjust the width
  // so the drop shadow ends up off screen.
  window_size.Enlarge(kBackgroundImageRight, kBackgroundImageBottom);
  window_bounds.set_size(window_size);
  return window_bounds;
}

void MainMenu::ShowImpl(Browser* browser) {
  Cleanup();

  Update(browser);
  popup_->Show();

  GtkWidget* rwhv_widget = rwhv_->GetNativeView();
  if (!has_shown_) {
    has_shown_ = true;
    gtk_widget_realize(rwhv_widget);
  }
}

void MainMenu::ActiveWindowChanged(GdkWindow* active_window) {
  if (!popup_->IsActive())
    Hide();
  else
    navigation_bar_->RequestFocus();
}

void MainMenu::Hide() {
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

void MainMenu::RequestMove(const gfx::Rect& new_bounds) {
  // Invoking PositionChild results in a gtk signal that triggers attempting to
  // to resize the window. We need to set the size request so that it resizes
  // correctly when this happens.
  gtk_widget_set_size_request(popup_->GetNativeView(),
                              new_bounds.width(), new_bounds.height());
  popup_->SetBounds(new_bounds);
}

RendererPreferences MainMenu::GetRendererPrefs(Profile* profile) const {
  RendererPreferences preferences;
  renderer_preferences_util::UpdateFromSystemSettings(&preferences, profile);
  return preferences;
}

void MainMenu::AddTabWithURL(const GURL& url,
                             PageTransition::Type transition) {
  switch (StatusAreaView::GetOpenTabsMode()) {
    case StatusAreaView::OPEN_TABS_ON_LEFT: {
      // Add the new tab at the first non-pinned location.
      int index = browser_->tabstrip_model()->IndexOfFirstNonMiniTab();
      browser_->AddTabWithURL(url, GURL(), transition,
                              true, index, true, NULL);
      break;
    }
    case StatusAreaView::OPEN_TABS_CLOBBER: {
      browser_->GetSelectedTabContents()->controller().LoadURL(
          url, GURL(), transition);
      break;
    }
    case StatusAreaView::OPEN_TABS_ON_RIGHT: {
      browser_->AddTabWithURL(url, GURL(), transition, true, -1, true, NULL);
      break;
    }
  }
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
                                 bool user_gesture) {
  if (disposition == NEW_POPUP) {
    pending_contents_->set_delegate(NULL);
    browser_->GetSelectedTabContents()->AddNewContents(
        pending_contents_.release(), disposition, initial_pos, user_gesture);
    Hide();
  }
}

void MainMenu::StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops) {
  // We're not going to do any drag & drop, but we have to tell the renderer the
  // drag & drop ended, othewise the renderer thinks the drag operation is
  // underway and mouse events won't work.
  menu_rvh_->DragSourceSystemDragEnded();
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
  if (BrowserList::begin() == BrowserList::end())
    return;  // No browser are around. Generally only happens during testing.

  MainMenu::Get();
}

}  // namespace chromeos
