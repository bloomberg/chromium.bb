// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_constants.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/elide_url.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_instructions_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_observer.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_drag_drop_views.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/metrics/metrics_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/drag_utils.h"
#include "ui/views/metrics.h"
#include "ui/views/view_constants.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

using base::UserMetricsAction;
using bookmarks::BookmarkNodeData;
using content::OpenURLParams;
using content::PageNavigator;
using content::Referrer;
using ui::DropTargetEvent;
using views::CustomButton;
using views::LabelButtonBorder;
using views::MenuButton;
using views::View;

// Margins around the content.
static const int kDetachedTopMargin = 1;  // When attached, we use 0 and let the
                                          // toolbar above serve as the margin.
static const int kBottomMargin = 2;
static const int kLeftMargin = 1;
static const int kRightMargin = 1;

// static
const char BookmarkBarView::kViewClassName[] = "BookmarkBarView";

// Padding between buttons.
static const int kButtonPadding = 0;

// Icon to display when one isn't found for the page.
static gfx::ImageSkia* kDefaultFavicon = NULL;

// Icon used for folders.
static gfx::ImageSkia* kFolderIcon = NULL;

// Color of the drop indicator.
static const SkColor kDropIndicatorColor = SK_ColorBLACK;

// Width of the drop indicator.
static const int kDropIndicatorWidth = 2;

// Distance between the bottom of the bar and the separator.
static const int kSeparatorMargin = 1;

// Width of the separator between the recently bookmarked button and the
// overflow indicator.
static const int kSeparatorWidth = 4;

// Starting x-coordinate of the separator line within a separator.
static const int kSeparatorStartX = 2;

// Left-padding for the instructional text.
static const int kInstructionsPadding = 6;

// Tag for the 'Other bookmarks' button.
static const int kOtherFolderButtonTag = 1;

// Tag for the 'Apps Shortcut' button.
static const int kAppsShortcutButtonTag = 2;

// Preferred padding between text and edge.
static const int kButtonPaddingHorizontal = 6;
static const int kButtonPaddingVertical = 4;

// Tag for the 'Managed bookmarks' button.
static const int kManagedFolderButtonTag = 3;

#if !defined(OS_WIN)
static const gfx::ElideBehavior kElideBehavior = gfx::FADE_TAIL;
#else
// Windows fade eliding causes text to darken; see http://crbug.com/388084
static const gfx::ElideBehavior kElideBehavior = gfx::ELIDE_TAIL;
#endif

namespace {

// To enable/disable BookmarkBar animations during testing. In production
// animations are enabled by default.
bool animations_enabled = true;

// BookmarkButtonBase -----------------------------------------------

// Base class for buttons used on the bookmark bar.

class BookmarkButtonBase : public views::LabelButton {
 public:
  BookmarkButtonBase(views::ButtonListener* listener,
                     const base::string16& title)
      : LabelButton(listener, title) {
    SetElideBehavior(kElideBehavior);
    show_animation_.reset(new gfx::SlideAnimation(this));
    if (!animations_enabled) {
      // For some reason during testing the events generated by animating
      // throw off the test. So, don't animate while testing.
      show_animation_->Reset(1);
    } else {
      show_animation_->Show();
    }
  }

  virtual View* GetTooltipHandlerForPoint(const gfx::Point& point) OVERRIDE {
    return HitTestPoint(point) && CanProcessEventsWithinSubtree() ? this : NULL;
  }

  virtual scoped_ptr<LabelButtonBorder> CreateDefaultBorder() const OVERRIDE {
    scoped_ptr<LabelButtonBorder> border = LabelButton::CreateDefaultBorder();
    border->set_insets(gfx::Insets(kButtonPaddingVertical,
                                   kButtonPaddingHorizontal,
                                   kButtonPaddingVertical,
                                   kButtonPaddingHorizontal));
    return border.Pass();
  }

  virtual bool IsTriggerableEvent(const ui::Event& e) OVERRIDE {
    return e.type() == ui::ET_GESTURE_TAP ||
           e.type() == ui::ET_GESTURE_TAP_DOWN ||
           event_utils::IsPossibleDispositionEvent(e);
  }

 private:
  scoped_ptr<gfx::SlideAnimation> show_animation_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkButtonBase);
};

// BookmarkButton -------------------------------------------------------------

// Buttons used for the bookmarks on the bookmark bar.

class BookmarkButton : public BookmarkButtonBase {
 public:
  // The internal view class name.
  static const char kViewClassName[];

  BookmarkButton(views::ButtonListener* listener,
                 const GURL& url,
                 const base::string16& title,
                 Profile* profile)
      : BookmarkButtonBase(listener, title),
        url_(url),
        profile_(profile) {
  }

  virtual bool GetTooltipText(const gfx::Point& p,
                              base::string16* tooltip) const OVERRIDE {
    gfx::Point location(p);
    ConvertPointToScreen(this, &location);
    *tooltip = BookmarkBarView::CreateToolTipForURLAndTitle(
        GetWidget(), location, url_, GetText(), profile_);
    return !tooltip->empty();
  }

  virtual const char* GetClassName() const OVERRIDE {
    return kViewClassName;
  }

 private:
  const GURL& url_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkButton);
};

// static
const char BookmarkButton::kViewClassName[] = "BookmarkButton";

// ShortcutButton -------------------------------------------------------------

// Buttons used for the shortcuts on the bookmark bar.

class ShortcutButton : public BookmarkButtonBase {
 public:
  // The internal view class name.
  static const char kViewClassName[];

  ShortcutButton(views::ButtonListener* listener,
                 const base::string16& title)
      : BookmarkButtonBase(listener, title) {
  }

  virtual const char* GetClassName() const OVERRIDE {
    return kViewClassName;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShortcutButton);
};

// static
const char ShortcutButton::kViewClassName[] = "ShortcutButton";

// BookmarkFolderButton -------------------------------------------------------

// Buttons used for folders on the bookmark bar, including the 'other folders'
// button.
class BookmarkFolderButton : public views::MenuButton {
 public:
  BookmarkFolderButton(views::ButtonListener* listener,
                       const base::string16& title,
                       views::MenuButtonListener* menu_button_listener,
                       bool show_menu_marker)
      : MenuButton(listener, title, menu_button_listener, show_menu_marker) {
    SetElideBehavior(kElideBehavior);
    show_animation_.reset(new gfx::SlideAnimation(this));
    if (!animations_enabled) {
      // For some reason during testing the events generated by animating
      // throw off the test. So, don't animate while testing.
      show_animation_->Reset(1);
    } else {
      show_animation_->Show();
    }
  }

  virtual bool GetTooltipText(const gfx::Point& p,
                              base::string16* tooltip) const OVERRIDE {
    if (label()->GetPreferredSize().width() > label()->size().width())
      *tooltip = GetText();
    return !tooltip->empty();
  }

  virtual bool IsTriggerableEvent(const ui::Event& e) OVERRIDE {
    // Left clicks and taps should show the menu contents and right clicks
    // should show the context menu. They should not trigger the opening of
    // underlying urls.
    if (e.type() == ui::ET_GESTURE_TAP ||
        (e.IsMouseEvent() && (e.flags() &
             (ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON))))
      return false;

    if (e.IsMouseEvent())
      return ui::DispositionFromEventFlags(e.flags()) != CURRENT_TAB;
    return false;
  }

 private:
  scoped_ptr<gfx::SlideAnimation> show_animation_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkFolderButton);
};

// OverFlowButton (chevron) --------------------------------------------------

class OverFlowButton : public views::MenuButton {
 public:
  explicit OverFlowButton(BookmarkBarView* owner)
      : MenuButton(NULL, base::string16(), owner, false),
        owner_(owner) {}

  virtual bool OnMousePressed(const ui::MouseEvent& e) OVERRIDE {
    owner_->StopThrobbing(true);
    return views::MenuButton::OnMousePressed(e);
  }

 private:
  BookmarkBarView* owner_;

  DISALLOW_COPY_AND_ASSIGN(OverFlowButton);
};

void RecordAppLaunch(Profile* profile, GURL url) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)
          ->enabled_extensions().GetAppByURL(url);
  if (!extension)
    return;

  CoreAppLauncherHandler::RecordAppLaunchType(
      extension_misc::APP_LAUNCH_BOOKMARK_BAR,
      extension->GetType());
}

}  // namespace

