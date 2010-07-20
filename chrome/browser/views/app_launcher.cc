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
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
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

// Padding between the navigation bar and the render view contents.
const int kNavigationBarBottomPadding = 3;

// NavigationBar constants.
const int kNavigationBarBorderThickness = 1;

// The speed in pixels per milli-second at which the animation should progress.
// It is easier to use a speed than a duration as the contents may report
// several changes in size over-time.
const double kAnimationSpeedPxPerMS = 1.5;

const SkColor kBorderColor = SkColorSetRGB(205, 201, 201);

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

// Returns the location bar view of |browser|.
static views::View* GetBrowserLocationBar(Browser* browser) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser->window());
  views::RootView* root_view = views::Widget::GetWidgetFromNativeWindow(
      browser_view->GetNativeHandle())->GetRootView();
  return root_view->GetViewByID(VIEW_ID_LOCATION_BAR);
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

  // Computes and sets the preferred size for the InfoBubbleContentsView based
  // on the preferred size of the DOMUI contents specified.
  void ComputePreferredSize(const gfx::Size& dom_view_preferred_size);

  // Sets the initial focus.
  // Should be called when the bubble that contains us is shown.
  void BubbleShown();

  // Returns the TabContents displaying the contents for this bubble.
  TabContents* GetBubbleTabContents();

  // views::View override:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // LocationBarView::Delegate implementation:

  // WARNING: this is not the TabContents of the bubble!  Use
  // GetBubbleTabContents() to get the bubble's TabContents.
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

  // The preferred size for this view (at which it fits its contents).
  gfx::Size preferred_size_;

  // CommandUpdater the location bar sends commands to.
  CommandUpdater command_updater_;

  // The width of the browser's location bar.
  int browser_location_bar_width_;

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

  browser_location_bar_width_ =
      GetBrowserLocationBar(app_launcher->browser())->width();
}

InfoBubbleContentsView::~InfoBubbleContentsView() {
}

void InfoBubbleContentsView::ComputePreferredSize(
    const gfx::Size& dom_view_preferred_size) {
  preferred_size_ = dom_view_preferred_size;

  // Add the padding and location bar height.
  preferred_size_.Enlarge(
      0, location_bar_->height() + kNavigationBarBottomPadding);

  // Make sure the width is at least the browser location bar width.
  if (preferred_size_.width() < browser_location_bar_width_)
    preferred_size_.set_width(browser_location_bar_width_);
}

void InfoBubbleContentsView::BubbleShown() {
  location_bar_->RequestFocus();
}

TabContents* InfoBubbleContentsView::GetBubbleTabContents() {
  return dom_view_->tab_contents();
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
  GURL url = GetMenuURL();
  std::string ref = url.ref();
  if (!app_launcher_->hash_params().empty()) {
    if (!ref.empty())
      ref += "&";
    ref += app_launcher_->hash_params();

    url_canon::Replacements<char> replacements;
    replacements.SetRef(ref.c_str(), url_parse::Component(0, ref.size()));
    url = url.ReplaceComponents(replacements);
  }
  dom_view_->LoadURL(url);

  Browser* browser = app_launcher_->browser();
  location_bar_ =  new LocationBarView(browser->profile(),
                                       &command_updater_,
                                       browser->toolbar_model(),
                                       this,
                                       LocationBarView::APP_LAUNCHER);

  location_bar_->set_border(
      views::Border::CreateSolidBorder(kNavigationBarBorderThickness,
                                       kBorderColor));
  AddChildView(location_bar_);
  location_bar_->Init();
  // Size the location to its preferred size so ComputePreferredSize() computes
  // the right size.
  location_bar_->SizeToPreferredSize();
  ComputePreferredSize(gfx::Size(browser_location_bar_width_, 0));
  Layout();
}

TabContents* InfoBubbleContentsView::GetTabContents() {
  return app_launcher_->browser()->GetSelectedTabContents();
}

gfx::Size InfoBubbleContentsView::GetPreferredSize() {
  return preferred_size_;
}

