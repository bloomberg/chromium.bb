// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/app_launcher.h"

#include <string>
#include <vector>

#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/bubble_positioner.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "views/controls/native/native_view_host.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include "chrome/browser/autocomplete/autocomplete_edit_view_win.h"
#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#elif defined(OS_LINUX)
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#endif
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/status/status_area_view.h"
#endif

namespace {

// Padding & margins for the navigation entry.
const int kNavigationEntryPadding = 2;
const int kNavigationEntryXMargin = 3;
const int kNavigationEntryYMargin = 1;

// NavigationBar size.
const int kNavigationBarHeight = 25;

// The delta applied to the default font size for the omnibox.
const int kAutocompleteEditFontDelta = 3;

// Command line switch for specifying url of the page.
const wchar_t kURLSwitch[] = L"main-menu-url";

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

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TabContentsDelegateImpl
//
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

TabContentsDelegateImpl::TabContentsDelegateImpl(AppLauncher* app_launcher)
    : app_launcher_(app_launcher) {
}

void TabContentsDelegateImpl::OpenURLFromTab(TabContents* source,
                                             const GURL& url,
                                             const GURL& referrer,
                                             WindowOpenDisposition disposition,
                                             PageTransition::Type transition) {
  app_launcher_->browser()->OpenURL(url, referrer, NEW_FOREGROUND_TAB,
                                   PageTransition::LINK);
  app_launcher_->Hide();
}

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
      : app_launcher_(app_launcher),
        location_entry_view_(NULL) {
    SetFocusable(true);
    location_entry_view_ = new views::NativeViewHost;
    AddChildView(location_entry_view_);
    set_border(views::Border::CreateSolidBorder(1, SK_ColorGRAY));
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

  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) {
    if (!is_add || child != this)
      return;

    DCHECK(!location_entry_.get());

    Browser* browser = app_launcher_->browser();
#if defined (OS_WIN)
    gfx::Font font;
    font = font.DeriveFont(kAutocompleteEditFontDelta);
    AutocompleteEditViewWin* autocomplete_view =
        new AutocompleteEditViewWin(font, this, browser->toolbar_model(),
                                    this, GetWidget()->GetNativeView(),
                                    browser->profile(),
                                    browser->command_updater(), false, this);
    location_entry_.reset(autocomplete_view);
    autocomplete_view->Update(NULL);
    // The Update call above sets the autocomplete text to the current one in
    // the location bar, make sure to clear it.
    autocomplete_view->SetUserText(std::wstring());
#elif defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
    AutocompleteEditViewGtk* autocomplete_view =
        new AutocompleteEditViewGtk(this, browser->toolbar_model(),
                                    browser->profile(),
                                    browser->command_updater(), false, this);
    autocomplete_view->Init();
    gtk_widget_show_all(autocomplete_view->widget());
    gtk_widget_hide(autocomplete_view->widget());
    location_entry_.reset(autocomplete_view);
#else
    NOTIMPLEMENTED();
#endif
    location_entry_view_->set_focus_view(this);
    location_entry_view_->Attach(location_entry_->GetNativeView());
  }

  virtual void Layout() {
    gfx::Rect bounds = GetLocalBounds(false);
    location_entry_view_->SetBounds(
        bounds.x() + kNavigationEntryXMargin + kNavigationEntryPadding,
        bounds.y() + kNavigationEntryYMargin,
        bounds.width() - 2 * (kNavigationEntryPadding +
                              kNavigationEntryXMargin),
        bounds.height() - kNavigationEntryYMargin * 2);
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

 private:
  AppLauncher* app_launcher_;
  views::NativeViewHost* location_entry_view_;
#if defined(OS_WIN)
  scoped_ptr<AutocompleteEditViewWin> location_entry_;
#elif defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
  scoped_ptr<AutocompleteEditViewGtk> location_entry_;
#else
  NOTIMPLEMENTED();
#endif

  DISALLOW_COPY_AND_ASSIGN(NavigationBar);
};

////////////////////////////////////////////////////////////////////////////////
// InfoBubbleContentsView
//
// The view that contains the navigation bar and render view.
// It is displayed in an info-bubble.

class InfoBubbleContentsView : public views::View {
 public:
  explicit InfoBubbleContentsView(AppLauncher* app_launcher);
  ~InfoBubbleContentsView();

