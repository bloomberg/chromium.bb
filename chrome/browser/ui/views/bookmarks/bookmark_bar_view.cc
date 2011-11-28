// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include <algorithm>
#include <limits>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/page_navigator.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/page_transition_types.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "views/controls/label.h"
#include "views/drag_utils.h"
#include "views/metrics.h"
#include "views/view_constants.h"

using views::CustomButton;
using views::DropTargetEvent;
using views::MenuButton;
using views::MenuItemView;
using views::View;

// How much we want the bookmark bar to overlap the toolbar when in its
// 'always shown' mode.
static const int kToolbarOverlap = 3;

// Margins around the content.
static const int kDetachedTopMargin = 1;  // When attached, we use 0 and let the
                                          // toolbar above serve as the margin.
static const int kBottomMargin = 2;
static const int kLeftMargin = 1;
static const int kRightMargin = 1;

// static
const char BookmarkBarView::kViewClassName[] =
    "browser/ui/views/bookmarks/BookmarkBarView";

// Padding between buttons.
static const int kButtonPadding = 0;

// Icon to display when one isn't found for the page.
static SkBitmap* kDefaultFavicon = NULL;

// Icon used for folders.
static SkBitmap* kFolderIcon = NULL;

// Offset for where the menu is shown relative to the bottom of the
// BookmarkBarView.
static const int kMenuOffset = 3;

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

namespace {

// BookmarkButton -------------------------------------------------------------

// Buttons used for the bookmarks on the bookmark bar.

class BookmarkButton : public views::TextButton {
 public:
  // The internal view class name.
  static const char kViewClassName[];

  BookmarkButton(views::ButtonListener* listener,
                 const GURL& url,
                 const string16& title,
                 Profile* profile)
      : TextButton(listener, title),
        url_(url),
        profile_(profile) {
    show_animation_.reset(new ui::SlideAnimation(this));
    if (BookmarkBarView::testing_) {
      // For some reason during testing the events generated by animating
      // throw off the test. So, don't animate while testing.
      show_animation_->Reset(1);
    } else {
      show_animation_->Show();
    }
  }

  virtual bool GetTooltipText(const gfx::Point& p,
                              string16* tooltip) const OVERRIDE {
    gfx::Point location(p);
    ConvertPointToScreen(this, &location);
    *tooltip = BookmarkBarView::CreateToolTipForURLAndTitle(location, url_,
                                                            text(), profile_);
    return !tooltip->empty();
  }

  virtual bool IsTriggerableEvent(const views::MouseEvent& e) OVERRIDE {
    return event_utils::IsPossibleDispositionEvent(e);
  }

  virtual std::string GetClassName() const OVERRIDE {
    return kViewClassName;
  }

 private:
  const GURL& url_;
  Profile* profile_;
  scoped_ptr<ui::SlideAnimation> show_animation_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkButton);
};

// static for BookmarkButton
const char BookmarkButton::kViewClassName[] =
    "browser/ui/views/bookmarks/BookmarkButton";

// BookmarkFolderButton -------------------------------------------------------

// Buttons used for folders on the bookmark bar, including the 'other folders'
// button.
class BookmarkFolderButton : public views::MenuButton {
 public:
  BookmarkFolderButton(views::ButtonListener* listener,
                       const string16& title,
                       views::ViewMenuDelegate* menu_delegate,
                       bool show_menu_marker)
      : MenuButton(listener, title, menu_delegate, show_menu_marker) {
    show_animation_.reset(new ui::SlideAnimation(this));
    if (BookmarkBarView::testing_) {
      // For some reason during testing the events generated by animating
      // throw off the test. So, don't animate while testing.
      show_animation_->Reset(1);
    } else {
      show_animation_->Show();
    }
  }

  virtual bool GetTooltipText(const gfx::Point& p,
                              string16* tooltip) const OVERRIDE {
    if (text_size_.width() > GetTextBounds().width())
      *tooltip = text_;
    return !tooltip->empty();
  }

  virtual bool IsTriggerableEvent(const views::MouseEvent& e) OVERRIDE {
    // Left clicks should show the menu contents and right clicks should show
    // the context menu. They should not trigger the opening of underlying urls.
    if (e.flags() == ui::EF_LEFT_BUTTON_DOWN ||
        e.flags() == ui::EF_RIGHT_BUTTON_DOWN)
      return false;

    WindowOpenDisposition disposition(
        event_utils::DispositionFromEventFlags(e.flags()));
    return disposition != CURRENT_TAB;
  }