// DropLocation ---------------------------------------------------------------

struct BookmarkBarView::DropLocation {
  DropLocation()
      : index(-1),
        operation(ui::DragDropTypes::DRAG_NONE),
        on(false),
        button_type(DROP_BOOKMARK) {
  }

  bool Equals(const DropLocation& other) {
    return ((other.index == index) && (other.on == on) &&
            (other.button_type == button_type));
  }

  // Index into the model the drop is over. This is relative to the root node.
  int index;

  // Drop constants.
  int operation;

  // If true, the user is dropping on a folder.
  bool on;

  // Type of button.
  DropButtonType button_type;
};

// DropInfo -------------------------------------------------------------------

// Tracks drops on the BookmarkBarView.

struct BookmarkBarView::DropInfo {
  DropInfo()
      : valid(false),
        is_menu_showing(false),
        x(0),
        y(0) {
  }

  // Whether the data is valid.
  bool valid;

  // If true, the menu is being shown.
  bool is_menu_showing;

  // Coordinates of the drag (in terms of the BookmarkBarView).
  int x;
  int y;

  // DropData for the drop.
  BookmarkNodeData data;

  DropLocation location;
};

// ButtonSeparatorView  --------------------------------------------------------

class BookmarkBarView::ButtonSeparatorView : public views::View {
 public:
  ButtonSeparatorView() {}
  virtual ~ButtonSeparatorView() {}

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    DetachableToolbarView::PaintVerticalDivider(
        canvas, kSeparatorStartX, height(), 1,
        DetachableToolbarView::kEdgeDividerColor,
        DetachableToolbarView::kMiddleDividerColor,
        GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR));
  }

  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    // We get the full height of the bookmark bar, so that the height returned
    // here doesn't matter.
    return gfx::Size(kSeparatorWidth, 1);
  }

  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE {
    state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_SEPARATOR);
    state->role = ui::AX_ROLE_SPLITTER;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ButtonSeparatorView);
};

// BookmarkBarView ------------------------------------------------------------

// static
const int BookmarkBarView::kMaxButtonWidth = 150;
const int BookmarkBarView::kNewtabHorizontalPadding = 2;
const int BookmarkBarView::kToolbarAttachedBookmarkBarOverlap = 3;

const gfx::ImageSkia& GetDefaultFavicon() {
  if (!kDefaultFavicon) {
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    kDefaultFavicon = rb->GetImageSkiaNamed(IDR_DEFAULT_FAVICON);
  }
  return *kDefaultFavicon;
}

const gfx::ImageSkia& GetFolderIcon() {
  if (!kFolderIcon) {
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    kFolderIcon = rb->GetImageSkiaNamed(IDR_BOOKMARK_BAR_FOLDER);
  }
  return *kFolderIcon;
}

BookmarkBarView::BookmarkBarView(Browser* browser, BrowserView* browser_view)
    : page_navigator_(NULL),
      client_(NULL),
      bookmark_menu_(NULL),
      bookmark_drop_menu_(NULL),
      other_bookmarked_button_(NULL),
      managed_bookmarks_button_(NULL),
      apps_page_shortcut_(NULL),
      overflow_button_(NULL),
      instructions_(NULL),
      bookmarks_separator_view_(NULL),
      browser_(browser),
      browser_view_(browser_view),
      infobar_visible_(false),
      throbbing_view_(NULL),
      bookmark_bar_state_(BookmarkBar::SHOW),
      animating_detached_(false),
      show_folder_method_factory_(this) {
  set_id(VIEW_ID_BOOKMARK_BAR);
  Init();

  size_animation_->Reset(1);
}

BookmarkBarView::~BookmarkBarView() {
  if (model_)
    model_->RemoveObserver(this);

  // It's possible for the menu to outlive us, reset the observer to make sure
  // it doesn't have a reference to us.
  if (bookmark_menu_) {
    bookmark_menu_->set_observer(NULL);
    bookmark_menu_->SetPageNavigator(NULL);
    bookmark_menu_->clear_bookmark_bar();
  }
  if (context_menu_.get())
    context_menu_->SetPageNavigator(NULL);

  StopShowFolderDropMenuTimer();
}

// static
void BookmarkBarView::DisableAnimationsForTesting(bool disabled) {
  animations_enabled = !disabled;
}

void BookmarkBarView::AddObserver(BookmarkBarViewObserver* observer) {
  observers_.AddObserver(observer);
}

void BookmarkBarView::RemoveObserver(BookmarkBarViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BookmarkBarView::SetPageNavigator(PageNavigator* navigator) {
  page_navigator_ = navigator;
  if (bookmark_menu_)
    bookmark_menu_->SetPageNavigator(navigator);
  if (context_menu_.get())
    context_menu_->SetPageNavigator(navigator);
}

void BookmarkBarView::SetBookmarkBarState(
    BookmarkBar::State state,
    BookmarkBar::AnimateChangeType animate_type) {
  if (animate_type == BookmarkBar::ANIMATE_STATE_CHANGE &&
      animations_enabled) {
    animating_detached_ = (state == BookmarkBar::DETACHED ||
                           bookmark_bar_state_ == BookmarkBar::DETACHED);
    if (state == BookmarkBar::SHOW)
      size_animation_->Show();
    else
      size_animation_->Hide();
  } else {
    size_animation_->Reset(state == BookmarkBar::SHOW ? 1 : 0);
  }
  bookmark_bar_state_ = state;
}

int BookmarkBarView::GetFullyDetachedToolbarOverlap() const {
  if (!infobar_visible_ && browser_->window()->IsFullscreen()) {
    // There is no client edge to overlap when detached in fullscreen with no
    // infobars visible.
    return 0;
  }
  return views::NonClientFrameView::kClientEdgeThickness;
}

bool BookmarkBarView::is_animating() {
  return size_animation_->is_animating();
}

const BookmarkNode* BookmarkBarView::GetNodeForButtonAtModelIndex(
    const gfx::Point& loc,
    int* model_start_index) {
  *model_start_index = 0;

  if (loc.x() < 0 || loc.x() >= width() || loc.y() < 0 || loc.y() >= height())
    return NULL;

  gfx::Point adjusted_loc(GetMirroredXInView(loc.x()), loc.y());

  // Check the managed button first.
  if (managed_bookmarks_button_->visible() &&
      managed_bookmarks_button_->bounds().Contains(adjusted_loc)) {
    return client_->managed_node();
  }

  // Then check the bookmark buttons.
  for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      break;
    if (child->bounds().Contains(adjusted_loc))
      return model_->bookmark_bar_node()->GetChild(i);
  }

  // Then the overflow button.
  if (overflow_button_->visible() &&
      overflow_button_->bounds().Contains(adjusted_loc)) {
    *model_start_index = GetFirstHiddenNodeIndex();
    return model_->bookmark_bar_node();
  }

  // And finally the other folder.
  if (other_bookmarked_button_->visible() &&
      other_bookmarked_button_->bounds().Contains(adjusted_loc)) {
    return model_->other_node();
  }

  return NULL;
}

views::MenuButton* BookmarkBarView::GetMenuButtonForNode(
    const BookmarkNode* node) {
  if (node == client_->managed_node())
    return managed_bookmarks_button_;
  if (node == model_->other_node())
    return other_bookmarked_button_;
  if (node == model_->bookmark_bar_node())
    return overflow_button_;
  int index = model_->bookmark_bar_node()->GetIndexOf(node);
  if (index == -1 || !node->is_folder())
    return NULL;
  return static_cast<views::MenuButton*>(child_at(index));
}

void BookmarkBarView::GetAnchorPositionForButton(
    views::MenuButton* button,
    views::MenuAnchorPosition* anchor) {
  if (button == other_bookmarked_button_ || button == overflow_button_)
    *anchor = views::MENU_ANCHOR_TOPRIGHT;
  else
    *anchor = views::MENU_ANCHOR_TOPLEFT;
}

views::MenuItemView* BookmarkBarView::GetMenu() {
  return bookmark_menu_ ? bookmark_menu_->menu() : NULL;
}

views::MenuItemView* BookmarkBarView::GetContextMenu() {
  return bookmark_menu_ ? bookmark_menu_->context_menu() : NULL;
}