  // Sets the initial focus.
  // Should be called when the bubble that contains us is shown.
  void BubbleShown();

  // views::View override:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  // The application launcher displaying this info bubble.
  AppLauncher* app_launcher_;

  // The navigation bar.
  NavigationBar* navigation_bar_;

  // The view containing the renderer view.
  views::NativeViewHost* render_view_container_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubbleContentsView);
};

InfoBubbleContentsView::InfoBubbleContentsView(AppLauncher* app_launcher)
    : app_launcher_(app_launcher),
      navigation_bar_(NULL),
      render_view_container_(NULL) {
}

InfoBubbleContentsView::~InfoBubbleContentsView() {
}

void InfoBubbleContentsView::BubbleShown() {
  navigation_bar_->RequestFocus();
}

void InfoBubbleContentsView::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  if (!is_add || child != this)
    return;

  DCHECK(!render_view_container_);
  render_view_container_ = new RWHVNativeViewHost;
  AddChildView(render_view_container_);
#if defined(OS_WIN)
  RenderWidgetHostViewWin* view_win =
      static_cast<RenderWidgetHostViewWin*>(app_launcher_->rwhv_);
  // Create the HWND now that we are parented.
  HWND hwnd = view_win->Create(GetWidget()->GetNativeView());
  view_win->ShowWindow(SW_SHOW);
#endif
  render_view_container_->Attach(app_launcher_->rwhv_->GetNativeView());

  navigation_bar_ = new NavigationBar(app_launcher_);
  AddChildView(navigation_bar_);
}

gfx::Size InfoBubbleContentsView::GetPreferredSize() {
  gfx::Rect bounds = app_launcher_->browser()->window()->GetRestoredBounds();
  return gfx::Size(bounds.width() * 2 / 3, bounds.width() * 4 / 5);
}

void InfoBubbleContentsView::Layout() {
  if (bounds().IsEmpty() || GetChildViewCount() == 0)
    return;

  int navigation_bar_height =
      kNavigationBarHeight + kNavigationEntryYMargin * 2;
  const views::Border* border = navigation_bar_->border();
  if (border) {
    gfx::Insets insets;
    border->GetInsets(&insets);
    navigation_bar_height += insets.height();
  }
  navigation_bar_->SetBounds(x(), y(), width(), navigation_bar_height);
  int render_y = navigation_bar_->bounds().bottom();
  gfx::Size rwhv_size =
      gfx::Size(width(), std::max(0, height() - render_y + y()));
  render_view_container_->SetBounds(x(), render_y,
                                    rwhv_size.width(), rwhv_size.height());
  app_launcher_->rwhv_->SetSize(rwhv_size);
}

////////////////////////////////////////////////////////////////////////////////
// AppLauncher

AppLauncher::AppLauncher(Browser* browser)
    : browser_(browser),
      info_bubble_(NULL),
      site_instance_(NULL),
      contents_rvh_(NULL),
      rwhv_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          tab_contents_delegate_(new TabContentsDelegateImpl(this))) {
  info_bubble_content_ = new InfoBubbleContentsView(this);

  Profile* profile = browser_->profile();
  int64 session_storage_namespace_id = profile->GetWebKitContext()->
      dom_storage_context()->AllocateSessionStorageNamespaceId();
  site_instance_ = SiteInstance::CreateSiteInstanceForURL(profile,
                                                          GetMenuURL());
  contents_rvh_ = new RenderViewHost(site_instance_, this, MSG_ROUTING_NONE,
                                     session_storage_namespace_id);
  rwhv_ = RenderWidgetHostView::CreateViewForWidget(contents_rvh_);
  contents_rvh_->set_view(rwhv_);

  // On Windows, we'll create the RWHV HWND once we are attached as we need
  // to be parented for CreateWindow to work.
#if defined(OS_LINUX)
  RenderWidgetHostViewGtk* view_gtk =
      static_cast<RenderWidgetHostViewGtk*>(rwhv_);
  view_gtk->InitAsChild();
#endif

  contents_rvh_->CreateRenderView(profile->GetRequestContext());
  DCHECK(contents_rvh_->IsRenderViewLive());
  contents_rvh_->NavigateToURL(GetMenuURL());
}