  virtual void OnPaint(gfx::Canvas* canvas) {
    views::MenuButton::PaintButton(canvas, views::MenuButton::PB_NORMAL);
  }

 private:
  scoped_ptr<ui::SlideAnimation> show_animation_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkFolderButton);
};

// OverFlowButton (chevron) --------------------------------------------------

class OverFlowButton : public views::MenuButton {
 public:
  explicit OverFlowButton(BookmarkBarView* owner)
      : MenuButton(NULL, string16(), owner, false),
        owner_(owner) {}

  virtual bool OnMousePressed(const views::MouseEvent& e) {
    owner_->StopThrobbing(true);
    return views::MenuButton::OnMousePressed(e);
  }

 private:
  BookmarkBarView* owner_;

  DISALLOW_COPY_AND_ASSIGN(OverFlowButton);
};

void RecordAppLaunch(Profile* profile, GURL url) {
  DCHECK(profile->GetExtensionService());
  if (!profile->GetExtensionService()->IsInstalledApp(url))
    return;

  UMA_HISTOGRAM_ENUMERATION(extension_misc::kAppLaunchHistogram,
                            extension_misc::APP_LAUNCH_BOOKMARK_BAR,
                            extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
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
        GetThemeProvider()->GetColor(ThemeService::COLOR_TOOLBAR));
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    // We get the full height of the bookmark bar, so that the height returned
    // here doesn't matter.
    return gfx::Size(kSeparatorWidth, 1);
  }

  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE {
    state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_SEPARATOR);
    state->role = ui::AccessibilityTypes::ROLE_SEPARATOR;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ButtonSeparatorView);
};

// BookmarkBarView ------------------------------------------------------------

// static
const int BookmarkBarView::kMaxButtonWidth = 150;
const int BookmarkBarView::kNewtabHorizontalPadding = 8;
const int BookmarkBarView::kNewtabVerticalPadding = 12;

// static
bool BookmarkBarView::testing_ = false;

// Returns the bitmap to use for starred folders.
static const SkBitmap& GetFolderIcon() {
  if (!kFolderIcon) {
    kFolderIcon = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_BOOKMARK_BAR_FOLDER);
  }
  return *kFolderIcon;
}

BookmarkBarView::BookmarkBarView(Browser* browser)
    : page_navigator_(NULL),
      model_(NULL),
      bookmark_menu_(NULL),
      bookmark_drop_menu_(NULL),
      other_bookmarked_button_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(show_folder_method_factory_(this)),
      overflow_button_(NULL),
      instructions_(NULL),
      bookmarks_separator_view_(NULL),
      browser_(browser),
      infobar_visible_(false),
      throbbing_view_(NULL),
      bookmark_bar_state_(BookmarkBar::SHOW),
      animating_detached_(false) {
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
  }
  if (context_menu_.get())
    context_menu_->SetPageNavigator(NULL);

  StopShowFolderDropMenuTimer();
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
  if (animate_type == BookmarkBar::ANIMATE_STATE_CHANGE) {
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

int BookmarkBarView::GetToolbarOverlap(bool return_max) const {
  // When not detached, always overlap by the full amount.
  if (return_max || bookmark_bar_state_ != BookmarkBar::DETACHED)
    return kToolbarOverlap;
  // When detached with an infobar, overlap by 0 whenever the infobar
  // is above us (i.e. when we're detached), since drawing over the infobar
  // looks weird.
  if (IsDetached() && infobar_visible_)
    return 0;
  // When detached with no infobar, animate the overlap between the attached and
  // detached states.
  return static_cast<int>(kToolbarOverlap * size_animation_->GetCurrentValue());
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

  // Check the buttons first.
  for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
    views::View* child = child_at(i);
    if (!child->IsVisible())
      break;
    if (child->bounds().Contains(adjusted_loc))
      return model_->bookmark_bar_node()->GetChild(i);
  }

  // Then the overflow button.
  if (overflow_button_->IsVisible() &&
      overflow_button_->bounds().Contains(adjusted_loc)) {
    *model_start_index = GetFirstHiddenNodeIndex();
    return model_->bookmark_bar_node();
  }

  // And finally the other folder.
  if (other_bookmarked_button_->IsVisible() &&
      other_bookmarked_button_->bounds().Contains(adjusted_loc)) {
    return model_->other_node();
  }

  return NULL;
}