views::MenuItemView* BookmarkBarView::GetDropMenu() {
  return bookmark_drop_menu_ ? bookmark_drop_menu_->menu() : NULL;
}

void BookmarkBarView::StopThrobbing(bool immediate) {
  if (!throbbing_view_)
    return;

  // If not immediate, cycle through 2 more complete cycles.
  throbbing_view_->StartThrobbing(immediate ? 0 : 4);
  throbbing_view_ = NULL;
}

// static
base::string16 BookmarkBarView::CreateToolTipForURLAndTitle(
    const views::Widget* widget,
    const gfx::Point& screen_loc,
    const GURL& url,
    const base::string16& title,
    Profile* profile) {
  int max_width = views::TooltipManager::GetMaxWidth(
      screen_loc.x(),
      screen_loc.y(),
      widget->GetNativeView());
  const gfx::FontList tt_fonts = widget->GetTooltipManager()->GetFontList();
  base::string16 result;

  // First the title.
  if (!title.empty()) {
    base::string16 localized_title = title;
    base::i18n::AdjustStringForLocaleDirection(&localized_title);
    result.append(gfx::ElideText(localized_title, tt_fonts, max_width,
                                 gfx::ELIDE_TAIL));
  }

  // Only show the URL if the url and title differ.
  if (title != base::UTF8ToUTF16(url.spec())) {
    if (!result.empty())
      result.push_back('\n');

    // We need to explicitly specify the directionality of the URL's text to
    // make sure it is treated as an LTR string when the context is RTL. For
    // example, the URL "http://www.yahoo.com/" appears as
    // "/http://www.yahoo.com" when rendered, as is, in an RTL context since
    // the Unicode BiDi algorithm puts certain characters on the left by
    // default.
    std::string languages = profile->GetPrefs()->GetString(
        prefs::kAcceptLanguages);
    base::string16 elided_url(ElideUrl(url, tt_fonts, max_width, languages));
    elided_url = base::i18n::GetDisplayStringInLTRDirectionality(elided_url);
    result.append(elided_url);
  }
  return result;
}

bool BookmarkBarView::IsDetached() const {
  return (bookmark_bar_state_ == BookmarkBar::DETACHED) ||
      (animating_detached_ && size_animation_->is_animating());
}

double BookmarkBarView::GetAnimationValue() const {
  return size_animation_->GetCurrentValue();
}

int BookmarkBarView::GetToolbarOverlap() const {
  int attached_overlap = kToolbarAttachedBookmarkBarOverlap +
      views::NonClientFrameView::kClientEdgeThickness;
  if (!IsDetached())
    return attached_overlap;

  int detached_overlap = GetFullyDetachedToolbarOverlap();

  // Do not animate the overlap when the infobar is above us (i.e. when we're
  // detached), since drawing over the infobar looks weird.
  if (infobar_visible_)
    return detached_overlap;

  // When detached with no infobar, animate the overlap between the attached and
  // detached states.
  return detached_overlap + static_cast<int>(
      (attached_overlap - detached_overlap) *
          size_animation_->GetCurrentValue());
}

gfx::Size BookmarkBarView::GetPreferredSize() const {
  gfx::Size prefsize;
  if (IsDetached()) {
    prefsize.set_height(
        chrome::kBookmarkBarHeight +
        static_cast<int>(
            (chrome::kNTPBookmarkBarHeight - chrome::kBookmarkBarHeight) *
            (1 - size_animation_->GetCurrentValue())));
  } else {
    prefsize.set_height(static_cast<int>(chrome::kBookmarkBarHeight *
                                         size_animation_->GetCurrentValue()));
  }
  return prefsize;
}

bool BookmarkBarView::CanProcessEventsWithinSubtree() const {
  // If the bookmark bar is attached and the omnibox popup is open (on top of
  // the bar), prevent events from targeting the bookmark bar or any of its
  // descendants. This will prevent hovers/clicks just above the omnibox popup
  // from activating the top few pixels of items on the bookmark bar.
  if (!IsDetached() && browser_view_ &&
      browser_view_->GetLocationBar()->GetOmniboxView()->model()->
          popup_model()->IsOpen()) {
    return false;
  }
  return true;
}

gfx::Size BookmarkBarView::GetMinimumSize() const {
  // The minimum width of the bookmark bar should at least contain the overflow
  // button, by which one can access all the Bookmark Bar items, and the "Other
  // Bookmarks" folder, along with appropriate margins and button padding.
  // It should also contain the Managed Bookmarks folder, if it's visible.
  int width = kLeftMargin;

  int height = chrome::kBookmarkBarHeight;
  if (IsDetached()) {
    double current_state = 1 - size_animation_->GetCurrentValue();
    width += 2 * static_cast<int>(kNewtabHorizontalPadding * current_state);
    height += static_cast<int>(
        (chrome::kNTPBookmarkBarHeight - chrome::kBookmarkBarHeight) *
            current_state);
  }

  if (managed_bookmarks_button_->visible()) {
    gfx::Size size = managed_bookmarks_button_->GetPreferredSize();
    width += size.width() + kButtonPadding;
  }
  if (other_bookmarked_button_->visible()) {
    gfx::Size size = other_bookmarked_button_->GetPreferredSize();
    width += size.width() + kButtonPadding;
  }
  if (overflow_button_->visible()) {
    gfx::Size size = overflow_button_->GetPreferredSize();
    width += size.width() + kButtonPadding;
  }
  if (bookmarks_separator_view_->visible()) {
    gfx::Size size = bookmarks_separator_view_->GetPreferredSize();
    width += size.width();
  }
  if (apps_page_shortcut_->visible()) {
    gfx::Size size = apps_page_shortcut_->GetPreferredSize();
    width += size.width() + kButtonPadding;
  }

  return gfx::Size(width, height);
}

void BookmarkBarView::Layout() {
  LayoutItems();
}

void BookmarkBarView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    // We may get inserted into a hierarchy with a profile - this typically
    // occurs when the bar's contents get populated fast enough that the
    // buttons are created before the bar is attached to a frame.
    UpdateColors();

    if (height() > 0) {
      // We only layout while parented. When we become parented, if our bounds
      // haven't changed, OnBoundsChanged() won't get invoked and we won't
      // layout. Therefore we always force a layout when added.
      Layout();
    }
  }
}

void BookmarkBarView::PaintChildren(gfx::Canvas* canvas,
                                    const views::CullSet& cull_set) {
  View::PaintChildren(canvas, cull_set);

  if (drop_info_.get() && drop_info_->valid &&
      drop_info_->location.operation != 0 && drop_info_->location.index != -1 &&
      drop_info_->location.button_type != DROP_OVERFLOW &&
      !drop_info_->location.on) {
    int index = drop_info_->location.index;
    DCHECK(index <= GetBookmarkButtonCount());
    int x = 0;
    int y = 0;
    int h = height();
    if (index == GetBookmarkButtonCount()) {
      if (index == 0) {
        x = kLeftMargin;
      } else {
        x = GetBookmarkButton(index - 1)->x() +
            GetBookmarkButton(index - 1)->width();
      }
    } else {
      x = GetBookmarkButton(index)->x();
    }
    if (GetBookmarkButtonCount() > 0 && GetBookmarkButton(0)->visible()) {
      y = GetBookmarkButton(0)->y();
      h = GetBookmarkButton(0)->height();
    }

    // Since the drop indicator is painted directly onto the canvas, we must
    // make sure it is painted in the right location if the locale is RTL.
    gfx::Rect indicator_bounds(x - kDropIndicatorWidth / 2,
                               y,
                               kDropIndicatorWidth,
                               h);
    indicator_bounds.set_x(GetMirroredXForRect(indicator_bounds));

    // TODO(sky/glen): make me pretty!
    canvas->FillRect(indicator_bounds, kDropIndicatorColor);
  }
}

bool BookmarkBarView::GetDropFormats(
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) {
  if (!model_ || !model_->loaded())
    return false;
  *formats = ui::OSExchangeData::URL;
  custom_formats->insert(BookmarkNodeData::GetBookmarkCustomFormat());
  return true;
}

bool BookmarkBarView::AreDropTypesRequired() {
  return true;
}