void InfoBubbleContentsView::Layout() {
  if (bounds().IsEmpty() || GetChildViewCount() == 0)
    return;

  gfx::Rect bounds = GetLocalBounds(false);
  // The browser's location bar use a vertical padding that we need to take into
  // account to match its height.
  int location_bar_height =
      location_bar_->GetPreferredSize().height() - LocationBarView::kVertMargin;
  location_bar_->SetBounds(bounds.x(), bounds.y(), bounds.width(),
                           location_bar_height);
  int render_y = location_bar_->bounds().bottom() + kNavigationBarBottomPadding;
  dom_view_->SetBounds(0, render_y,
                       width(), app_launcher_->contents_pref_size_.height());
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
#if defined(OS_WIN)
  animate_ = true;
  animation_.reset(new SlideAnimation(this));
  animation_->SetTweenType(Tween::LINEAR);
#else
  animate_ = false;
#endif
}

AppLauncher::~AppLauncher() {
}

// static
AppLauncher* AppLauncher::Show(Browser* browser,
                               const gfx::Rect& bounds,
                               const gfx::Point& bubble_anchor,
                               const std::string& hash_params) {
  AppLauncher* app_launcher = new AppLauncher(browser);
  BrowserView* browser_view = static_cast<BrowserView*>(browser->window());
  app_launcher->hash_params_ = hash_params;
  app_launcher->info_bubble_ =
      PinnedContentsInfoBubble::Show(browser_view->GetWidget(),
          bounds, BubbleBorder::TOP_LEFT, bubble_anchor,
          app_launcher->info_bubble_content_, app_launcher);
  app_launcher->info_bubble_content_->BubbleShown();

  // TODO(finnur): Change this so that we only fade out when the user launches
  // something from the bubble. This will fade out on dismiss as well.
  app_launcher->info_bubble_->set_fade_away_on_close(true);
  return app_launcher;
}

// static
AppLauncher* AppLauncher::ShowForNewTab(Browser* browser,
                                        const std::string& hash_params) {
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
  views::View* location_bar = GetBrowserLocationBar(browser);
  gfx::Point location_bar_origin = location_bar->bounds().origin();
  views::RootView::ConvertPointToScreen(location_bar->GetParent(),
                                        &location_bar_origin);

  return Show(browser, bounds, location_bar_origin, hash_params);
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

void AppLauncher::UpdatePreferredSize(const gfx::Size& pref_size) {
  if (pref_size.width() == 0 || pref_size.height() == 0)
    return;

  previous_contents_pref_size_ = contents_pref_size_;
  contents_pref_size_ = pref_size;

  if (!animate_) {
    info_bubble_content_->ComputePreferredSize(pref_size);
    info_bubble_->SizeToContents();
    return;
  }

  int original_height = previous_contents_pref_size_.height();
  int new_height = contents_pref_size_.height();
  int new_duration;
  if (animation_->is_animating()) {
    // Modify the animation duration so that the current running animation does
    // not appear janky.
    new_duration = static_cast<int>(new_height / kAnimationSpeedPxPerMS);
  } else {
    // The animation is not running.
    animation_->Reset();  // It may have already been run.
    new_duration = static_cast<int>(abs(new_height - original_height) /
                                    kAnimationSpeedPxPerMS);
  }
  animation_->SetSlideDuration(new_duration);
  animation_->Show();  // No-op if already showing.
}

void AppLauncher::AnimationProgressed(const Animation* animation) {
  gfx::Size contents_size(contents_pref_size_.width(),
      animation->CurrentValueBetween(previous_contents_pref_size_.height(),
                                     contents_pref_size_.height()));
  info_bubble_content_->ComputePreferredSize(contents_size);
  info_bubble_->SizeToContents();
}

void AppLauncher::InfoBubbleClosing(InfoBubble* info_bubble,
                                    bool closed_by_escape) {
  // Delay deleting to be safe (we, and our tabcontents may be on the stack).
  // Remove ourself as a delegate as on GTK the Widget destruction is
  // asynchronous and will happen after the AppLauncher has been deleted (and it
  // might notify us after we have been deleted).
  info_bubble_content_->GetBubbleTabContents()->set_delegate(NULL);
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new DeleteTask<AppLauncher>(this));
}

void AppLauncher::AddTabWithURL(const GURL& url,
                                PageTransition::Type transition) {
  browser_->AddTabWithURL(
      url, GURL(), transition, -1,
      TabStripModel::ADD_SELECTED | TabStripModel::ADD_FORCE_INDEX, NULL,
      std::string());
}

void AppLauncher::Resize(const gfx::Size& contents_size) {
  info_bubble_content_->ComputePreferredSize(contents_size);
  info_bubble_->SizeToContents();
}
