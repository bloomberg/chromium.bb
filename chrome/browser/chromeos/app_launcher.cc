// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_launcher.h"

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
#include "chrome/browser/views/bubble_border.h"
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

// Padding for the bubble info window.
const int kBubbleWindowXPadding = 6;
const int kBubbleWindowYPadding = 16;

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

// RenderWidgetHostViewGtk propagates the mouse press events (see
// render_widget_host_view_gtk.cc).  We subclass to stop the propagation here,
// as if the click event was propagated it would reach the TopContainer view and
// it would close the popup.
class RWHVNativeViewHost : public views::NativeViewHost {
 public:
  RWHVNativeViewHost() {}

  virtual bool OnMousePressed(const views::MouseEvent& event) { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(RWHVNativeViewHost);
};

}  // namspace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NavigationBar
//
// A navigation bar that is shown in the app launcher in compact navigation bar
// mode.

class NavigationBar : public views::View,
                      public AutocompleteEditController,
                      public BubblePositioner {
 public:
  explicit NavigationBar(AppLauncher* app_launcher)
      : views::View(),
        app_launcher_(app_launcher),
        location_entry_view_(NULL) {
    SetFocusable(true);
    location_entry_view_ = new views::NativeViewHost;
    AddChildView(location_entry_view_);
  }

  virtual ~NavigationBar() {
    if (location_entry_view_->native_view())
      location_entry_view_->Detach();
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
    app_launcher_->AddTabWithURL(url, transition);
    app_launcher_->Hide();
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
  AppLauncher* app_launcher_;
  views::NativeViewHost* location_entry_view_;
  scoped_ptr<AutocompleteEditViewGtk> location_entry_;

  DISALLOW_COPY_AND_ASSIGN(NavigationBar);
};

////////////////////////////////////////////////////////////////////////////////
// TopContainer
//
// A view that grays-out the browser and contains the navigation bar and
// renderer view.

AppLauncher::TopContainer::TopContainer(AppLauncher* app_launcher)
    : app_launcher_(app_launcher) {
  // Use a transparent black background so the browser appears grayed-out.
  set_background(views::Background::CreateSolidBackground(0, 0, 0, 100));
}

void AppLauncher::TopContainer::Layout() {
  if (bounds().IsEmpty())
    return;

  // We only expect to contain the BubbleContents.
  DCHECK(GetChildViewCount() == 1);
  GetChildViewAt(0)->SetBounds(kBubbleWindowXPadding, kBubbleWindowYPadding,
                               width() * 2 / 3, height() * 4 / 5);
}

bool AppLauncher::TopContainer::OnMousePressed(const views::MouseEvent& event) {
  // Clicking outside the bubble closes the bubble.
  app_launcher_->Hide();
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// BubbleContainer
//
// The view that contains the navigation bar and render view.  It has a bubble
// border.

AppLauncher::BubbleContainer::BubbleContainer(AppLauncher* app_launcher)
    : app_launcher_(app_launcher) {
  BubbleBorder* bubble_border = new BubbleBorder();
  bubble_border->set_arrow_location(BubbleBorder::TOP_LEFT);
  set_border(bubble_border);
  set_background(new BubbleBackground(bubble_border));
}

void AppLauncher::BubbleContainer::Layout() {
  if (bounds().IsEmpty() || GetChildViewCount() == 0)
    return;

  gfx::Rect bounds = GetLocalBounds(false);
  // TODO(jcampan): figure-out why we need to inset for the contained view not
  //                to paint over the bubble border.
  bounds.Inset(2, 2);

  if (app_launcher_->navigation_bar_->IsVisible()) {
    app_launcher_->navigation_bar_->SetBounds(bounds.x(), bounds.y(),
                                              bounds.width(),
                                              kNavigationBarHeight);
  } else {
    app_launcher_->navigation_bar_->SetBounds(bounds.x(), bounds.y(), 0, 0);
  }
  int render_y = app_launcher_->navigation_bar_->bounds().bottom();
  gfx::Size rwhv_size =
      gfx::Size(bounds.width(),
                std::max(0, bounds.height() - render_y + bounds.y()));
  app_launcher_->render_view_container_->SetBounds(bounds.x(), render_y,
                                                   rwhv_size.width(),
                                                   rwhv_size.height());
  app_launcher_->rwhv_->SetSize(rwhv_size);
}

////////////////////////////////////////////////////////////////////////////////
// AppLauncher

AppLauncher::AppLauncher()
    : browser_(NULL),
      popup_(NULL),
      site_instance_(NULL),
      contents_rvh_(NULL),
      rwhv_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(tab_contents_delegate_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      top_container_(NULL),
      bubble_container_(NULL),
      navigation_bar_(NULL),
      render_view_container_(NULL),
      has_shown_(false) {
  popup_ = new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
  // The background image has transparency, so we make the window transparent.
  popup_->MakeTransparent();
  popup_->Init(NULL, gfx::Rect());
  WmIpc::instance()->SetWindowType(
      popup_->GetNativeView(),
      WmIpc::WINDOW_TYPE_CHROME_INFO_BUBBLE,
      NULL);

  // Register Esc as an accelerator for closing the app launcher.
  views::FocusManager* focus_manager = popup_->GetFocusManager();
  focus_manager->RegisterAccelerator(views::Accelerator(base::VKEY_ESCAPE,
                                                        false, false, false),
                                     this);

  top_container_ = new TopContainer(this);
  popup_->SetContentsView(top_container_);

  bubble_container_ = new BubbleContainer(this);
  top_container_->AddChildView(bubble_container_);
  navigation_bar_ = new NavigationBar(this);
  bubble_container_->AddChildView(navigation_bar_);

  GURL menu_url(GetMenuURL());
  DCHECK(BrowserList::begin() != BrowserList::end());
  // TODO(sky): this shouldn't pick a random profile to use.
  Profile* profile = (*BrowserList::begin())->profile();
  int64 session_storage_namespace_id = profile->GetWebKitContext()->
      dom_storage_context()->AllocateSessionStorageNamespaceId();
  site_instance_ = SiteInstance::CreateSiteInstanceForURL(profile, menu_url);
  contents_rvh_ = new RenderViewHost(site_instance_, this, MSG_ROUTING_NONE,
                                     session_storage_namespace_id);

  rwhv_ = new RenderWidgetHostViewGtk(contents_rvh_);
  rwhv_->InitAsChild();
  contents_rvh_->CreateRenderView(profile->GetRequestContext());

  render_view_container_ = new RWHVNativeViewHost;
  bubble_container_->AddChildView(render_view_container_);
  render_view_container_->Attach(rwhv_->GetNativeView());
  contents_rvh_->NavigateToURL(menu_url);

  ActiveWindowWatcherX::AddObserver(this);
}

AppLauncher::~AppLauncher() {
  contents_rvh_->Shutdown();
  popup_->CloseNow();
  ActiveWindowWatcherX::RemoveObserver(this);
}

void AppLauncher::Update(Browser* browser) {
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
  popup_->SetBounds(browser_->window()->GetRestoredBounds());
  top_container_->Layout();
}

void AppLauncher::Show(Browser* browser) {
  Cleanup();

  Update(browser);
  popup_->Show();

  GtkWidget* rwhv_widget = rwhv_->GetNativeView();
  if (!has_shown_) {
    has_shown_ = true;
    gtk_widget_realize(rwhv_widget);
  }
}

void AppLauncher::ActiveWindowChanged(GdkWindow* active_window) {
  if (!popup_->IsActive())
    Hide();
  else
    navigation_bar_->RequestFocus();
}

bool AppLauncher::AcceleratorPressed(const views::Accelerator& accelerator) {
  DCHECK(accelerator.GetKeyCode() == base::VKEY_ESCAPE);
  popup_->Hide();
  return true;
}

void AppLauncher::Hide() {
  popup_->Hide();
  // The stack may have pending_contents_ on it. Delay deleting the
  // pending_contents_ as TabContents doesn't deal well with being deleted
  // while on the stack.
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&AppLauncher::Cleanup));
}

void AppLauncher::Cleanup() {
  pending_contents_.reset(NULL);
  method_factory_.RevokeAll();
}

void AppLauncher::RequestMove(const gfx::Rect& new_bounds) {
  // Invoking PositionChild results in a gtk signal that triggers attempting to
  // to resize the window. We need to set the size request so that it resizes
  // correctly when this happens.
  gtk_widget_set_size_request(popup_->GetNativeView(),
                              new_bounds.width(), new_bounds.height());
  popup_->SetBounds(new_bounds);
}

RendererPreferences AppLauncher::GetRendererPrefs(Profile* profile) const {
  RendererPreferences preferences;
  renderer_preferences_util::UpdateFromSystemSettings(&preferences, profile);
  return preferences;
}

void AppLauncher::AddTabWithURL(const GURL& url,
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

void AppLauncher::CreateNewWindow(int route_id) {
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

void AppLauncher::ShowCreatedWindow(int route_id,
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

void AppLauncher::StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops) {
  // We're not going to do any drag & drop, but we have to tell the renderer the
  // drag & drop ended, othewise the renderer thinks the drag operation is
  // underway and mouse events won't work.
  contents_rvh_->DragSourceSystemDragEnded();
}

AppLauncher::TabContentsDelegateImpl::TabContentsDelegateImpl(
    AppLauncher* app_launcher)
    : app_launcher_(app_launcher) {
}

void AppLauncher::TabContentsDelegateImpl::OpenURLFromTab(
    TabContents* source,
    const GURL& url,
    const GURL& referrer,
    WindowOpenDisposition disposition,
    PageTransition::Type transition) {
  app_launcher_->browser_->OpenURL(url, referrer, NEW_FOREGROUND_TAB,
                                   PageTransition::LINK);
  app_launcher_->Hide();
}

}  // namespace chromeos
