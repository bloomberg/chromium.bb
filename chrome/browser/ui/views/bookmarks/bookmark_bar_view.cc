// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include <algorithm>
#include <limits>
#include <set>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/page_navigator.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas_skia.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/label.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/drag_utils.h"
#include "views/metrics.h"
#include "views/view_constants.h"
#include "views/widget/tooltip_manager.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

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

// Preferred height of the bookmarks bar.
static const int kBarHeight = 28;

// Preferred height of the bookmarks bar when only shown on the new tab page.
const int BookmarkBarView::kNewtabBarHeight = 57;

// Padding between buttons.
static const int kButtonPadding = 0;

// Command ids used in the menu allowing the user to choose when we're visible.
static const int kAlwaysShowCommandID = 1;

// Icon to display when one isn't found for the page.
static SkBitmap* kDefaultFavIcon = NULL;

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

// Tag for the sync error button.
static const int kSyncErrorButtonTag = 2;

namespace {

// BookmarkButton -------------------------------------------------------------

// Buttons used for the bookmarks on the bookmark bar.

class BookmarkButton : public views::TextButton {
 public:
  BookmarkButton(views::ButtonListener* listener,
                 const GURL& url,
                 const std::wstring& title,
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

  bool GetTooltipText(const gfx::Point& p, std::wstring* tooltip) {
    gfx::Point location(p);
    ConvertPointToScreen(this, &location);
    *tooltip = BookmarkBarView::CreateToolTipForURLAndTitle(location, url_,
        text(), profile_);
    return !tooltip->empty();
  }

  virtual bool IsTriggerableEvent(const views::MouseEvent& e) {
    return event_utils::IsPossibleDispositionEvent(e);
  }

 private:
  const GURL& url_;
  Profile* profile_;
  scoped_ptr<ui::SlideAnimation> show_animation_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkButton);
};

// BookmarkFolderButton -------------------------------------------------------

// Buttons used for folders on the bookmark bar, including the 'other folders'
// button.
class BookmarkFolderButton : public views::MenuButton {
 public:
  BookmarkFolderButton(views::ButtonListener* listener,
                       const std::wstring& title,
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

  virtual bool IsTriggerableEvent(const views::MouseEvent& e) {
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
      : MenuButton(NULL, std::wstring(), owner, false),
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

// DropInfo -------------------------------------------------------------------

// Tracks drops on the BookmarkBarView.

struct BookmarkBarView::DropInfo {
  DropInfo()
      : valid(false),
        drop_index(-1),
        is_menu_showing(false),
        drop_on(false),
        is_over_overflow(false),
        is_over_other(false),
        x(0),
        y(0),
        drag_operation(0) {
  }

  // Whether the data is valid.
  bool valid;

  // Index into the model the drop is over. This is relative to the root node.
  int drop_index;

  // If true, the menu is being shown.
  bool is_menu_showing;

  // If true, the user is dropping on a node. This is only used for group
  // nodes.
  bool drop_on;

  // If true, the user is over the overflow button.
  bool is_over_overflow;

  // If true, the user is over the other button.
  bool is_over_other;

  // Coordinates of the drag (in terms of the BookmarkBarView).
  int x;
  int y;

  // The current drag operation.
  int drag_operation;

  // DropData for the drop.
  BookmarkNodeData data;
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
        GetThemeProvider()->GetColor(BrowserThemeProvider::COLOR_TOOLBAR));
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

// Returns the bitmap to use for starred groups.
static const SkBitmap& GetGroupIcon() {
  if (!kFolderIcon) {
    kFolderIcon = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_BOOKMARK_BAR_FOLDER);
  }
  return *kFolderIcon;
}

BookmarkBarView::BookmarkBarView(Profile* profile, Browser* browser)
    : profile_(NULL),
      page_navigator_(NULL),
      model_(NULL),
      bookmark_menu_(NULL),
      bookmark_drop_menu_(NULL),
      other_bookmarked_button_(NULL),
      model_changed_listener_(NULL),
      show_folder_drop_menu_task_(NULL),
      sync_error_button_(NULL),
      sync_service_(NULL),
      overflow_button_(NULL),
      instructions_(NULL),
      bookmarks_separator_view_(NULL),
      browser_(browser),
      infobar_visible_(false),
      throbbing_view_(NULL) {
  if (profile->GetProfileSyncService()) {
    // Obtain a pointer to the profile sync service and add our instance as an
    // observer.
    sync_service_ = profile->GetProfileSyncService();
    sync_service_->AddObserver(this);
  }

  SetID(VIEW_ID_BOOKMARK_BAR);
  Init();
  SetProfile(profile);

  size_animation_->Reset(IsAlwaysShown() ? 1 : 0);
}

BookmarkBarView::~BookmarkBarView() {
  NotifyModelChanged();
  if (model_)
    model_->RemoveObserver(this);

  // It's possible for the menu to outlive us, reset the observer to make sure
  // it doesn't have a reference to us.
  if (bookmark_menu_)
    bookmark_menu_->set_observer(NULL);

  StopShowFolderDropMenuTimer();

  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

void BookmarkBarView::SetProfile(Profile* profile) {
  DCHECK(profile);
  if (profile_ == profile)
    return;

  StopThrobbing(true);

  // Cancels the current cancelable.
  NotifyModelChanged();
  registrar_.RemoveAll();

  profile_ = profile;

  if (model_)
    model_->RemoveObserver(this);

  // Disable the other bookmarked button, we'll re-enable when the model is
  // loaded.
  other_bookmarked_button_->SetEnabled(false);

  Source<Profile> ns_source(profile_->GetOriginalProfile());
  registrar_.Add(this, NotificationType::BOOKMARK_BUBBLE_SHOWN, ns_source);
  registrar_.Add(this, NotificationType::BOOKMARK_BUBBLE_HIDDEN, ns_source);
  registrar_.Add(this, NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
                 NotificationService::AllSources());

  // Remove any existing bookmark buttons.
  while (GetBookmarkButtonCount())
    delete GetChildViewAt(0);

  model_ = profile_->GetBookmarkModel();
  if (model_) {
    model_->AddObserver(this);
    if (model_->IsLoaded())
      Loaded(model_);
    // else case: we'll receive notification back from the BookmarkModel when
    // done loading, then we'll populate the bar.
  }
}

void BookmarkBarView::SetPageNavigator(PageNavigator* navigator) {
  page_navigator_ = navigator;
}

gfx::Size BookmarkBarView::GetPreferredSize() {
  return LayoutItems(true);
}

gfx::Size BookmarkBarView::GetMinimumSize() {
  // The minimum width of the bookmark bar should at least contain the overflow
  // button, by which one can access all the Bookmark Bar items, and the "Other
  // Bookmarks" folder, along with appropriate margins and button padding.
  int width = kLeftMargin;

  if (OnNewTabPage()) {
    double current_state = 1 - size_animation_->GetCurrentValue();
    width += 2 * static_cast<int>(static_cast<double>
        (kNewtabHorizontalPadding) * current_state);
  }

  int sync_error_total_width = 0;
  gfx::Size sync_error_button_pref = sync_error_button_->GetPreferredSize();
  if (sync_ui_util::ShouldShowSyncErrorButton(sync_service_))
    sync_error_total_width += kButtonPadding + sync_error_button_pref.width();

  gfx::Size other_bookmarked_pref =
      other_bookmarked_button_->GetPreferredSize();
  gfx::Size overflow_pref = overflow_button_->GetPreferredSize();
  gfx::Size bookmarks_separator_pref =
      bookmarks_separator_view_->GetPreferredSize();

  width += (other_bookmarked_pref.width() + kButtonPadding +
      overflow_pref.width() + kButtonPadding +
      bookmarks_separator_pref.width() + sync_error_total_width);

  return gfx::Size(width, kBarHeight);
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
      drop_info_->drag_operation != 0 && drop_info_->drop_index != -1 &&
      !drop_info_->is_over_overflow && !drop_info_->drop_on) {
    int index = drop_info_->drop_index;
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
    canvas->FillRectInt(kDropIndicatorColor, indicator_bounds.x(),
                        indicator_bounds.y(), indicator_bounds.width(),
                        indicator_bounds.height());
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
  if (!model_ || !model_->IsLoaded())
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
    return drop_info_->drag_operation;
  }

  drop_info_->x = event.x();
  drop_info_->y = event.y();

  int drop_index;
  bool drop_on;
  bool is_over_overflow;
  bool is_over_other;

  drop_info_->drag_operation = CalculateDropOperation(
      event, drop_info_->data, &drop_index, &drop_on, &is_over_overflow,
      &is_over_other);

  if (drop_info_->valid && drop_info_->drop_index == drop_index &&
      drop_info_->drop_on == drop_on &&
      drop_info_->is_over_overflow == is_over_overflow &&
      drop_info_->is_over_other == is_over_other) {
    // The position we're going to drop didn't change, return the last drag
    // operation we calculated.
    return drop_info_->drag_operation;
  }

  drop_info_->valid = true;

  StopShowFolderDropMenuTimer();

  // TODO(sky): Optimize paint region.
  SchedulePaint();
  drop_info_->drop_index = drop_index;
  drop_info_->drop_on = drop_on;
  drop_info_->is_over_overflow = is_over_overflow;
  drop_info_->is_over_other = is_over_other;

  if (drop_info_->is_menu_showing) {
    if (bookmark_drop_menu_)
      bookmark_drop_menu_->Cancel();
    drop_info_->is_menu_showing = false;
  }

  if (drop_on || is_over_overflow || is_over_other) {
    const BookmarkNode* node;
    if (is_over_other)
      node = model_->other_node();
    else if (is_over_overflow)
      node = model_->GetBookmarkBarNode();
    else
      node = model_->GetBookmarkBarNode()->GetChild(drop_index);
    StartShowFolderDropMenuTimer(node);
  }

  return drop_info_->drag_operation;
}

void BookmarkBarView::OnDragExited() {
  StopShowFolderDropMenuTimer();

  // NOTE: we don't hide the menu on exit as it's possible the user moved the
  // mouse over the menu, which triggers an exit on us.

  drop_info_->valid = false;

  if (drop_info_->drop_index != -1) {
    // TODO(sky): optimize the paint region.
    SchedulePaint();
  }
  drop_info_.reset();
}

int BookmarkBarView::OnPerformDrop(const DropTargetEvent& event) {
  StopShowFolderDropMenuTimer();

  if (bookmark_drop_menu_)
    bookmark_drop_menu_->Cancel();

  if (!drop_info_.get() || !drop_info_->drag_operation)
    return ui::DragDropTypes::DRAG_NONE;

  const BookmarkNode* root =
      drop_info_->is_over_other ? model_->other_node() :
                                  model_->GetBookmarkBarNode();
  int index = drop_info_->drop_index;
  const bool drop_on = drop_info_->drop_on;
  const BookmarkNodeData data = drop_info_->data;
  const bool is_over_other = drop_info_->is_over_other;
  DCHECK(data.is_valid());

  if (drop_info_->drop_index != -1) {
    // TODO(sky): optimize the SchedulePaint region.
    SchedulePaint();
  }
  drop_info_.reset();

  const BookmarkNode* parent_node;
  if (is_over_other) {
    parent_node = root;
    index = parent_node->child_count();
  } else if (drop_on) {
    parent_node = root->GetChild(index);
    index = parent_node->child_count();
  } else {
    parent_node = root;
  }
  return bookmark_utils::PerformBookmarkDrop(profile_, data, parent_node,
                                             index);
}

void BookmarkBarView::ShowContextMenu(const gfx::Point& p,
                                      bool is_mouse_gesture) {
  ShowContextMenuForView(this, p, is_mouse_gesture);
}

void BookmarkBarView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_TOOLBAR;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_BOOKMARKS);
}

void BookmarkBarView::OnStateChanged() {
  // When the sync state changes, it is sufficient to invoke View::Layout since
  // during layout we query the profile sync service and determine whether the
  // new state requires showing the sync error button so that the user can
  // re-enter her password. If extension shelf appears along with the bookmark
  // shelf, it too needs to be layed out. Since both have the same parent, it is
  // enough to let the parent layout both of these children.
  // TODO(sky): This should not require Layout() and SchedulePaint(). Needs
  //            some cleanup.
  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void BookmarkBarView::OnFullscreenToggled(bool fullscreen) {
  if (!fullscreen)
    size_animation_->Reset(IsAlwaysShown() ? 1 : 0);
  else if (IsAlwaysShown())
    size_animation_->Reset(0);
}

bool BookmarkBarView::IsDetached() const {
  return OnNewTabPage() && (size_animation_->GetCurrentValue() != 1);
}

bool BookmarkBarView::IsOnTop() const {
  return true;
}

double BookmarkBarView::GetAnimationValue() const {
  return size_animation_->GetCurrentValue();
}

int BookmarkBarView::GetToolbarOverlap() const {
  return GetToolbarOverlap(false);
}

bool BookmarkBarView::IsAlwaysShown() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

bool BookmarkBarView::OnNewTabPage() const {
  return (browser_ && browser_->GetSelectedTabContents() &&
          browser_->GetSelectedTabContents()->ShouldShowBookmarkBar());
}

int BookmarkBarView::GetToolbarOverlap(bool return_max) const {
  // When not on the New Tab Page, always overlap by the full amount.
  if (return_max || !OnNewTabPage())
    return kToolbarOverlap;
  // When on the New Tab Page with an infobar, overlap by 0 whenever the infobar
  // is above us (i.e. when we're detached), since drawing over the infobar
  // looks weird.
  if (IsDetached() && infobar_visible_)
    return 0;
  // When on the New Tab Page with no infobar, animate the overlap between the
  // attached and detached states.
  return static_cast<int>(static_cast<double>(kToolbarOverlap) *
      size_animation_->GetCurrentValue());
}

bool BookmarkBarView::is_animating() {
  return size_animation_->is_animating();
}

void BookmarkBarView::AnimationProgressed(const ui::Animation* animation) {
  if (browser_)
    browser_->ToolbarSizeChanged(true);
}

void BookmarkBarView::AnimationEnded(const ui::Animation* animation) {
  if (browser_)
    browser_->ToolbarSizeChanged(false);

  SchedulePaint();
}

void BookmarkBarView::BookmarkMenuDeleted(BookmarkMenuController* controller) {
  if (controller == bookmark_menu_)
    bookmark_menu_ = NULL;
  else if (controller == bookmark_drop_menu_)
    bookmark_drop_menu_ = NULL;
}

views::TextButton* BookmarkBarView::GetBookmarkButton(int index) {
  DCHECK(index >= 0 && index < GetBookmarkButtonCount());
  return static_cast<views::TextButton*>(GetChildViewAt(index));
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

const BookmarkNode* BookmarkBarView::GetNodeForButtonAt(const gfx::Point& loc,
                                                        int* start_index) {
  *start_index = 0;

  if (loc.x() < 0 || loc.x() >= width() || loc.y() < 0 || loc.y() >= height())
    return NULL;

  gfx::Point adjusted_loc(GetMirroredXInView(loc.x()), loc.y());

  // Check the buttons first.
  for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
    views::View* child = GetChildViewAt(i);
    if (!child->IsVisible())
      break;
    if (child->bounds().Contains(adjusted_loc))
      return model_->GetBookmarkBarNode()->GetChild(i);
  }

  // Then the overflow button.
  if (overflow_button_->IsVisible() &&
      overflow_button_->bounds().Contains(adjusted_loc)) {
    *start_index = GetFirstHiddenNodeIndex();
    return model_->GetBookmarkBarNode();
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
  if (node == model_->GetBookmarkBarNode())
    return overflow_button_;
  int index = model_->GetBookmarkBarNode()->GetIndexOf(node);
  if (index == -1 || !node->is_folder())
    return NULL;
  return static_cast<views::MenuButton*>(GetChildViewAt(index));
}

void BookmarkBarView::GetAnchorPositionAndStartIndexForButton(
    views::MenuButton* button,
    MenuItemView::AnchorPosition* anchor,
    int* start_index) {
  if (button == other_bookmarked_button_ || button == overflow_button_)
    *anchor = MenuItemView::TOPRIGHT;
  else
    *anchor = MenuItemView::TOPLEFT;

  // Invert orientation if right to left.
  if (base::i18n::IsRTL()) {
    if (*anchor == MenuItemView::TOPRIGHT)
      *anchor = MenuItemView::TOPLEFT;
    else
      *anchor = MenuItemView::TOPRIGHT;
  }

  if (button == overflow_button_)
    *start_index = GetFirstHiddenNodeIndex();
  else
    *start_index = 0;
}

void BookmarkBarView::ShowImportDialog() {
  browser_->OpenImportSettingsDialog();
}

void BookmarkBarView::Init() {
  // Note that at this point we're not in a hierarchy so GetThemeProvider() will
  // return NULL.  When we're inserted into a hierarchy, we'll call
  // UpdateColors(), which will set the appropriate colors for all the objects
  // added in this function.

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  if (!kDefaultFavIcon)
    kDefaultFavIcon = rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);

  // Child views are traversed in the order they are added. Make sure the order
  // they are added matches the visual order.
  sync_error_button_ = CreateSyncErrorButton();
  AddChildView(sync_error_button_);

  overflow_button_ = CreateOverflowButton();
  AddChildView(overflow_button_);

  other_bookmarked_button_ = CreateOtherBookmarkedButton();
  AddChildView(other_bookmarked_button_);

  bookmarks_separator_view_ = new ButtonSeparatorView();
  AddChildView(bookmarks_separator_view_);

  instructions_ = new BookmarkBarInstructionsView(this);
  AddChildView(instructions_);

  SetContextMenuController(this);

  size_animation_.reset(new ui::SlideAnimation(this));
}

MenuButton* BookmarkBarView::CreateOtherBookmarkedButton() {
  MenuButton* button = new BookmarkFolderButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_BOOMARK_BAR_OTHER_BOOKMARKED)),
      this,
      false);
  button->SetID(VIEW_ID_OTHER_BOOKMARKS);
  button->SetIcon(GetGroupIcon());
  button->SetContextMenuController(this);
  button->set_tag(kOtherFolderButtonTag);
  button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_BOOMARK_BAR_OTHER_BOOKMARKED));
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

