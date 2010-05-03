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
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/dom_view.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/location_bar/location_bar_view.h"
#include "chrome/common/url_constants.h"
#include "views/widget/root_view.h"

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

// NavigationBar constants.
const int kNavigationBarHeight = 23;
const int kNavigationBarBorderThickness = 1;

// The delta applied to the default font size for the Omnibox.
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
  return GURL(chrome::kChromeUIAppLauncherURL);
}

}  // namespace

// InfoBubbleContentsView
//
// The view that contains the navigation bar and DOMUI.
// It is displayed in an info-bubble.

class InfoBubbleContentsView : public views::View,
                               public LocationBarView::Delegate,
                               public CommandUpdater::CommandUpdaterDelegate {
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

  // LocationBarView::Delegate implementation:
  virtual TabContents* GetTabContents();
  virtual void OnInputInProgress(bool in_progress) {}

  // CommandUpdater::CommandUpdaterDelegate implementation:
  virtual void ExecuteCommand(int id);

 private:
  // The application launcher displaying this info bubble.
  AppLauncher* app_launcher_;

  // The location bar.
  LocationBarView* location_bar_;

  // The view containing the renderer view.
  DOMView* dom_view_;

  // CommandUpdater the location bar sends commands to.
  CommandUpdater command_updater_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubbleContentsView);
};

InfoBubbleContentsView::InfoBubbleContentsView(AppLauncher* app_launcher)
    : app_launcher_(app_launcher),
      location_bar_(NULL),
      dom_view_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(command_updater_(this)) {
  // Allow the location bar to open URLs.
  command_updater_.UpdateCommandEnabled(IDC_OPEN_CURRENT_URL, true);
  DCHECK(app_launcher);
}

InfoBubbleContentsView::~InfoBubbleContentsView() {
}

void InfoBubbleContentsView::BubbleShown() {
  location_bar_->RequestFocus();
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

  Browser* browser = app_launcher_->browser();
  location_bar_ =  new LocationBarView(browser->profile(),
                                       &command_updater_,
                                       browser->toolbar_model(),
                                       this,
                                       LocationBarView::APP_LAUNCHER);

  location_bar_->set_border(
      views::Border::CreateSolidBorder(1, SkColorSetRGB(205, 201, 201)));
  AddChildView(location_bar_);
  location_bar_->Init();
}

TabContents* InfoBubbleContentsView::GetTabContents() {
  return app_launcher_->browser()->GetSelectedTabContents();
}

gfx::Size InfoBubbleContentsView::GetPreferredSize() {
  gfx::Rect bounds = app_launcher_->browser()->window()->GetRestoredBounds();
  return gfx::Size(bounds.width() * 6 / 7, bounds.height() * 9 / 10);
}

void InfoBubbleContentsView::Layout() {
  if (bounds().IsEmpty() || GetChildViewCount() == 0)
    return;

  gfx::Rect bounds = GetLocalBounds(false);

  location_bar_->SetBounds(bounds.x(), bounds.y(), bounds.width(),
                           location_bar_->GetPreferredSize().height());
  int render_y = location_bar_->bounds().bottom() + kNavigationBarBottomPadding;
  gfx::Size dom_view_size =
      gfx::Size(width(), std::max(0, bounds.height() - render_y + bounds.y()));
  dom_view_->SetBounds(bounds.x(), render_y,
                       dom_view_size.width(), dom_view_size.height());
}

void InfoBubbleContentsView::ExecuteCommand(int id) {
  // The user navigated by typing or selecting an entry in the location bar.
  DCHECK_EQ(IDC_OPEN_CURRENT_URL, id);
  GURL url(WideToUTF8(location_bar_->GetInputString()));
  app_launcher_->AddTabWithURL(url, location_bar_->GetPageTransition());
  app_launcher_->Hide();
}

////////////////////////////////////////////////////////////////////////////////
// AppLauncher

AppLauncher::AppLauncher(Browser* browser)
    : browser_(browser),
      info_bubble_(NULL) {
  DCHECK(browser);
  info_bubble_content_ = new InfoBubbleContentsView(this);
}

AppLauncher::~AppLauncher() {
}

// static
AppLauncher* AppLauncher::Show(Browser* browser,
                               const gfx::Rect& bounds,
                               const gfx::Point& bubble_anchor) {
  AppLauncher* app_launcher = new AppLauncher(browser);
  BrowserView* browser_view = static_cast<BrowserView*>(browser->window());
  app_launcher->info_bubble_ =
      PinnedContentsInfoBubble::Show(browser_view->frame()->GetWindow(),
          bounds, bubble_anchor, app_launcher->info_bubble_content_,
          app_launcher);
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

  // Figure out where the location bar is, so we can pin the bubble to
  // make our url bar appear exactly over it.
  views::RootView* root_view = views::Widget::GetWidgetFromNativeWindow(
      browser_view->GetNativeHandle())->GetRootView();
  views::View* location_bar = root_view->GetViewByID(VIEW_ID_LOCATION_BAR);
  gfx::Point location_bar_origin = location_bar->bounds().origin();
  views::RootView::ConvertPointToScreen(location_bar->GetParent(),
                                        &location_bar_origin);

  return Show(browser, bounds, location_bar_origin);
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
  // The user clicked an item in the app launcher contents.
  AddTabWithURL(url, PageTransition::AUTO_BOOKMARK);
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
      browser_->AddTabWithURL(url, GURL(), transition, index,
                              Browser::ADD_SELECTED | Browser::ADD_FORCE_INDEX,
                              NULL, std::string());
      break;
    }
    case chromeos::StatusAreaView::OPEN_TABS_CLOBBER: {
      browser_->GetSelectedTabContents()->controller().LoadURL(
          url, GURL(), transition);
      break;
    }
    case chromeos::StatusAreaView::OPEN_TABS_ON_RIGHT: {
      browser_->AddTabWithURL(url, GURL(), transition, -1,
                              Browser::ADD_SELECTED | Browser::ADD_FORCE_INDEX,
                              NULL, std::string());
      break;
    }
  }
#else
  browser_->AddTabWithURL(url, GURL(), transition, -1,
                          Browser::ADD_SELECTED | Browser::ADD_FORCE_INDEX,
                          NULL, std::string());
#endif
}