AppLauncher::~AppLauncher() {
  contents_rvh_->Shutdown();
}

// static
AppLauncher* AppLauncher::Show(Browser* browser) {
  AppLauncher* app_launcher = new AppLauncher(browser);

  BrowserView* browser_view = static_cast<BrowserView*>(browser->window());
  TabStrip* tabstrip = browser_view->tabstrip()->AsTabStrip();
  if (!tabstrip) {
    delete app_launcher;
    return NULL;
  }
  gfx::Rect bounds = tabstrip->GetNewTabButtonBounds();
  gfx::Point origin = bounds.origin();
  views::RootView::ConvertPointToScreen(tabstrip, &origin);
  bounds.set_origin(origin);
  app_launcher->info_bubble_ =
      InfoBubble::Show(browser_view->frame()->GetWindow(), bounds,
                       app_launcher->info_bubble_content_, app_launcher);
  app_launcher->info_bubble_content_->BubbleShown();
  return app_launcher;
}

void AppLauncher::Hide() {
  info_bubble_->Close();
}

void AppLauncher::InfoBubbleClosing(InfoBubble* info_bubble,
                                    bool closed_by_escape) {
  // The stack may have pending_contents_ on it. Delay deleting the
  // pending_contents_ as TabContents doesn't deal well with being deleted
  // while on the stack.
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new DeleteTask<AppLauncher>(this));
}

void AppLauncher::RequestMove(const gfx::Rect& new_bounds) {
#if defined(OS_LINUX)
  // Invoking PositionChild results in a gtk signal that triggers attempting to
  // to resize the window. We need to set the size request so that it resizes
  // correctly when this happens.
  gtk_widget_set_size_request(info_bubble_->GetNativeView(),
                              new_bounds.width(), new_bounds.height());
  info_bubble_->SetBounds(new_bounds);
#endif
}

RendererPreferences AppLauncher::GetRendererPrefs(Profile* profile) const {
  RendererPreferences preferences;
  renderer_preferences_util::UpdateFromSystemSettings(&preferences, profile);
  return preferences;
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
  pending_contents_->set_delegate(tab_contents_delegate_.get());
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
                                WebKit::WebDragOperationsMask allowed_ops,
                                const SkBitmap& image,
                                const gfx::Point& image_offset) {
  // We're not going to do any drag & drop, but we have to tell the renderer the
  // drag & drop ended, othewise the renderer thinks the drag operation is
  // underway and mouse events won't work.
  contents_rvh_->DragSourceSystemDragEnded();
}

void AppLauncher::AddTabWithURL(const GURL& url,
                                PageTransition::Type transition) {
#if defined(OS_CHROMEOS)
  switch (chromeos::StatusAreaView::GetOpenTabsMode()) {
    case chromeos::StatusAreaView::OPEN_TABS_ON_LEFT: {
      // Add the new tab at the first non-pinned location.
      int index = browser_->tabstrip_model()->IndexOfFirstNonMiniTab();
      browser_->AddTabWithURL(url, GURL(), transition,
                              true, index, true, NULL);
      break;
    }
    case chromeos::StatusAreaView::OPEN_TABS_CLOBBER: {
      browser_->GetSelectedTabContents()->controller().LoadURL(
          url, GURL(), transition);
      break;
    }
    case chromeos::StatusAreaView::OPEN_TABS_ON_RIGHT: {
      browser_->AddTabWithURL(url, GURL(), transition, true, -1, true, NULL);
      break;
    }
  }
#else
  browser_->AddTabWithURL(url, GURL(), transition, true, -1, true, NULL);
#endif
}
