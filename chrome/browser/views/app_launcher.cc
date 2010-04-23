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
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/dom_view.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/common/url_constants.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include "chrome/browser/autocomplete/autocomplete_edit_view_win.h"
#elif defined(OS_LINUX)
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#endif
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/status/status_area_view.h"
#endif

namespace {

// Padding & margins for the navigation entry.
const int kNavigationEntryPadding = 2;
const int kNavigationEntryXMargin = 3;
const int kNavigationEntryYMargin = 1;

// Padding between the navigation bar and the render view contents.
const int kNavigationBarBottomPadding = 3;

// NavigationBar size.
const int kNavigationBarHeight = 25;

// The delta applied to the default font size for the omnibox.
const int kAutocompleteEditFontDelta = 3;

// Command line switch for specifying url of the page.
// TODO: nuke when we convert to the real app page. Also nuke code in
// AddNewContents
const wchar_t kURLSwitch[] = L"main-menu-url";

// Returns the URL of the menu.
static GURL GetMenuURL() {
  std::wstring url_string =
      CommandLine::ForCurrentProcess()->GetSwitchValue(kURLSwitch);
  if (!url_string.empty())
    return GURL(WideToUTF8(url_string));
  return GURL(chrome::kChromeUINewTabURL);
}

}  // namespace

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
// The view that contains the navigation bar and DOMUI.
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
  DOMView* dom_view_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubbleContentsView);
};

InfoBubbleContentsView::InfoBubbleContentsView(AppLauncher* app_launcher)
    : app_launcher_(app_launcher),
      navigation_bar_(NULL),
      dom_view_(NULL) {
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

  DCHECK(!dom_view_);
  dom_view_ = new DOMView();
  AddChildView(dom_view_);
  // We pass NULL for site instance so the renderer uses its own process.
  dom_view_->Init(app_launcher_->browser()->profile(), NULL);
  // We make the AppLauncher the TabContents delegate so we get notifications
  // from the page to open links.
  dom_view_->tab_contents()->set_delegate(app_launcher_);
  dom_view_->LoadURL(GetMenuURL());

  navigation_bar_ = new NavigationBar(app_launcher_);
  AddChildView(navigation_bar_);
}

gfx::Size InfoBubbleContentsView::GetPreferredSize() {
  gfx::Rect bounds = app_launcher_->browser()->window()->GetRestoredBounds();
  return gfx::Size(bounds.width() * 6 / 7, bounds.height() * 9 / 10);
}

void InfoBubbleContentsView::Layout() {
  if (bounds().IsEmpty() || GetChildViewCount() == 0)
    return;

  gfx::Rect bounds = GetLocalBounds(false);
  int navigation_bar_height =
      kNavigationBarHeight + kNavigationEntryYMargin * 2;
  const views::Border* border = navigation_bar_->border();
  if (border) {
    gfx::Insets insets;
    border->GetInsets(&insets);
    navigation_bar_height += insets.height();
  }
  navigation_bar_->SetBounds(bounds.x(), bounds.y(),
                             bounds.width(), navigation_bar_height);
  int render_y = navigation_bar_->bounds().bottom() +
      kNavigationBarBottomPadding;
  gfx::Size dom_view_size =
      gfx::Size(width(), std::max(0, bounds.height() - render_y + bounds.y()));
  dom_view_->SetBounds(bounds.x(), render_y,
                       dom_view_size.width(), dom_view_size.height());
}

////////////////////////////////////////////////////////////////////////////////
// AppLauncher

AppLauncher::AppLauncher(Browser* browser)
    : browser_(browser),
      info_bubble_(NULL) {
  info_bubble_content_ = new InfoBubbleContentsView(this);
}

AppLauncher::~AppLauncher() {
}

// static
AppLauncher* AppLauncher::Show(Browser* browser, const gfx::Rect& bounds) {
  AppLauncher* app_launcher = new AppLauncher(browser);
  BrowserView* browser_view = static_cast<BrowserView*>(browser->window());
  app_launcher->info_bubble_ =
      InfoBubble::Show(browser_view->frame()->GetWindow(), bounds,
                       app_launcher->info_bubble_content_, app_launcher);
  app_launcher->info_bubble_content_->BubbleShown();
  return app_launcher;
}

// static
AppLauncher* AppLauncher::ShowForNewTab(Browser* browser) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser->window());
  TabStrip* tabstrip = browser_view->tabstrip()->AsTabStrip();
  if (!tabstrip)
    return NULL;
  gfx::Rect bounds = tabstrip->GetNewTabButtonBounds();
  gfx::Point origin = bounds.origin();
  views::RootView::ConvertPointToScreen(tabstrip, &origin);
  bounds.set_origin(origin);
  return Show(browser, bounds);
}

void AppLauncher::Hide() {
  info_bubble_->Close();
}

void AppLauncher::OpenURLFromTab(TabContents* source,
                                 const GURL& url, const GURL& referrer,
                                 WindowOpenDisposition disposition,
                                 PageTransition::Type transition) {
  // TODO(jcivelli): we should call Browser::OpenApplicationTab(), we would need
  //                 to access the app for this URL.
  AddTabWithURL(url, PageTransition::LINK);
  Hide();
}

void AppLauncher::AddNewContents(TabContents* source,
                                 TabContents* new_contents,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) {
#if defined(OS_CHROMEOS)
  // ChromeOS uses the kURLSwitch to specify a page that opens popups. We need
  // to do this so the popups are opened when the user clicks on the page.
  // TODO: nuke when convert to the real app page.
  new_contents->set_delegate(NULL);
  browser_->GetSelectedTabContents()->AddNewContents(
      new_contents, disposition, initial_pos, user_gesture);
  Hide();
#endif
}

void AppLauncher::InfoBubbleClosing(InfoBubble* info_bubble,
                                    bool closed_by_escape) {
  // Delay deleting to be safe (we, and our tabcontents may be on the stack).
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new DeleteTask<AppLauncher>(this));
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