bool BookmarkBarView::CanDrop(const ui::OSExchangeData& data) {
  if (!model_ || !model_->loaded() ||
      !browser_->profile()->GetPrefs()->GetBoolean(
          bookmarks::prefs::kEditBookmarksEnabled))
    return false;

  if (!drop_info_.get())
    drop_info_.reset(new DropInfo());

  // Only accept drops of 1 node, which is the case for all data dragged from
  // bookmark bar and menus.
  return drop_info_->data.Read(data) && drop_info_->data.size() == 1;
}

void BookmarkBarView::OnDragEntered(const DropTargetEvent& event) {
}

int BookmarkBarView::OnDragUpdated(const DropTargetEvent& event) {
  if (!drop_info_.get())
    return 0;

  if (drop_info_->valid &&
      (drop_info_->x == event.x() && drop_info_->y == event.y())) {
    // The location of the mouse didn't change, return the last operation.
    return drop_info_->location.operation;
  }

  drop_info_->x = event.x();
  drop_info_->y = event.y();

  DropLocation location;
  CalculateDropLocation(event, drop_info_->data, &location);

  if (drop_info_->valid && drop_info_->location.Equals(location)) {
    // The position we're going to drop didn't change, return the last drag
    // operation we calculated. Copy of the operation in case it changed.
    drop_info_->location.operation = location.operation;
    return drop_info_->location.operation;
  }

  StopShowFolderDropMenuTimer();

  // TODO(sky): Optimize paint region.
  SchedulePaint();

  drop_info_->location = location;
  drop_info_->valid = true;

  if (drop_info_->is_menu_showing) {
    if (bookmark_drop_menu_)
      bookmark_drop_menu_->Cancel();
    drop_info_->is_menu_showing = false;
  }

  if (location.on || location.button_type == DROP_OVERFLOW ||
      location.button_type == DROP_OTHER_FOLDER) {
    const BookmarkNode* node;
    if (location.button_type == DROP_OTHER_FOLDER)
      node = model_->other_node();
    else if (location.button_type == DROP_OVERFLOW)
      node = model_->bookmark_bar_node();
    else
      node = model_->bookmark_bar_node()->GetChild(location.index);
    StartShowFolderDropMenuTimer(node);
  }

  return drop_info_->location.operation;
}

void BookmarkBarView::OnDragExited() {
  StopShowFolderDropMenuTimer();

  // NOTE: we don't hide the menu on exit as it's possible the user moved the
  // mouse over the menu, which triggers an exit on us.

  drop_info_->valid = false;

  if (drop_info_->location.index != -1) {
    // TODO(sky): optimize the paint region.
    SchedulePaint();
  }
  drop_info_.reset();
}

int BookmarkBarView::OnPerformDrop(const DropTargetEvent& event) {
  StopShowFolderDropMenuTimer();

  if (bookmark_drop_menu_)
    bookmark_drop_menu_->Cancel();

  if (!drop_info_.get() || !drop_info_->location.operation)
    return ui::DragDropTypes::DRAG_NONE;

  const BookmarkNode* root =
      (drop_info_->location.button_type == DROP_OTHER_FOLDER) ?
      model_->other_node() : model_->bookmark_bar_node();
  int index = drop_info_->location.index;

  if (index != -1) {
    // TODO(sky): optimize the SchedulePaint region.
    SchedulePaint();
  }
  const BookmarkNode* parent_node;
  if (drop_info_->location.button_type == DROP_OTHER_FOLDER) {
    parent_node = root;
    index = parent_node->child_count();
  } else if (drop_info_->location.on) {
    parent_node = root->GetChild(index);
    index = parent_node->child_count();
  } else {
    parent_node = root;
  }
  const BookmarkNodeData data = drop_info_->data;
  DCHECK(data.is_valid());
  bool copy = drop_info_->location.operation == ui::DragDropTypes::DRAG_COPY;
  drop_info_.reset();
  return chrome::DropBookmarks(
      browser_->profile(), data, parent_node, index, copy);
}

void BookmarkBarView::OnThemeChanged() {
  UpdateColors();
}

const char* BookmarkBarView::GetClassName() const {
  return kViewClassName;
}

void BookmarkBarView::SetVisible(bool v) {
  if (v == visible())
    return;

  View::SetVisible(v);
  FOR_EACH_OBSERVER(BookmarkBarViewObserver, observers_,
                    OnBookmarkBarVisibilityChanged());
}

void BookmarkBarView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_TOOLBAR;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_BOOKMARKS);
}

void BookmarkBarView::AnimationProgressed(const gfx::Animation* animation) {
  // |browser_view_| can be NULL during tests.
  if (browser_view_)
    browser_view_->ToolbarSizeChanged(true);
}

void BookmarkBarView::AnimationEnded(const gfx::Animation* animation) {
  // |browser_view_| can be NULL during tests.
  if (browser_view_) {
    browser_view_->ToolbarSizeChanged(false);
    SchedulePaint();
  }
}

void BookmarkBarView::BookmarkMenuControllerDeleted(
    BookmarkMenuController* controller) {
  if (controller == bookmark_menu_)
    bookmark_menu_ = NULL;
  else if (controller == bookmark_drop_menu_)
    bookmark_drop_menu_ = NULL;
}

void BookmarkBarView::ShowImportDialog() {
  int64 install_time = g_browser_process->metrics_service()->GetInstallDate();
  int64 time_from_install = base::Time::Now().ToTimeT() - install_time;
  if (bookmark_bar_state_ == BookmarkBar::SHOW) {
    UMA_HISTOGRAM_COUNTS("Import.ShowDialog.FromBookmarkBarView",
                         time_from_install);
  } else if (bookmark_bar_state_ == BookmarkBar::DETACHED) {
    UMA_HISTOGRAM_COUNTS("Import.ShowDialog.FromFloatingBookmarkBarView",
                         time_from_install);
  }

  chrome::ShowImportDialog(browser_);
}

void BookmarkBarView::OnBookmarkBubbleShown(const GURL& url) {
  StopThrobbing(true);
  const BookmarkNode* node = model_->GetMostRecentlyAddedUserNodeForURL(url);
  if (!node)
    return;  // Generally shouldn't happen.
  StartThrobbing(node, false);
}

void BookmarkBarView::OnBookmarkBubbleHidden() {
  StopThrobbing(false);
}

void BookmarkBarView::BookmarkModelLoaded(BookmarkModel* model,
                                          bool ids_reassigned) {
  // There should be no buttons. If non-zero it means Load was invoked more than
  // once, or we didn't properly clear things. Either of which shouldn't happen.
  DCHECK_EQ(0, GetBookmarkButtonCount());
  const BookmarkNode* node = model->bookmark_bar_node();
  DCHECK(node);
  // Create a button for each of the children on the bookmark bar.
  for (int i = 0, child_count = node->child_count(); i < child_count; ++i)
    AddChildViewAt(CreateBookmarkButton(node->GetChild(i)), i);
  DCHECK(model->other_node());
  other_bookmarked_button_->SetAccessibleName(model->other_node()->GetTitle());
  other_bookmarked_button_->SetText(model->other_node()->GetTitle());
  managed_bookmarks_button_->SetAccessibleName(
      client_->managed_node()->GetTitle());
  managed_bookmarks_button_->SetText(client_->managed_node()->GetTitle());
  UpdateColors();
  UpdateButtonsVisibility();
  other_bookmarked_button_->SetEnabled(true);
  managed_bookmarks_button_->SetEnabled(true);

  Layout();
  SchedulePaint();
}

void BookmarkBarView::BookmarkModelBeingDeleted(BookmarkModel* model) {
  NOTREACHED();
  // Do minimal cleanup, presumably we'll be deleted shortly.
  model_->RemoveObserver(this);
  model_ = NULL;
}

void BookmarkBarView::BookmarkNodeMoved(BookmarkModel* model,
                                        const BookmarkNode* old_parent,
                                        int old_index,
                                        const BookmarkNode* new_parent,
                                        int new_index) {
  bool was_throbbing = throbbing_view_ &&
      throbbing_view_ == DetermineViewToThrobFromRemove(old_parent, old_index);
  if (was_throbbing)
    throbbing_view_->StopThrobbing();
  BookmarkNodeRemovedImpl(model, old_parent, old_index);
  BookmarkNodeAddedImpl(model, new_parent, new_index);
  if (was_throbbing)
    StartThrobbing(new_parent->GetChild(new_index), false);
}