void BookmarkBarView::Loaded(BookmarkModel* model) {
  volatile int button_count = GetBookmarkButtonCount();
  DCHECK(button_count == 0);  // If non-zero it means Load was invoked more than
                              // once, or we didn't properly clear things.
                              // Either of which shouldn't happen
  const BookmarkNode* node = model_->GetBookmarkBarNode();
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
  NotifyModelChanged();
  model_->RemoveObserver(this);
  model_ = NULL;
}

void BookmarkBarView::BookmarkNodeMoved(BookmarkModel* model,
                                        const BookmarkNode* old_parent,
                                        int old_index,
                                        const BookmarkNode* new_parent,
                                        int new_index) {
  BookmarkNodeRemovedImpl(model, old_parent, old_index);
  BookmarkNodeAddedImpl(model, new_parent, new_index);
}

void BookmarkBarView::BookmarkNodeAdded(BookmarkModel* model,
                                        const BookmarkNode* parent,
                                        int index) {
  BookmarkNodeAddedImpl(model, parent, index);
}

void BookmarkBarView::BookmarkNodeAddedImpl(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            int index) {
  UpdateOtherBookmarksVisibility();
  NotifyModelChanged();
  if (parent != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  DCHECK(index >= 0 && index <= GetBookmarkButtonCount());
  const BookmarkNode* node = parent->GetChild(index);
  if (!throbbing_view_ && sync_service_ && sync_service_->SetupInProgress()) {
    StartThrobbing(node, true);
  }
  AddChildViewAt(CreateBookmarkButton(node), index);
  UpdateColors();
  Layout();
  SchedulePaint();
}

void BookmarkBarView::BookmarkNodeRemoved(BookmarkModel* model,
                                          const BookmarkNode* parent,
                                          int old_index,
                                          const BookmarkNode* node) {
  BookmarkNodeRemovedImpl(model, parent, old_index);
}

void BookmarkBarView::BookmarkNodeRemovedImpl(BookmarkModel* model,
                                              const BookmarkNode* parent,
                                              int index) {
  UpdateOtherBookmarksVisibility();

  StopThrobbing(true);
  // No need to start throbbing again as the bookmark bubble can't be up at
  // the same time as the user reorders.

  NotifyModelChanged();
  if (parent != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  DCHECK(index >= 0 && index < GetBookmarkButtonCount());
  views::View* button = GetChildViewAt(index);
  RemoveChildView(button);
  MessageLoop::current()->DeleteSoon(FROM_HERE, button);
  Layout();
  SchedulePaint();
}

void BookmarkBarView::BookmarkNodeChanged(BookmarkModel* model,
                                          const BookmarkNode* node) {
  NotifyModelChanged();
  BookmarkNodeChangedImpl(model, node);
}

void BookmarkBarView::BookmarkNodeChangedImpl(BookmarkModel* model,
                                              const BookmarkNode* node) {
  if (node->parent() != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  int index = model_->GetBookmarkBarNode()->GetIndexOf(node);
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

void BookmarkBarView::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  NotifyModelChanged();
  if (node != model_->GetBookmarkBarNode())
    return;  // We only care about reordering of the bookmark bar node.

  // Remove the existing buttons.
  while (GetBookmarkButtonCount()) {
    views::View* button = GetChildViewAt(0);
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

void BookmarkBarView::BookmarkNodeFaviconLoaded(BookmarkModel* model,
                                                const BookmarkNode* node) {
  BookmarkNodeChangedImpl(model, node);
}

void BookmarkBarView::WriteDragDataForView(View* sender,
                                           const gfx::Point& press_pt,
                                           ui::OSExchangeData* data) {
  UserMetrics::RecordAction(UserMetricsAction("BookmarkBar_DragButton"),
                            profile_);

  for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
    if (sender == GetBookmarkButton(i)) {
      views::TextButton* button = GetBookmarkButton(i);
      gfx::CanvasSkia canvas(button->width(), button->height(), false);
      button->PaintButton(&canvas, views::TextButton::PB_FOR_DRAG);
      drag_utils::SetDragImageOnDataObject(canvas, button->size(), press_pt,
                                           data);
      WriteBookmarkDragData(model_->GetBookmarkBarNode()->GetChild(i), data);
      return;
    }
  }
  NOTREACHED();
}

int BookmarkBarView::GetDragOperationsForView(View* sender,
                                              const gfx::Point& p) {
  if (size_animation_->is_animating() ||
      (size_animation_->GetCurrentValue() == 0 && !OnNewTabPage())) {
    // Don't let the user drag while animating open or we're closed (and not on
    // the new tab page, on the new tab page size_animation_ is always 0). This
    // typically is only hit if the user does something to inadvertanty trigger
    // dnd, such as pressing the mouse and hitting control-b.
    return ui::DragDropTypes::DRAG_NONE;
  }

  for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
    if (sender == GetBookmarkButton(i)) {
      return bookmark_utils::BookmarkDragOperation(
          model_->GetBookmarkBarNode()->GetChild(i));
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
        const BookmarkNode* node = model_->GetBookmarkBarNode()->GetChild(i);
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

void BookmarkBarView::WriteBookmarkDragData(const BookmarkNode* node,
                                            ui::OSExchangeData* data) {
  DCHECK(node && data);
  BookmarkNodeData drag_data(node);
  drag_data.Write(profile_, data);
}

void BookmarkBarView::RunMenu(views::View* view, const gfx::Point& pt) {
  const BookmarkNode* node;

  int start_index = 0;
  if (view == other_bookmarked_button_) {
    node = model_->other_node();
  } else if (view == overflow_button_) {
    node = model_->GetBookmarkBarNode();
    start_index = GetFirstHiddenNodeIndex();
  } else {
    int button_index = GetIndexOf(view);
    DCHECK_NE(-1, button_index);
    node = model_->GetBookmarkBarNode()->GetChild(button_index);
  }

  bookmark_menu_ = new BookmarkMenuController(browser_, profile_,
      page_navigator_, GetWindow()->GetNativeWindow(), node, start_index);
  bookmark_menu_->set_observer(this);
  bookmark_menu_->RunMenuAt(this, false);
}

void BookmarkBarView::ButtonPressed(views::Button* sender,
                                    const views::Event& event) {
  // Show the login wizard if the user clicked the re-login button.
  if (sender->tag() == kSyncErrorButtonTag) {
    DCHECK(sender == sync_error_button_);
    DCHECK(sync_service_ && !sync_service_->IsManaged());
    sync_service_->ShowErrorUI(GetWindow()->GetNativeWindow());
    return;
  }

  const BookmarkNode* node;
  if (sender->tag() == kOtherFolderButtonTag) {
    node = model_->other_node();
  } else {
    int index = GetIndexOf(sender);
    DCHECK_NE(-1, index);
    node = model_->GetBookmarkBarNode()->GetChild(index);
  }
  DCHECK(page_navigator_);

  WindowOpenDisposition disposition_from_event_flags =
      event_utils::DispositionFromEventFlags(sender->mouse_event_flags());

  if (node->is_url()) {
    RecordAppLaunch(profile_, node->GetURL());
    page_navigator_->OpenURL(node->GetURL(), GURL(),
        disposition_from_event_flags, PageTransition::AUTO_BOOKMARK);
  } else {
    bookmark_utils::OpenAll(GetWindow()->GetNativeWindow(), profile_,
        GetPageNavigator(), node, disposition_from_event_flags);
  }
  UserMetrics::RecordAction(UserMetricsAction("ClickedBookmarkBarURLButton"),
                            profile_);
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
        model_->GetBookmarkBarNode()->GetChild(bookmark_button_index);
    nodes.push_back(node);
    parent = node->parent();
  } else {
    parent = model_->GetBookmarkBarNode();
    nodes.push_back(parent);
  }
  // Browser may be null during testing.
  PageNavigator* navigator =
      browser() ? browser()->GetSelectedTabContents() : NULL;
  BookmarkContextMenu controller(GetWindow()->GetNativeWindow(), GetProfile(),
                                 navigator, parent, nodes);
  controller.RunMenuAt(p);
}

views::View* BookmarkBarView::CreateBookmarkButton(const BookmarkNode* node) {
  if (node->is_url()) {
    BookmarkButton* button = new BookmarkButton(this, node->GetURL(),
        UTF16ToWide(node->GetTitle()), GetProfile());
    ConfigureButton(node, button);
    return button;
  } else {
    views::MenuButton* button = new BookmarkFolderButton(this,
        UTF16ToWide(node->GetTitle()), this, false);
    button->SetIcon(GetGroupIcon());
    ConfigureButton(node, button);
    return button;
  }
}

void BookmarkBarView::ConfigureButton(const BookmarkNode* node,
                                      views::TextButton* button) {
  button->SetText(UTF16ToWide(node->GetTitle()));
  button->SetAccessibleName(node->GetTitle());
  button->SetID(VIEW_ID_BOOKMARK_BAR_ELEMENT);
  // We don't always have a theme provider (ui tests, for example).
  if (GetThemeProvider()) {
    button->SetEnabledColor(GetThemeProvider()->GetColor(
        BrowserThemeProvider::COLOR_BOOKMARK_TEXT));
  }

  button->ClearMaxTextSize();
  button->SetContextMenuController(this);
  button->SetDragController(this);
  if (node->is_url()) {
    if (model_->GetFavicon(node).width() != 0)
      button->SetIcon(model_->GetFavicon(node));
    else
      button->SetIcon(*kDefaultFavIcon);
  }
  button->set_max_width(kMaxButtonWidth);
}

bool BookmarkBarView::IsItemChecked(int id) const {
  DCHECK(id == kAlwaysShowCommandID);
  return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

void BookmarkBarView::ExecuteCommand(int id) {
  bookmark_utils::ToggleWhenVisible(profile_);
}

void BookmarkBarView::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(profile_);
  switch (type.value) {
    case NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED:
      if (IsAlwaysShown()) {
        size_animation_->Show();
      } else {
        size_animation_->Hide();
      }
      break;

    case NotificationType::BOOKMARK_BUBBLE_SHOWN: {
      StopThrobbing(true);
      GURL url = *(Details<GURL>(details).ptr());
      const BookmarkNode* node = model_->GetMostRecentlyAddedNodeForURL(url);
      if (!node)
        return;  // Generally shouldn't happen.
      StartThrobbing(node, false);
      break;
    }
    case NotificationType::BOOKMARK_BUBBLE_HIDDEN:
      StopThrobbing(false);
      break;

    default:
      NOTREACHED();
      break;
  }
}

void BookmarkBarView::OnThemeChanged() {
  UpdateColors();
}

void BookmarkBarView::NotifyModelChanged() {
  if (model_changed_listener_)
    model_changed_listener_->ModelChanged();
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
  if (node == model_->GetBookmarkBarNode())
    start_index = GetFirstHiddenNodeIndex();

  drop_info_->is_menu_showing = true;
  bookmark_drop_menu_ = new BookmarkMenuController(browser_, profile_,
      page_navigator_, GetWindow()->GetNativeWindow(), node, start_index);
  bookmark_drop_menu_->set_observer(this);
  bookmark_drop_menu_->RunMenuAt(this, true);
}

void BookmarkBarView::StopShowFolderDropMenuTimer() {
  if (show_folder_drop_menu_task_)
    show_folder_drop_menu_task_->Cancel();
}

void BookmarkBarView::StartShowFolderDropMenuTimer(const BookmarkNode* node) {
  if (testing_) {
    // So that tests can run as fast as possible disable the delay during
    // testing.
    ShowDropFolderForNode(node);
    return;
  }
  DCHECK(!show_folder_drop_menu_task_);
  show_folder_drop_menu_task_ = new ShowFolderDropMenuTask(this, node);
  int delay = views::GetMenuShowDelay();
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          show_folder_drop_menu_task_, delay);
}

int BookmarkBarView::CalculateDropOperation(const DropTargetEvent& event,
                                            const BookmarkNodeData& data,
                                            int* index,
                                            bool* drop_on,
                                            bool* is_over_overflow,
                                            bool* is_over_other) {
  DCHECK(model_);
  DCHECK(model_->IsLoaded());
  DCHECK(data.is_valid());

  // The drop event uses the screen coordinates while the child Views are
  // always laid out from left to right (even though they are rendered from
  // right-to-left on RTL locales). Thus, in order to make sure the drop
  // coordinates calculation works, we mirror the event's X coordinate if the
  // locale is RTL.
  int mirrored_x = GetMirroredXInView(event.x());

  *index = -1;
  *drop_on = false;
  *is_over_other = *is_over_overflow = false;

  bool found = false;
  const int other_delta_x = mirrored_x - other_bookmarked_button_->x();
  if (other_bookmarked_button_->IsVisible() && other_delta_x >= 0 &&
      other_delta_x < other_bookmarked_button_->width()) {
    // Mouse is over 'other' folder.
    *is_over_other = true;
    *drop_on = true;
    found = true;
  } else if (!GetBookmarkButtonCount()) {
    // No bookmarks, accept the drop.
    *index = 0;
    int ops = data.GetFirstNode(profile_)
        ? ui::DragDropTypes::DRAG_MOVE
        : ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK;
    return bookmark_utils::PreferredDropOperation(event.source_operations(),
                                                  ops);
  }

  for (int i = 0; i < GetBookmarkButtonCount() &&
       GetBookmarkButton(i)->IsVisible() && !found; i++) {
    views::TextButton* button = GetBookmarkButton(i);
    int button_x = mirrored_x - button->x();
    int button_w = button->width();
    if (button_x < button_w) {
      found = true;
      const BookmarkNode* node = model_->GetBookmarkBarNode()->GetChild(i);
      if (node->is_folder()) {
        if (button_x <= views::kDropBetweenPixels) {
          *index = i;
        } else if (button_x < button_w - views::kDropBetweenPixels) {
          *index = i;
          *drop_on = true;
        } else {
          *index = i + 1;
        }
      } else if (button_x < button_w / 2) {
        *index = i;
      } else {
        *index = i + 1;
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
        *index = GetFirstHiddenNodeIndex();
        *is_over_overflow = true;
      } else if (overflow_delta_x < 0) {
        // Mouse is after the last visible button but before overflow button;
        // use the last visible index.
        *index = GetFirstHiddenNodeIndex();
      } else {
        return ui::DragDropTypes::DRAG_NONE;
      }
    } else if (!other_bookmarked_button_->IsVisible() ||
               mirrored_x < other_bookmarked_button_->x()) {
      // Mouse is after the last visible button but before more recently
      // bookmarked; use the last visible index.
      *index = GetFirstHiddenNodeIndex();
    } else {
      return ui::DragDropTypes::DRAG_NONE;
    }
  }

  if (*drop_on) {
    const BookmarkNode* parent =
        *is_over_other ? model_->other_node() :
                         model_->GetBookmarkBarNode()->GetChild(*index);
    int operation =
        bookmark_utils::BookmarkDropOperation(profile_, event, data, parent,
                                              parent->child_count());
    if (!operation && !data.has_single_url() &&
        data.GetFirstNode(profile_) == parent) {
      // Don't open a menu if the node being dragged is the the menu to
      // open.
      *drop_on = false;
    }
    return operation;
  }
  return bookmark_utils::BookmarkDropOperation(profile_, event, data,
                                               model_->GetBookmarkBarNode(),
                                               *index);
}

int BookmarkBarView::GetFirstHiddenNodeIndex() {
  const int bb_count = GetBookmarkButtonCount();
  for (int i = 0; i < bb_count; ++i) {
    if (!GetBookmarkButton(i)->IsVisible())
      return i;
  }
  return bb_count;
}

void BookmarkBarView::StartThrobbing(const BookmarkNode* node,
                                     bool overflow_only) {
  DCHECK(!throbbing_view_);

  // Determine which visible button is showing the bookmark (or is an ancestor
  // of the bookmark).
  const BookmarkNode* bbn = model_->GetBookmarkBarNode();
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
      throbbing_view_ = static_cast<CustomButton*>(GetChildViewAt(index));
    }
  } else if (!overflow_only) {
    throbbing_view_ = other_bookmarked_button_;
  }

  // Use a large number so that the button continues to throb.
  if (throbbing_view_)
    throbbing_view_->StartThrobbing(std::numeric_limits<int>::max());
}

int BookmarkBarView::GetBookmarkButtonCount() {
  // We contain five non-bookmark button views: other bookmarks, bookmarks
  // separator, chevrons (for overflow), the instruction label and the sync
  // error button.
  return child_count() - 5;
}

void BookmarkBarView::StopThrobbing(bool immediate) {
  if (!throbbing_view_)
    return;

  // If not immediate, cycle through 2 more complete cycles.
  throbbing_view_->StartThrobbing(immediate ? 0 : 4);
  throbbing_view_ = NULL;
}

// static
std::wstring BookmarkBarView::CreateToolTipForURLAndTitle(
    const gfx::Point& screen_loc,
    const GURL& url,
    const std::wstring& title,
    Profile* profile) {
  int max_width = views::TooltipManager::GetMaxWidth(screen_loc.x(),
                                                     screen_loc.y());
  gfx::Font tt_font = views::TooltipManager::GetDefaultFont();
  std::wstring result;

  // First the title.
  if (!title.empty()) {
    std::wstring localized_title = title;
    base::i18n::AdjustStringForLocaleDirection(&localized_title);
    result.append(UTF16ToWideHack(ui::ElideText(WideToUTF16Hack(
        localized_title), tt_font, max_width, false)));
  }

  // Only show the URL if the url and title differ.
  if (title != UTF8ToWide(url.spec())) {
    if (!result.empty())
      result.append(views::TooltipManager::GetLineSeparator());

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
    result.append(UTF16ToWideHack(elided_url));
  }
  return result;
}

void BookmarkBarView::UpdateColors() {
  // We don't always have a theme provider (ui tests, for example).
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return;
  SkColor text_color =
      theme_provider->GetColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT);
  for (int i = 0; i < GetBookmarkButtonCount(); ++i)
    GetBookmarkButton(i)->SetEnabledColor(text_color);
  other_bookmarked_button()->SetEnabledColor(text_color);
}

void BookmarkBarView::UpdateOtherBookmarksVisibility() {
  bool has_other_children = model_->other_node()->child_count() > 0;
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
  int height = View::height() - top_margin - kBottomMargin;
  int separator_margin = kSeparatorMargin;

  if (OnNewTabPage()) {
    double current_state = 1 - size_animation_->GetCurrentValue();
    x += static_cast<int>(static_cast<double>
        (kNewtabHorizontalPadding) * current_state);
    y += static_cast<int>(static_cast<double>
        (kNewtabVerticalPadding) * current_state);
    width -= static_cast<int>(static_cast<double>
        (kNewtabHorizontalPadding) * current_state);
    height -= static_cast<int>(static_cast<double>
        (kNewtabVerticalPadding * 2) * current_state);
    separator_margin -= static_cast<int>(static_cast<double>
        (kSeparatorMargin) * current_state);
  }

  gfx::Size other_bookmarked_pref =
      other_bookmarked_button_->IsVisible() ?
      other_bookmarked_button_->GetPreferredSize() : gfx::Size();
  gfx::Size overflow_pref = overflow_button_->GetPreferredSize();
  gfx::Size bookmarks_separator_pref =
      bookmarks_separator_view_->GetPreferredSize();

  int sync_error_total_width = 0;
  gfx::Size sync_error_button_pref = sync_error_button_->GetPreferredSize();
  if (sync_ui_util::ShouldShowSyncErrorButton(sync_service_)) {
    sync_error_total_width += kButtonPadding + sync_error_button_pref.width();
  }
  int max_x = width - overflow_pref.width() - kButtonPadding -
      bookmarks_separator_pref.width() - sync_error_total_width;
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
      views::View* child = GetChildViewAt(i);
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
       GetChildViewAt(GetBookmarkButtonCount() - 1)->IsVisible());

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

  // Set the real bounds of the sync error button only if it needs to appear on
  // the bookmarks bar.
  if (sync_ui_util::ShouldShowSyncErrorButton(sync_service_)) {
    x += kButtonPadding;
    if (!compute_bounds_only) {
      sync_error_button_->SetBounds(
          x, y, sync_error_button_pref.width(), height);
      sync_error_button_->SetVisible(true);
    }
    x += sync_error_button_pref.width();
  } else if (!compute_bounds_only) {
    sync_error_button_->SetBounds(x, y, 0, height);
    sync_error_button_->SetVisible(false);
  }

  // Set the preferred size computed so far.
  if (compute_bounds_only) {
    x += kRightMargin;
    prefsize.set_width(x);
    if (OnNewTabPage()) {
      x += static_cast<int>(static_cast<double>(kNewtabHorizontalPadding) *
          (1 - size_animation_->GetCurrentValue()));
      prefsize.set_height(kBarHeight + static_cast<int>(static_cast<double>
                          (kNewtabBarHeight - kBarHeight) *
                          (1 - size_animation_->GetCurrentValue())));
    } else {
      prefsize.set_height(static_cast<int>(static_cast<double>(kBarHeight) *
                          size_animation_->GetCurrentValue()));
    }
  }
  return prefsize;
}

views::TextButton* BookmarkBarView::CreateSyncErrorButton() {
  views::TextButton* sync_error_button =
      new views::TextButton(this, UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_SYNC_BOOKMARK_BAR_ERROR)));
  sync_error_button->set_tag(kSyncErrorButtonTag);

  // The tooltip is the only way we have to display text explaining the error
  // to the user.
  sync_error_button->SetTooltipText(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_SYNC_BOOKMARK_BAR_ERROR_DESC)));
  sync_error_button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SYNC_ERROR_BUTTON));
  sync_error_button->SetIcon(
      *ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_WARNING));
  return sync_error_button;
}