views::MenuButton* BookmarkBarView::GetMenuButtonForNode(
    const BookmarkNode* node) {
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
    MenuItemView::AnchorPosition* anchor) {
  if (button == other_bookmarked_button_ || button == overflow_button_)
    *anchor = MenuItemView::TOPRIGHT;
  else
    *anchor = MenuItemView::TOPLEFT;
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
string16 BookmarkBarView::CreateToolTipForURLAndTitle(
    const gfx::Point& screen_loc,
    const GURL& url,
    const string16& title,
    Profile* profile) {
  int max_width = views::TooltipManager::GetMaxWidth(screen_loc.x(),
                                                     screen_loc.y());
  gfx::Font tt_font = views::TooltipManager::GetDefaultFont();
  string16 result;

  // First the title.
  if (!title.empty()) {
    string16 localized_title = title;
    base::i18n::AdjustStringForLocaleDirection(&localized_title);
    result.append(ui::ElideText(localized_title, tt_font, max_width, false));
  }

  // Only show the URL if the url and title differ.
  if (title != UTF8ToUTF16(url.spec())) {
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
    string16 elided_url(ui::ElideUrl(url, tt_font, max_width, languages));
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
  return GetToolbarOverlap(false);
}

gfx::Size BookmarkBarView::GetPreferredSize() {
  return LayoutItems(true);
}

gfx::Size BookmarkBarView::GetMinimumSize() {
  // The minimum width of the bookmark bar should at least contain the overflow
  // button, by which one can access all the Bookmark Bar items, and the "Other
  // Bookmarks" folder, along with appropriate margins and button padding.
  int width = kLeftMargin;

  if (bookmark_bar_state_ == BookmarkBar::DETACHED) {
    double current_state = 1 - size_animation_->GetCurrentValue();
    width += 2 * static_cast<int>(kNewtabHorizontalPadding * current_state);
  }

  gfx::Size other_bookmarked_pref =
      other_bookmarked_button_->GetPreferredSize();
  gfx::Size overflow_pref = overflow_button_->GetPreferredSize();
  gfx::Size bookmarks_separator_pref =
      bookmarks_separator_view_->GetPreferredSize();

  width += other_bookmarked_pref.width() + kButtonPadding +
      overflow_pref.width() + kButtonPadding +
      bookmarks_separator_pref.width();

  return gfx::Size(width, browser_defaults::kBookmarkBarHeight);
}

void BookmarkBarView::Layout() {
  LayoutItems(false);
}

void BookmarkBarView::ViewHierarchyChanged(bool is_add,
                                           View* parent,
                                           View* child) {
  if (is_add && child == this) {
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

void BookmarkBarView::PaintChildren(gfx::Canvas* canvas) {
  View::PaintChildren(canvas);

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
    if (GetBookmarkButtonCount() > 0 && GetBookmarkButton(0)->IsVisible()) {
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
    canvas->FillRect(kDropIndicatorColor, indicator_bounds);
  }
}

bool BookmarkBarView::GetDropFormats(
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) {
  if (!model_ || !model_->IsLoaded())
    return false;
  *formats = ui::OSExchangeData::URL;
  custom_formats->insert(BookmarkNodeData::GetBookmarkCustomFormat());
  return true;
}

bool BookmarkBarView::AreDropTypesRequired() {
  return true;
}

bool BookmarkBarView::CanDrop(const ui::OSExchangeData& data) {
  if (!model_ || !model_->IsLoaded() ||
      !browser_->profile()->GetPrefs()->GetBoolean(
          prefs::kEditBookmarksEnabled))
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
  drop_info_.reset();
  return bookmark_utils::PerformBookmarkDrop(browser_->profile(), data,
                                             parent_node, index);
}

void BookmarkBarView::ShowContextMenu(const gfx::Point& p,
                                      bool is_mouse_gesture) {
  ShowContextMenuForView(this, p, is_mouse_gesture);
}

void BookmarkBarView::OnThemeChanged() {
  UpdateColors();
}

std::string BookmarkBarView::GetClassName() const {
  return kViewClassName;
}

void BookmarkBarView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_TOOLBAR;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_BOOKMARKS);
}

void BookmarkBarView::AnimationProgressed(const ui::Animation* animation) {
  if (browser_)
    browser_->BookmarkBarSizeChanged(true);
}

void BookmarkBarView::AnimationEnded(const ui::Animation* animation) {
  if (browser_)
    browser_->BookmarkBarSizeChanged(false);

  SchedulePaint();
}

void BookmarkBarView::BookmarkMenuDeleted(BookmarkMenuController* controller) {
  if (controller == bookmark_menu_)
    bookmark_menu_ = NULL;
  else if (controller == bookmark_drop_menu_)
    bookmark_drop_menu_ = NULL;
}

void BookmarkBarView::ShowImportDialog() {
  browser_->OpenImportSettingsDialog();
}

void BookmarkBarView::Loaded(BookmarkModel* model, bool ids_reassigned) {
  volatile int button_count = GetBookmarkButtonCount();
  DCHECK(button_count == 0);  // If non-zero it means Load was invoked more than
                              // once, or we didn't properly clear things.
                              // Either of which shouldn't happen
  const BookmarkNode* node = model_->bookmark_bar_node();
  DCHECK(node && model_->other_node());
  // Create a button for each of the children on the bookmark bar.
  for (int i = 0, child_count = node->child_count(); i < child_count; ++i)
    AddChildViewAt(CreateBookmarkButton(node->GetChild(i)), i);
  UpdateColors();
  UpdateOtherBookmarksVisibility();
  other_bookmarked_button_->SetEnabled(true);

  Layout();
  SchedulePaint();
}

void BookmarkBarView::BookmarkModelBeingDeleted(BookmarkModel* model) {
  // In normal shutdown The bookmark model should never be deleted before us.
  // When X exits suddenly though, it can happen, This code exists
  // to check for regressions in shutdown code and not crash.
  if (!browser_shutdown::ShuttingDownWithoutClosingBrowsers())
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
                                          const BookmarkNode* node) {
  BookmarkNodeRemovedImpl(model, parent, old_index);
}

void BookmarkBarView::BookmarkNodeChanged(BookmarkModel* model,
                                          const BookmarkNode* node) {
  BookmarkNodeChangedImpl(model, node);
}

void BookmarkBarView::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  if (node != model_->bookmark_bar_node())
    return;  // We only care about reordering of the bookmark bar node.

  // Remove the existing buttons.
  while (GetBookmarkButtonCount()) {
    views::View* button = child_at(0);
    RemoveChildView(button);
    MessageLoop::current()->DeleteSoon(FROM_HERE, button);
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
  UserMetrics::RecordAction(UserMetricsAction("BookmarkBar_DragButton"));

  for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
    if (sender == GetBookmarkButton(i)) {
      views::TextButton* button = GetBookmarkButton(i);
      gfx::CanvasSkia canvas(button->width(), button->height(), false);
      button->PaintButton(&canvas, views::TextButton::PB_FOR_DRAG);
      drag_utils::SetDragImageOnDataObject(canvas, button->size(), press_pt,
                                           data);
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
      return bookmark_utils::BookmarkDragOperation(
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
  if (!View::ExceededDragThreshold(press_pt.x() - p.x(), 0) &&
      press_pt.y() < p.y()) {
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

void BookmarkBarView::RunMenu(views::View* view, const gfx::Point& pt) {
  const BookmarkNode* node;

  int start_index = 0;
  if (view == other_bookmarked_button_) {
    node = model_->other_node();
  } else if (view == overflow_button_) {
    node = model_->bookmark_bar_node();
    start_index = GetFirstHiddenNodeIndex();
  } else {
    int button_index = GetIndexOf(view);
    DCHECK_NE(-1, button_index);
    node = model_->bookmark_bar_node()->GetChild(button_index);
  }

  bookmark_menu_ = new BookmarkMenuController(browser_->profile(),
      page_navigator_, GetWidget(), node, start_index);
  bookmark_menu_->set_observer(this);
  bookmark_menu_->RunMenuAt(this, false);
}

void BookmarkBarView::ButtonPressed(views::Button* sender,
                                    const views::Event& event) {
  const BookmarkNode* node;
  if (sender->tag() == kOtherFolderButtonTag) {
    node = model_->other_node();
  } else {
    int index = GetIndexOf(sender);
    DCHECK_NE(-1, index);
    node = model_->bookmark_bar_node()->GetChild(index);
  }
  DCHECK(page_navigator_);

  WindowOpenDisposition disposition_from_event_flags =
      event_utils::DispositionFromEventFlags(sender->mouse_event_flags());

  Profile* profile = browser_->profile();
  if (node->is_url()) {
    RecordAppLaunch(profile, node->url());
    page_navigator_->OpenURL(node->url(), GURL(),
        disposition_from_event_flags, content::PAGE_TRANSITION_AUTO_BOOKMARK);
  } else {
    bookmark_utils::OpenAll(GetWidget()->GetNativeWindow(), profile,
        page_navigator_, node, disposition_from_event_flags);
  }

  bookmark_utils::RecordBookmarkLaunch(IsDetached() ?
      bookmark_utils::LAUNCH_DETACHED_BAR :
      bookmark_utils::LAUNCH_ATTACHED_BAR);
  UserMetrics::RecordAction(UserMetricsAction("ClickedBookmarkBarURLButton"));
}

void BookmarkBarView::ShowContextMenuForView(View* source,
                                             const gfx::Point& p,
                                             bool is_mouse_gesture) {
  if (!model_->IsLoaded()) {
    // Don't do anything if the model isn't loaded.
    return;
  }

  const BookmarkNode* parent = NULL;
  std::vector<const BookmarkNode*> nodes;
  if (source == other_bookmarked_button_) {
    parent = model_->other_node();
    // Do this so the user can open all bookmarks. BookmarkContextMenu makes
    // sure the user can edit/delete the node in this case.
    nodes.push_back(parent);
  } else if (source != this) {
    // User clicked on one of the bookmark buttons, find which one they
    // clicked on.
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
  Profile* profile = browser_->profile();
  bool close_on_remove =
      (parent == profile->GetBookmarkModel()->other_node()) &&
      (parent->child_count() == 1);
  context_menu_.reset(new BookmarkContextMenu(GetWidget(), profile,
      browser_->GetSelectedTabContents(), parent, nodes, close_on_remove));
  context_menu_->RunMenuAt(p);
}

void BookmarkBarView::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK(browser_->profile());
  switch (type) {
    case chrome::NOTIFICATION_BOOKMARK_BUBBLE_SHOWN: {
      StopThrobbing(true);
      GURL url = *(content::Details<GURL>(details).ptr());
      const BookmarkNode* node = model_->GetMostRecentlyAddedNodeForURL(url);
      if (!node)
        return;  // Generally shouldn't happen.
      StartThrobbing(node, false);
      break;
    }
    case chrome::NOTIFICATION_BOOKMARK_BUBBLE_HIDDEN:
      StopThrobbing(false);
      break;

    default:
      NOTREACHED();
      break;
  }
}

void BookmarkBarView::Init() {
  // Note that at this point we're not in a hierarchy so GetThemeProvider() will
  // return NULL.  When we're inserted into a hierarchy, we'll call
  // UpdateColors(), which will set the appropriate colors for all the objects
  // added in this function.

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  if (!kDefaultFavicon)
    kDefaultFavicon = rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);

  // Child views are traversed in the order they are added. Make sure the order
  // they are added matches the visual order.
  overflow_button_ = CreateOverflowButton();
  AddChildView(overflow_button_);

  other_bookmarked_button_ = CreateOtherBookmarkedButton();
  // We'll re-enable when the model is loaded.
  other_bookmarked_button_->SetEnabled(false);
  AddChildView(other_bookmarked_button_);

  bookmarks_separator_view_ = new ButtonSeparatorView();
  AddChildView(bookmarks_separator_view_);

  instructions_ = new BookmarkBarInstructionsView(this);
  AddChildView(instructions_);

  set_context_menu_controller(this);

  size_animation_.reset(new ui::SlideAnimation(this));

  Profile* profile = browser_->profile();
  content::Source<Profile> ns_source(profile->GetOriginalProfile());
  registrar_.Add(this, chrome::NOTIFICATION_BOOKMARK_BUBBLE_SHOWN, ns_source);
  registrar_.Add(this, chrome::NOTIFICATION_BOOKMARK_BUBBLE_HIDDEN, ns_source);

  model_ = profile->GetBookmarkModel();
  if (model_) {
    model_->AddObserver(this);
    if (model_->IsLoaded())
      Loaded(model_, false);
    // else case: we'll receive notification back from the BookmarkModel when
    // done loading, then we'll populate the bar.
  }
}

int BookmarkBarView::GetBookmarkButtonCount() {
  // We contain four non-bookmark button views: other bookmarks, bookmarks
  // separator, chevrons (for overflow), and the instruction label.
  return child_count() - 4;
}

views::TextButton* BookmarkBarView::GetBookmarkButton(int index) {
  DCHECK(index >= 0 && index < GetBookmarkButtonCount());
  return static_cast<views::TextButton*>(child_at(index));
}

int BookmarkBarView::GetFirstHiddenNodeIndex() {
  const int bb_count = GetBookmarkButtonCount();
  for (int i = 0; i < bb_count; ++i) {
    if (!GetBookmarkButton(i)->IsVisible())
      return i;
  }
  return bb_count;
}

MenuButton* BookmarkBarView::CreateOtherBookmarkedButton() {
  MenuButton* button = new BookmarkFolderButton(
      this,
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OTHER_BOOKMARKED),
      this,
      false);
  button->set_id(VIEW_ID_OTHER_BOOKMARKS);
  button->SetIcon(GetFolderIcon());
  button->set_context_menu_controller(this);
  button->set_tag(kOtherFolderButtonTag);
  button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OTHER_BOOKMARKED));
  return button;
}

MenuButton* BookmarkBarView::CreateOverflowButton() {
  MenuButton* button = new OverFlowButton(this);
  button->SetIcon(*ResourceBundle::GetSharedInstance().
                  GetBitmapNamed(IDR_BOOKMARK_BAR_CHEVRONS));

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
    button->SetIcon(GetFolderIcon());
    ConfigureButton(node, button);
    return button;
  }
}

void BookmarkBarView::ConfigureButton(const BookmarkNode* node,
                                      views::TextButton* button) {
  button->SetText(node->GetTitle());
  button->SetAccessibleName(node->GetTitle());
  button->set_id(VIEW_ID_BOOKMARK_BAR_ELEMENT);
  // We don't always have a theme provider (ui tests, for example).
  if (GetThemeProvider()) {
    button->SetEnabledColor(GetThemeProvider()->GetColor(
        ThemeService::COLOR_BOOKMARK_TEXT));
  }

  button->ClearMaxTextSize();
  button->set_context_menu_controller(this);
  button->set_drag_controller(this);
  if (node->is_url()) {
    if (model_->GetFavicon(node).width() != 0)
      button->SetIcon(model_->GetFavicon(node));
    else
      button->SetIcon(*kDefaultFavicon);
  }
  button->set_max_width(kMaxButtonWidth);
}

void BookmarkBarView::BookmarkNodeAddedImpl(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            int index) {
  UpdateOtherBookmarksVisibility();
  if (parent != model_->bookmark_bar_node()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  DCHECK(index >= 0 && index <= GetBookmarkButtonCount());
  const BookmarkNode* node = parent->GetChild(index);
  ProfileSyncService* sync_service =
      browser_->profile()->GetProfileSyncService();
  if (!throbbing_view_ && sync_service && sync_service->SetupInProgress())
    StartThrobbing(node, true);
  AddChildViewAt(CreateBookmarkButton(node), index);
  UpdateColors();
  Layout();
  SchedulePaint();
}

void BookmarkBarView::BookmarkNodeRemovedImpl(BookmarkModel* model,
                                              const BookmarkNode* parent,
                                              int index) {
  UpdateOtherBookmarksVisibility();

  StopThrobbing(true);
  // No need to start throbbing again as the bookmark bubble can't be up at
  // the same time as the user reorders.

  if (parent != model_->bookmark_bar_node()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  DCHECK(index >= 0 && index < GetBookmarkButtonCount());
  views::View* button = child_at(index);
  RemoveChildView(button);
  MessageLoop::current()->DeleteSoon(FROM_HERE, button);
  Layout();
  SchedulePaint();
}

void BookmarkBarView::BookmarkNodeChangedImpl(BookmarkModel* model,
                                              const BookmarkNode* node) {
  if (node->parent() != model_->bookmark_bar_node()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  int index = model_->bookmark_bar_node()->GetIndexOf(node);
  DCHECK_NE(-1, index);
  views::TextButton* button = GetBookmarkButton(index);
  gfx::Size old_pref = button->GetPreferredSize();
  ConfigureButton(node, button);
  gfx::Size new_pref = button->GetPreferredSize();
  if (old_pref.width() != new_pref.width()) {
    Layout();
    SchedulePaint();
  } else if (button->IsVisible()) {
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
  bookmark_drop_menu_ = new BookmarkMenuController(browser_->profile(),
      page_navigator_, GetWidget(), node, start_index);
  bookmark_drop_menu_->set_observer(this);
  bookmark_drop_menu_->RunMenuAt(this, true);
}

void BookmarkBarView::StopShowFolderDropMenuTimer() {
  show_folder_method_factory_.InvalidateWeakPtrs();
}

void BookmarkBarView::StartShowFolderDropMenuTimer(const BookmarkNode* node) {
  if (testing_) {
    // So that tests can run as fast as possible disable the delay during
    // testing.
    ShowDropFolderForNode(node);
    return;
  }
  show_folder_method_factory_.InvalidateWeakPtrs();
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BookmarkBarView::ShowDropFolderForNode,
                 show_folder_method_factory_.GetWeakPtr(),
                 node),
      views::GetMenuShowDelay());
}

void BookmarkBarView::CalculateDropLocation(const DropTargetEvent& event,
                                            const BookmarkNodeData& data,
                                            DropLocation* location) {
  DCHECK(model_);
  DCHECK(model_->IsLoaded());
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
  if (other_bookmarked_button_->IsVisible() && other_delta_x >= 0 &&
      other_delta_x < other_bookmarked_button_->width()) {
    // Mouse is over 'other' folder.
    location->button_type = DROP_OTHER_FOLDER;
    location->on = true;
    found = true;
  } else if (!GetBookmarkButtonCount()) {
    // No bookmarks, accept the drop.
    location->index = 0;
    int ops = data.GetFirstNode(profile) ? ui::DragDropTypes::DRAG_MOVE :
        ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK;
    location->operation =
        bookmark_utils::PreferredDropOperation(event.source_operations(), ops);
    return;
  }

  for (int i = 0; i < GetBookmarkButtonCount() &&
       GetBookmarkButton(i)->IsVisible() && !found; i++) {
    views::TextButton* button = GetBookmarkButton(i);
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
    if (overflow_button_->IsVisible()) {
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
    } else if (!other_bookmarked_button_->IsVisible() ||
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
    location->operation =
        bookmark_utils::BookmarkDropOperation(profile, event, data, parent,
                                              parent->child_count());
    if (!location->operation && !data.has_single_url() &&
        data.GetFirstNode(profile) == parent) {
      // Don't open a menu if the node being dragged is the menu to open.
      location->on = false;
    }
  } else {
    location->operation = bookmark_utils::BookmarkDropOperation(profile, event,
        data, model_->bookmark_bar_node(), location->index);
  }
}

void BookmarkBarView::WriteBookmarkDragData(const BookmarkNode* node,
                                            ui::OSExchangeData* data) {
  DCHECK(node && data);
  BookmarkNodeData drag_data(node);
  drag_data.Write(browser_->profile(), data);
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
  // Node wasn't on the bookmark bar, use the other bookmark button.
  return other_bookmarked_button_;
}

void BookmarkBarView::UpdateColors() {
  // We don't always have a theme provider (ui tests, for example).
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return;
  SkColor text_color =
      theme_provider->GetColor(ThemeService::COLOR_BOOKMARK_TEXT);
  for (int i = 0; i < GetBookmarkButtonCount(); ++i)
    GetBookmarkButton(i)->SetEnabledColor(text_color);
  other_bookmarked_button()->SetEnabledColor(text_color);
}

void BookmarkBarView::UpdateOtherBookmarksVisibility() {
  bool has_other_children = !model_->other_node()->empty();
  if (has_other_children == other_bookmarked_button_->IsVisible())
    return;
  other_bookmarked_button_->SetVisible(has_other_children);
  bookmarks_separator_view_->SetVisible(has_other_children);
  Layout();
  SchedulePaint();
}

gfx::Size BookmarkBarView::LayoutItems(bool compute_bounds_only) {
  gfx::Size prefsize;
  if (!parent() && !compute_bounds_only)
    return prefsize;

  int x = kLeftMargin;
  int top_margin = IsDetached() ? kDetachedTopMargin : 0;
  int y = top_margin;
  int width = View::width() - kRightMargin - kLeftMargin;
  int height = -top_margin - kBottomMargin;
  int separator_margin = kSeparatorMargin;

  if (IsDetached()) {
    double current_state = 1 - size_animation_->GetCurrentValue();
    x += static_cast<int>(kNewtabHorizontalPadding * current_state);
    y += static_cast<int>(kNewtabVerticalPadding * current_state);
    width -= static_cast<int>(kNewtabHorizontalPadding * current_state);
    height += View::height() -
        static_cast<int>(kNewtabVerticalPadding * 2 * current_state);
    separator_margin -= static_cast<int>(kSeparatorMargin * current_state);
  } else {
    // For the attached appearance, pin the content to the bottom of the bar
    // when animating in/out, as shrinking its height instead looks weird.  This
    // also matches how we layout infobars.
    y += View::height() - browser_defaults::kBookmarkBarHeight;
    height += browser_defaults::kBookmarkBarHeight;
  }

  gfx::Size other_bookmarked_pref =
      other_bookmarked_button_->IsVisible() ?
      other_bookmarked_button_->GetPreferredSize() : gfx::Size();
  gfx::Size overflow_pref = overflow_button_->GetPreferredSize();
  gfx::Size bookmarks_separator_pref =
      bookmarks_separator_view_->GetPreferredSize();

  int max_x = width - overflow_pref.width() - kButtonPadding -
      bookmarks_separator_pref.width();
  if (other_bookmarked_button_->IsVisible())
    max_x -= other_bookmarked_pref.width() + kButtonPadding;

  // Next, layout out the buttons. Any buttons that are placed beyond the
  // visible region and made invisible.
  if (GetBookmarkButtonCount() == 0 && model_ && model_->IsLoaded()) {
    gfx::Size pref = instructions_->GetPreferredSize();
    if (!compute_bounds_only) {
      instructions_->SetBounds(
          x + kInstructionsPadding, y,
          std::min(static_cast<int>(pref.width()),
          max_x - x),
          height);
      instructions_->SetVisible(true);
    }
  } else {
    if (!compute_bounds_only)
      instructions_->SetVisible(false);

    for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
      views::View* child = child_at(i);
      gfx::Size pref = child->GetPreferredSize();
      int next_x = x + pref.width() + kButtonPadding;
      if (!compute_bounds_only) {
        child->SetVisible(next_x < max_x);
        child->SetBounds(x, y, pref.width(), height);
      }
      x = next_x;
    }
  }

  // Layout the right side of the bar.
  const bool all_visible =
      (GetBookmarkButtonCount() == 0 ||
       child_at(GetBookmarkButtonCount() - 1)->IsVisible());

  // Layout the right side buttons.
  if (!compute_bounds_only)
    x = max_x + kButtonPadding;
  else
    x += kButtonPadding;

  // The overflow button.
  if (!compute_bounds_only) {
    overflow_button_->SetBounds(x, y, overflow_pref.width(), height);
    overflow_button_->SetVisible(!all_visible);
  }
  x += overflow_pref.width();

  // Separator.
  if (bookmarks_separator_view_->IsVisible()) {
    if (!compute_bounds_only) {
      bookmarks_separator_view_->SetBounds(x,
                                           y - top_margin,
                                           bookmarks_separator_pref.width(),
                                           height + top_margin + kBottomMargin -
                                           separator_margin);
    }

    x += bookmarks_separator_pref.width();
  }

  // The other bookmarks button.
  if (other_bookmarked_button_->IsVisible()) {
    if (!compute_bounds_only) {
      other_bookmarked_button_->SetBounds(x, y, other_bookmarked_pref.width(),
                                          height);
    }
    x += other_bookmarked_pref.width() + kButtonPadding;
  }

  // Set the preferred size computed so far.
  if (compute_bounds_only) {
    x += kRightMargin;
    prefsize.set_width(x);
    if (IsDetached()) {
      x += static_cast<int>(
          kNewtabHorizontalPadding * (1 - size_animation_->GetCurrentValue()));
      prefsize.set_height(
          browser_defaults::kBookmarkBarHeight +
          static_cast<int>(
              (browser_defaults::kNewtabBookmarkBarHeight -
               browser_defaults::kBookmarkBarHeight) *
              (1 - size_animation_->GetCurrentValue())));
    } else {
      prefsize.set_height(
          static_cast<int>(
              browser_defaults::kBookmarkBarHeight *
              size_animation_->GetCurrentValue()));
    }
  }
  return prefsize;
}