void BookmarkBarView::BookmarkNodeAdded(BookmarkModel* model,
                                        const BookmarkNode* parent,
                                        int index) {
  BookmarkNodeAddedImpl(model, parent, index);
}

void BookmarkBarView::BookmarkNodeRemoved(BookmarkModel* model,
                                          const BookmarkNode* parent,
                                          int old_index,
                                          const BookmarkNode* node,
                                          const std::set<GURL>& removed_urls) {
  // Close the menu if the menu is showing for the deleted node.
  if (bookmark_menu_ && bookmark_menu_->node() == node)
    bookmark_menu_->Cancel();
  BookmarkNodeRemovedImpl(model, parent, old_index);
}

void BookmarkBarView::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  UpdateButtonsVisibility();

  StopThrobbing(true);

  // Remove the existing buttons.
  while (GetBookmarkButtonCount()) {
    delete GetBookmarkButton(0);
  }

  Layout();
  SchedulePaint();
}

void BookmarkBarView::BookmarkNodeChanged(BookmarkModel* model,
                                          const BookmarkNode* node) {
  BookmarkNodeChangedImpl(model, node);
}

void BookmarkBarView::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  if (node != model->bookmark_bar_node())
    return;  // We only care about reordering of the bookmark bar node.

  // Remove the existing buttons.
  while (GetBookmarkButtonCount()) {
    views::View* button = child_at(0);
    RemoveChildView(button);
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, button);
  }

  // Create the new buttons.
  for (int i = 0, child_count = node->child_count(); i < child_count; ++i)
    AddChildViewAt(CreateBookmarkButton(node->GetChild(i)), i);
  UpdateColors();

  Layout();
  SchedulePaint();
}

void BookmarkBarView::BookmarkNodeFaviconChanged(BookmarkModel* model,
                                                 const BookmarkNode* node) {
  BookmarkNodeChangedImpl(model, node);
}

void BookmarkBarView::WriteDragDataForView(View* sender,
                                           const gfx::Point& press_pt,
                                           ui::OSExchangeData* data) {
  content::RecordAction(UserMetricsAction("BookmarkBar_DragButton"));

  for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
    if (sender == GetBookmarkButton(i)) {
      views::LabelButton* button = GetBookmarkButton(i);
      const BookmarkNode* node = model_->bookmark_bar_node()->GetChild(i);

      const gfx::Image& image_from_model = model_->GetFavicon(node);
      const gfx::ImageSkia& icon = image_from_model.IsEmpty() ?
          (node->is_folder() ? GetFolderIcon() : GetDefaultFavicon()) :
          *image_from_model.ToImageSkia();

      button_drag_utils::SetDragImage(
          node->url(),
          node->GetTitle(),
          icon,
          &press_pt,
          data,
          button->GetWidget());
      WriteBookmarkDragData(model_->bookmark_bar_node()->GetChild(i), data);
      return;
    }
  }
  NOTREACHED();
}

int BookmarkBarView::GetDragOperationsForView(View* sender,
                                              const gfx::Point& p) {
  if (size_animation_->is_animating() ||
      (size_animation_->GetCurrentValue() == 0 &&
       bookmark_bar_state_ != BookmarkBar::DETACHED)) {
    // Don't let the user drag while animating open or we're closed (and not
    // detached, when detached size_animation_ is always 0). This typically is
    // only hit if the user does something to inadvertently trigger DnD such as
    // pressing the mouse and hitting control-b.
    return ui::DragDropTypes::DRAG_NONE;
  }

  for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
    if (sender == GetBookmarkButton(i)) {
      return chrome::GetBookmarkDragOperation(
          browser_->profile(), model_->bookmark_bar_node()->GetChild(i));
    }
  }
  NOTREACHED();
  return ui::DragDropTypes::DRAG_NONE;
}

bool BookmarkBarView::CanStartDragForView(views::View* sender,
                                          const gfx::Point& press_pt,
                                          const gfx::Point& p) {
  // Check if we have not moved enough horizontally but we have moved downward
  // vertically - downward drag.
  gfx::Vector2d move_offset = p - press_pt;
  gfx::Vector2d horizontal_offset(move_offset.x(), 0);
  if (!View::ExceededDragThreshold(horizontal_offset) && move_offset.y() > 0) {
    for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
      if (sender == GetBookmarkButton(i)) {
        const BookmarkNode* node = model_->bookmark_bar_node()->GetChild(i);
        // If the folder button was dragged, show the menu instead.
        if (node && node->is_folder()) {
          views::MenuButton* menu_button =
              static_cast<views::MenuButton*>(sender);
          menu_button->Activate();
          return false;
        }
        break;
      }
    }
  }
  return true;
}

void BookmarkBarView::OnMenuButtonClicked(views::View* view,
                                          const gfx::Point& point) {
  const BookmarkNode* node;

  int start_index = 0;
  if (view == other_bookmarked_button_) {
    node = model_->other_node();
  } else if (view == managed_bookmarks_button_) {
    node = client_->managed_node();
  } else if (view == overflow_button_) {
    node = model_->bookmark_bar_node();
    start_index = GetFirstHiddenNodeIndex();
  } else {
    int button_index = GetIndexOf(view);
    DCHECK_NE(-1, button_index);
    node = model_->bookmark_bar_node()->GetChild(button_index);
  }

  RecordBookmarkFolderOpen(GetBookmarkLaunchLocation());
  bookmark_menu_ = new BookmarkMenuController(
      browser_, page_navigator_, GetWidget(), node, start_index, false);
  bookmark_menu_->set_observer(this);
  bookmark_menu_->RunMenuAt(this);
}

void BookmarkBarView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  WindowOpenDisposition disposition_from_event_flags =
      ui::DispositionFromEventFlags(event.flags());

  if (sender->tag() == kAppsShortcutButtonTag) {
    OpenURLParams params(GURL(chrome::kChromeUIAppsURL),
                         Referrer(),
                         disposition_from_event_flags,
                         ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                         false);
    page_navigator_->OpenURL(params);
    RecordBookmarkAppsPageOpen(GetBookmarkLaunchLocation());
    return;
  }

  const BookmarkNode* node;
  if (sender->tag() == kOtherFolderButtonTag) {
    node = model_->other_node();
  } else if (sender->tag() == kManagedFolderButtonTag) {
    node = client_->managed_node();
  } else {
    int index = GetIndexOf(sender);
    DCHECK_NE(-1, index);
    node = model_->bookmark_bar_node()->GetChild(index);
  }
  DCHECK(page_navigator_);

  if (node->is_url()) {
    RecordAppLaunch(browser_->profile(), node->url());
    OpenURLParams params(
        node->url(), Referrer(), disposition_from_event_flags,
        ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
    page_navigator_->OpenURL(params);
  } else {
    chrome::OpenAll(GetWidget()->GetNativeWindow(), page_navigator_, node,
                    disposition_from_event_flags, browser_->profile());
  }

  RecordBookmarkLaunch(node, GetBookmarkLaunchLocation());
}

void BookmarkBarView::ShowContextMenuForView(views::View* source,
                                             const gfx::Point& point,
                                             ui::MenuSourceType source_type) {
  if (!model_->loaded()) {
    // Don't do anything if the model isn't loaded.
    return;
  }

  const BookmarkNode* parent = NULL;
  std::vector<const BookmarkNode*> nodes;
  if (source == other_bookmarked_button_) {
    parent = model_->other_node();
    // Do this so the user can open all bookmarks. BookmarkContextMenu makes
    // sure the user can't edit/delete the node in this case.
    nodes.push_back(parent);
  } else if (source == managed_bookmarks_button_) {
    parent = client_->managed_node();
    nodes.push_back(parent);
  } else if (source != this && source != apps_page_shortcut_) {
    // User clicked on one of the bookmark buttons, find which one they
    // clicked on, except for the apps page shortcut, which must behave as if
    // the user clicked on the bookmark bar background.
    int bookmark_button_index = GetIndexOf(source);
    DCHECK(bookmark_button_index != -1 &&
           bookmark_button_index < GetBookmarkButtonCount());
    const BookmarkNode* node =
        model_->bookmark_bar_node()->GetChild(bookmark_button_index);
    nodes.push_back(node);
    parent = node->parent();
  } else {
    parent = model_->bookmark_bar_node();
    nodes.push_back(parent);
  }
  bool close_on_remove =
      (parent == model_->other_node()) && (parent->child_count() == 1);

  context_menu_.reset(new BookmarkContextMenu(
      GetWidget(), browser_, browser_->profile(),
      browser_->tab_strip_model()->GetActiveWebContents(),
      parent, nodes, close_on_remove));
  context_menu_->RunMenuAt(point, source_type);
}

void BookmarkBarView::Init() {
  // Note that at this point we're not in a hierarchy so GetThemeProvider() will
  // return NULL.  When we're inserted into a hierarchy, we'll call
  // UpdateColors(), which will set the appropriate colors for all the objects
  // added in this function.

  // Child views are traversed in the order they are added. Make sure the order
  // they are added matches the visual order.
  overflow_button_ = CreateOverflowButton();
  AddChildView(overflow_button_);

  other_bookmarked_button_ = CreateOtherBookmarkedButton();
  // We'll re-enable when the model is loaded.
  other_bookmarked_button_->SetEnabled(false);
  AddChildView(other_bookmarked_button_);

  managed_bookmarks_button_ = CreateManagedBookmarksButton();
  // Also re-enabled when the model is loaded.
  managed_bookmarks_button_->SetEnabled(false);
  AddChildView(managed_bookmarks_button_);

  apps_page_shortcut_ = CreateAppsPageShortcutButton();
  AddChildView(apps_page_shortcut_);
  profile_pref_registrar_.Init(browser_->profile()->GetPrefs());
  profile_pref_registrar_.Add(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
      base::Bind(&BookmarkBarView::OnAppsPageShortcutVisibilityPrefChanged,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
      base::Bind(&BookmarkBarView::UpdateButtonsVisibility,
                 base::Unretained(this)));
  apps_page_shortcut_->SetVisible(
      chrome::ShouldShowAppsShortcutInBookmarkBar(
          browser_->profile(), browser_->host_desktop_type()));

  bookmarks_separator_view_ = new ButtonSeparatorView();
  AddChildView(bookmarks_separator_view_);
  UpdateBookmarksSeparatorVisibility();

  instructions_ = new BookmarkBarInstructionsView(this);
  AddChildView(instructions_);

  set_context_menu_controller(this);

  size_animation_.reset(new gfx::SlideAnimation(this));

  model_ = BookmarkModelFactory::GetForProfile(browser_->profile());
  client_ = ChromeBookmarkClientFactory::GetForProfile(browser_->profile());
  if (model_) {
    model_->AddObserver(this);
    if (model_->loaded())
      BookmarkModelLoaded(model_, false);
    // else case: we'll receive notification back from the BookmarkModel when
    // done loading, then we'll populate the bar.
  }
}

int BookmarkBarView::GetBookmarkButtonCount() const {
  // We contain six non-bookmark button views: managed bookmarks,
  // other bookmarks, bookmarks separator, chevrons (for overflow), apps page,
  // and the instruction label.
  return child_count() - 6;
}

views::LabelButton* BookmarkBarView::GetBookmarkButton(int index) {
  DCHECK(index >= 0 && index < GetBookmarkButtonCount());
  return static_cast<views::LabelButton*>(child_at(index));
}

BookmarkLaunchLocation BookmarkBarView::GetBookmarkLaunchLocation() const {
  return IsDetached() ? BOOKMARK_LAUNCH_LOCATION_DETACHED_BAR :
                        BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR;
}

int BookmarkBarView::GetFirstHiddenNodeIndex() {
  const int bb_count = GetBookmarkButtonCount();
  for (int i = 0; i < bb_count; ++i) {
    if (!GetBookmarkButton(i)->visible())
      return i;
  }
  return bb_count;
}

MenuButton* BookmarkBarView::CreateOtherBookmarkedButton() {
  // Title is set in Loaded.
  MenuButton* button =
      new BookmarkFolderButton(this, base::string16(), this, false);
  button->set_id(VIEW_ID_OTHER_BOOKMARKS);
  button->SetImage(views::Button::STATE_NORMAL, GetFolderIcon());
  button->set_context_menu_controller(this);
  button->set_tag(kOtherFolderButtonTag);
  return button;
}

MenuButton* BookmarkBarView::CreateManagedBookmarksButton() {
  // Title is set in Loaded.
  MenuButton* button =
      new BookmarkFolderButton(this, base::string16(), this, false);
  button->set_id(VIEW_ID_MANAGED_BOOKMARKS);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* image =
      rb->GetImageSkiaNamed(IDR_BOOKMARK_BAR_FOLDER_MANAGED);
  button->SetImage(views::Button::STATE_NORMAL, *image);
  button->set_context_menu_controller(this);
  button->set_tag(kManagedFolderButtonTag);
  return button;
}

MenuButton* BookmarkBarView::CreateOverflowButton() {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  MenuButton* button = new OverFlowButton(this);
  button->SetImage(views::Button::STATE_NORMAL,
                   *rb->GetImageSkiaNamed(IDR_BOOKMARK_BAR_CHEVRONS));

  // The overflow button's image contains an arrow and therefore it is a
  // direction sensitive image and we need to flip it if the UI layout is
  // right-to-left.
  //
  // By default, menu buttons are not flipped because they generally contain
  // text and flipping the gfx::Canvas object will break text rendering. Since
  // the overflow button does not contain text, we can safely flip it.
  button->EnableCanvasFlippingForRTLUI(true);

  // Make visible as necessary.
  button->SetVisible(false);
  // Set accessibility name.
  button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_BOOKMARKS_CHEVRON));
  return button;
}

views::View* BookmarkBarView::CreateBookmarkButton(const BookmarkNode* node) {
  if (node->is_url()) {
    BookmarkButton* button = new BookmarkButton(
        this, node->url(), node->GetTitle(), browser_->profile());
    ConfigureButton(node, button);
    return button;
  } else {
    views::MenuButton* button = new BookmarkFolderButton(
        this, node->GetTitle(), this, false);
    button->SetImage(views::Button::STATE_NORMAL, GetFolderIcon());
    ConfigureButton(node, button);
    return button;
  }
}

views::LabelButton* BookmarkBarView::CreateAppsPageShortcutButton() {
  views::LabelButton* button = new ShortcutButton(
      this, l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_APPS_SHORTCUT_NAME));
  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_BAR_APPS_SHORTCUT_TOOLTIP));
  button->set_id(VIEW_ID_BOOKMARK_BAR_ELEMENT);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  button->SetImage(views::Button::STATE_NORMAL,
                   *rb->GetImageSkiaNamed(IDR_BOOKMARK_BAR_APPS_SHORTCUT));
  button->set_context_menu_controller(this);
  button->set_tag(kAppsShortcutButtonTag);
  return button;
}

void BookmarkBarView::ConfigureButton(const BookmarkNode* node,
                                      views::LabelButton* button) {
  button->SetText(node->GetTitle());
  button->SetAccessibleName(node->GetTitle());
  button->set_id(VIEW_ID_BOOKMARK_BAR_ELEMENT);
  // We don't always have a theme provider (ui tests, for example).
  if (GetThemeProvider()) {
    button->SetTextColor(
        views::Button::STATE_NORMAL,
        GetThemeProvider()->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT));
  }

  button->SetMinSize(gfx::Size());
  button->set_context_menu_controller(this);
  button->set_drag_controller(this);
  if (node->is_url()) {
    const gfx::Image& favicon = model_->GetFavicon(node);
    if (!favicon.IsEmpty())
      button->SetImage(views::Button::STATE_NORMAL, *favicon.ToImageSkia());
    else
      button->SetImage(views::Button::STATE_NORMAL, GetDefaultFavicon());
  }
  button->SetMaxSize(gfx::Size(kMaxButtonWidth, 0));
}

void BookmarkBarView::BookmarkNodeAddedImpl(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            int index) {
  UpdateButtonsVisibility();
  if (parent != model->bookmark_bar_node()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  DCHECK(index >= 0 && index <= GetBookmarkButtonCount());
  const BookmarkNode* node = parent->GetChild(index);
  ProfileSyncService* sync_service(ProfileSyncServiceFactory::
      GetInstance()->GetForProfile(browser_->profile()));
  if (!throbbing_view_ && sync_service && sync_service->FirstSetupInProgress())
    StartThrobbing(node, true);
  AddChildViewAt(CreateBookmarkButton(node), index);
  UpdateColors();
  Layout();
  SchedulePaint();
}

void BookmarkBarView::BookmarkNodeRemovedImpl(BookmarkModel* model,
                                              const BookmarkNode* parent,
                                              int index) {
  UpdateButtonsVisibility();

  StopThrobbing(true);
  // No need to start throbbing again as the bookmark bubble can't be up at
  // the same time as the user reorders.

  if (parent != model->bookmark_bar_node()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  DCHECK(index >= 0 && index < GetBookmarkButtonCount());
  views::View* button = child_at(index);
  RemoveChildView(button);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, button);
  Layout();
  SchedulePaint();
}

void BookmarkBarView::BookmarkNodeChangedImpl(BookmarkModel* model,
                                              const BookmarkNode* node) {
  if (node == client_->managed_node()) {
    // The managed node may have its title updated.
    managed_bookmarks_button_->SetAccessibleName(
        client_->managed_node()->GetTitle());
    managed_bookmarks_button_->SetText(client_->managed_node()->GetTitle());
    return;
  }

  if (node->parent() != model->bookmark_bar_node()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  int index = model->bookmark_bar_node()->GetIndexOf(node);
  DCHECK_NE(-1, index);
  views::LabelButton* button = GetBookmarkButton(index);
  gfx::Size old_pref = button->GetPreferredSize();
  ConfigureButton(node, button);
  gfx::Size new_pref = button->GetPreferredSize();
  if (old_pref.width() != new_pref.width()) {
    Layout();
    SchedulePaint();
  } else if (button->visible()) {
    button->SchedulePaint();
  }
}

void BookmarkBarView::ShowDropFolderForNode(const BookmarkNode* node) {
  if (bookmark_drop_menu_) {
    if (bookmark_drop_menu_->node() == node) {
      // Already showing for the specified node.
      return;
    }
    bookmark_drop_menu_->Cancel();
  }

  views::MenuButton* menu_button = GetMenuButtonForNode(node);
  if (!menu_button)
    return;

  int start_index = 0;
  if (node == model_->bookmark_bar_node())
    start_index = GetFirstHiddenNodeIndex();

  drop_info_->is_menu_showing = true;
  bookmark_drop_menu_ = new BookmarkMenuController(
      browser_, page_navigator_, GetWidget(), node, start_index, true);
  bookmark_drop_menu_->set_observer(this);
  bookmark_drop_menu_->RunMenuAt(this);
}

void BookmarkBarView::StopShowFolderDropMenuTimer() {
  show_folder_method_factory_.InvalidateWeakPtrs();
}

void BookmarkBarView::StartShowFolderDropMenuTimer(const BookmarkNode* node) {
  if (!animations_enabled) {
    // So that tests can run as fast as possible disable the delay during
    // testing.
    ShowDropFolderForNode(node);
    return;
  }
  show_folder_method_factory_.InvalidateWeakPtrs();
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BookmarkBarView::ShowDropFolderForNode,
                 show_folder_method_factory_.GetWeakPtr(),
                 node),
      base::TimeDelta::FromMilliseconds(views::GetMenuShowDelay()));
}

void BookmarkBarView::CalculateDropLocation(const DropTargetEvent& event,
                                            const BookmarkNodeData& data,
                                            DropLocation* location) {
  DCHECK(model_);
  DCHECK(model_->loaded());
  DCHECK(data.is_valid());

  *location = DropLocation();

  // The drop event uses the screen coordinates while the child Views are
  // always laid out from left to right (even though they are rendered from
  // right-to-left on RTL locales). Thus, in order to make sure the drop
  // coordinates calculation works, we mirror the event's X coordinate if the
  // locale is RTL.
  int mirrored_x = GetMirroredXInView(event.x());

  bool found = false;
  const int other_delta_x = mirrored_x - other_bookmarked_button_->x();
  Profile* profile = browser_->profile();
  if (other_bookmarked_button_->visible() && other_delta_x >= 0 &&
      other_delta_x < other_bookmarked_button_->width()) {
    // Mouse is over 'other' folder.
    location->button_type = DROP_OTHER_FOLDER;
    location->on = true;
    found = true;
  } else if (!GetBookmarkButtonCount()) {
    // No bookmarks, accept the drop.
    location->index = 0;
    const BookmarkNode* node = data.GetFirstNode(model_, profile->GetPath());
    int ops = node && client_->CanBeEditedByUser(node) ?
        ui::DragDropTypes::DRAG_MOVE :
        ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK;
    location->operation = chrome::GetPreferredBookmarkDropOperation(
        event.source_operations(), ops);
    return;
  }

  for (int i = 0; i < GetBookmarkButtonCount() &&
       GetBookmarkButton(i)->visible() && !found; i++) {
    views::LabelButton* button = GetBookmarkButton(i);
    int button_x = mirrored_x - button->x();
    int button_w = button->width();
    if (button_x < button_w) {
      found = true;
      const BookmarkNode* node = model_->bookmark_bar_node()->GetChild(i);
      if (node->is_folder()) {
        if (button_x <= views::kDropBetweenPixels) {
          location->index = i;
        } else if (button_x < button_w - views::kDropBetweenPixels) {
          location->index = i;
          location->on = true;
        } else {
          location->index = i + 1;
        }
      } else if (button_x < button_w / 2) {
        location->index = i;
      } else {
        location->index = i + 1;
      }
      break;
    }
  }

  if (!found) {
    if (overflow_button_->visible()) {
      // Are we over the overflow button?
      int overflow_delta_x = mirrored_x - overflow_button_->x();
      if (overflow_delta_x >= 0 &&
          overflow_delta_x < overflow_button_->width()) {
        // Mouse is over overflow button.
        location->index = GetFirstHiddenNodeIndex();
        location->button_type = DROP_OVERFLOW;
      } else if (overflow_delta_x < 0) {
        // Mouse is after the last visible button but before overflow button;
        // use the last visible index.
        location->index = GetFirstHiddenNodeIndex();
      } else {
        return;
      }
    } else if (!other_bookmarked_button_->visible() ||
               mirrored_x < other_bookmarked_button_->x()) {
      // Mouse is after the last visible button but before more recently
      // bookmarked; use the last visible index.
      location->index = GetFirstHiddenNodeIndex();
    } else {
      return;
    }
  }

  if (location->on) {
    const BookmarkNode* parent = (location->button_type == DROP_OTHER_FOLDER) ?
        model_->other_node() :
        model_->bookmark_bar_node()->GetChild(location->index);
    location->operation = chrome::GetBookmarkDropOperation(
        profile, event, data, parent, parent->child_count());
    if (!location->operation && !data.has_single_url() &&
        data.GetFirstNode(model_, profile->GetPath()) == parent) {
      // Don't open a menu if the node being dragged is the menu to open.
      location->on = false;
    }
  } else {
    location->operation = chrome::GetBookmarkDropOperation(
        profile, event, data, model_->bookmark_bar_node(), location->index);
  }
}

void BookmarkBarView::WriteBookmarkDragData(const BookmarkNode* node,
                                            ui::OSExchangeData* data) {
  DCHECK(node && data);
  BookmarkNodeData drag_data(node);
  drag_data.Write(browser_->profile()->GetPath(), data);
}

void BookmarkBarView::StartThrobbing(const BookmarkNode* node,
                                     bool overflow_only) {
  DCHECK(!throbbing_view_);

  // Determine which visible button is showing the bookmark (or is an ancestor
  // of the bookmark).
  const BookmarkNode* bbn = model_->bookmark_bar_node();
  const BookmarkNode* parent_on_bb = node;
  while (parent_on_bb) {
    const BookmarkNode* parent = parent_on_bb->parent();
    if (parent == bbn)
      break;
    parent_on_bb = parent;
  }
  if (parent_on_bb) {
    int index = bbn->GetIndexOf(parent_on_bb);
    if (index >= GetFirstHiddenNodeIndex()) {
      // Node is hidden, animate the overflow button.
      throbbing_view_ = overflow_button_;
    } else if (!overflow_only) {
      throbbing_view_ = static_cast<CustomButton*>(child_at(index));
    }
  } else if (client_->IsDescendantOfManagedNode(node)) {
    throbbing_view_ = managed_bookmarks_button_;
  } else if (!overflow_only) {
    throbbing_view_ = other_bookmarked_button_;
  }

  // Use a large number so that the button continues to throb.
  if (throbbing_view_)
    throbbing_view_->StartThrobbing(std::numeric_limits<int>::max());
}

views::CustomButton* BookmarkBarView::DetermineViewToThrobFromRemove(
    const BookmarkNode* parent,
    int old_index) {
  const BookmarkNode* bbn = model_->bookmark_bar_node();
  const BookmarkNode* old_node = parent;
  int old_index_on_bb = old_index;
  while (old_node && old_node != bbn) {
    const BookmarkNode* parent = old_node->parent();
    if (parent == bbn) {
      old_index_on_bb = bbn->GetIndexOf(old_node);
      break;
    }
    old_node = parent;
  }
  if (old_node) {
    if (old_index_on_bb >= GetFirstHiddenNodeIndex()) {
      // Node is hidden, animate the overflow button.
      return overflow_button_;
    }
    return static_cast<CustomButton*>(child_at(old_index_on_bb));
  }
  if (client_->IsDescendantOfManagedNode(parent))
    return managed_bookmarks_button_;
  // Node wasn't on the bookmark bar, use the other bookmark button.
  return other_bookmarked_button_;
}

void BookmarkBarView::UpdateColors() {
  // We don't always have a theme provider (ui tests, for example).
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return;
  SkColor color =
      theme_provider->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT);
  for (int i = 0; i < GetBookmarkButtonCount(); ++i)
    GetBookmarkButton(i)->SetTextColor(views::Button::STATE_NORMAL, color);
  other_bookmarked_button_->SetTextColor(views::Button::STATE_NORMAL, color);
  managed_bookmarks_button_->SetTextColor(views::Button::STATE_NORMAL, color);
  if (apps_page_shortcut_->visible())
    apps_page_shortcut_->SetTextColor(views::Button::STATE_NORMAL, color);
}

void BookmarkBarView::UpdateButtonsVisibility() {
  bool has_other_children = !model_->other_node()->empty();
  bool update_other = has_other_children != other_bookmarked_button_->visible();
  if (update_other) {
    other_bookmarked_button_->SetVisible(has_other_children);
    UpdateBookmarksSeparatorVisibility();
  }

  bool show_managed = !client_->managed_node()->empty() &&
                      browser_->profile()->GetPrefs()->GetBoolean(
                          bookmarks::prefs::kShowManagedBookmarksInBookmarkBar);
  bool update_managed = show_managed != managed_bookmarks_button_->visible();
  if (update_managed)
    managed_bookmarks_button_->SetVisible(show_managed);

  if (update_other || update_managed) {
    Layout();
    SchedulePaint();
  }
}

void BookmarkBarView::UpdateBookmarksSeparatorVisibility() {
  // Ash does not paint the bookmarks separator line because it looks odd on
  // the flat background.  We keep it present for layout, but don't draw it.
  bookmarks_separator_view_->SetVisible(
      browser_->host_desktop_type() != chrome::HOST_DESKTOP_TYPE_ASH &&
      other_bookmarked_button_->visible());
}

void BookmarkBarView::LayoutItems() {
  if (!parent())
    return;

  int x = kLeftMargin;
  int top_margin = IsDetached() ? kDetachedTopMargin : 0;
  int y = top_margin;
  int width = View::width() - kRightMargin - kLeftMargin;
  int height = chrome::kBookmarkBarHeight - kBottomMargin;
  int separator_margin = kSeparatorMargin;

  if (IsDetached()) {
    double current_state = 1 - size_animation_->GetCurrentValue();
    x += static_cast<int>(kNewtabHorizontalPadding * current_state);
    y += (View::height() - chrome::kBookmarkBarHeight) / 2;
    width -= static_cast<int>(kNewtabHorizontalPadding * current_state);
    separator_margin -= static_cast<int>(kSeparatorMargin * current_state);
  } else {
    // For the attached appearance, pin the content to the bottom of the bar
    // when animating in/out, as shrinking its height instead looks weird.  This
    // also matches how we layout infobars.
    y += View::height() - chrome::kBookmarkBarHeight;
  }

  gfx::Size other_bookmarked_pref = other_bookmarked_button_->visible() ?
      other_bookmarked_button_->GetPreferredSize() : gfx::Size();
  gfx::Size overflow_pref = overflow_button_->GetPreferredSize();
  gfx::Size bookmarks_separator_pref =
      bookmarks_separator_view_->GetPreferredSize();
  gfx::Size apps_page_shortcut_pref = apps_page_shortcut_->visible() ?
      apps_page_shortcut_->GetPreferredSize() : gfx::Size();

  int max_x = width - overflow_pref.width() - kButtonPadding -
      bookmarks_separator_pref.width();
  if (other_bookmarked_button_->visible())
    max_x -= other_bookmarked_pref.width() + kButtonPadding;

  // Next, layout out the buttons. Any buttons that are placed beyond the
  // visible region are made invisible.

  // Start with the apps page shortcut button.
  if (apps_page_shortcut_->visible()) {
    apps_page_shortcut_->SetBounds(x, y, apps_page_shortcut_pref.width(),
                                   height);
    x += apps_page_shortcut_pref.width() + kButtonPadding;
  }

  // Then comes the managed bookmarks folder, if visible.
  if (managed_bookmarks_button_->visible()) {
    gfx::Size managed_bookmarks_pref = managed_bookmarks_button_->visible() ?
        managed_bookmarks_button_->GetPreferredSize() : gfx::Size();
    managed_bookmarks_button_->SetBounds(x, y, managed_bookmarks_pref.width(),
                                         height);
    x += managed_bookmarks_pref.width() + kButtonPadding;
  }

  // Then go through the bookmark buttons.
  if (GetBookmarkButtonCount() == 0 && model_ && model_->loaded()) {
    gfx::Size pref = instructions_->GetPreferredSize();
    instructions_->SetBounds(
        x + kInstructionsPadding, y,
        std::min(static_cast<int>(pref.width()),
                 max_x - x),
        height);
    instructions_->SetVisible(true);
  } else {
    instructions_->SetVisible(false);

    for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
      views::View* child = child_at(i);
      gfx::Size pref = child->GetPreferredSize();
      int next_x = x + pref.width() + kButtonPadding;
      child->SetVisible(next_x < max_x);
      child->SetBounds(x, y, pref.width(), height);
      x = next_x;
    }
  }

  // Layout the right side of the bar.
  const bool all_visible = (GetBookmarkButtonCount() == 0 ||
                            child_at(GetBookmarkButtonCount() - 1)->visible());

  // Layout the right side buttons.
  x = max_x + kButtonPadding;

  // The overflow button.
  overflow_button_->SetBounds(x, y, overflow_pref.width(), height);
  overflow_button_->SetVisible(!all_visible);
  x += overflow_pref.width();

  // Separator.
  if (bookmarks_separator_view_->visible()) {
    bookmarks_separator_view_->SetBounds(x,
                                         y - top_margin,
                                         bookmarks_separator_pref.width(),
                                         height + top_margin + kBottomMargin -
                                         separator_margin);

    x += bookmarks_separator_pref.width();
  }

  // The other bookmarks button.
  if (other_bookmarked_button_->visible()) {
    other_bookmarked_button_->SetBounds(x, y, other_bookmarked_pref.width(),
                                        height);
    x += other_bookmarked_pref.width() + kButtonPadding;
  }
}

void BookmarkBarView::OnAppsPageShortcutVisibilityPrefChanged() {
  DCHECK(apps_page_shortcut_);
  // Only perform layout if required.
  bool visible = chrome::ShouldShowAppsShortcutInBookmarkBar(
      browser_->profile(), browser_->host_desktop_type());
  if (apps_page_shortcut_->visible() == visible)
    return;
  apps_page_shortcut_->SetVisible(visible);
  UpdateBookmarksSeparatorVisibility();
  Layout();
}
