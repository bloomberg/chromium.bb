// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_service.h"
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
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_instructions_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_drag_drop_views.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/base/theme_provider.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/canvas.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/drag_utils.h"
#include "ui/views/metrics.h"
#include "ui/views/view_constants.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

using content::OpenURLParams;
using content::PageNavigator;
using content::Referrer;
using content::UserMetricsAction;
using ui::DropTargetEvent;
using views::CustomButton;
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
static gfx::ImageSkia* kDefaultFavicon = NULL;

// Icon used for folders.
static gfx::ImageSkia* kFolderIcon = NULL;

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

// TODO(kuan): change chrome::kNTPBookmarkBarHeight to this new height when
// search_ntp replaces ntp4; for now, while both versions exist, this new height
// is only needed locally.
static const int kSearchNewTabBookmarkBarHeight = 40;

// TODO(kuan): change BookmarkBarView::kNewtabHorizontalPadding and
// BookmarkBarView::kNewtabVerticalPadding to these new values when search_ntp
// replaces ntp4; for now, while both versions exist, these new values are only
// needed locally.
static const int kSearchNewTabHorizontalPadding = 2;
static const int kSearchNewTabVerticalPadding = 5;

// Tag for the 'Apps Shortcut' button.
static const int kAppsShortcutButtonTag = 2;

namespace {

// To enable/disable BookmarkBar animations during testing. In production
// animations are enabled by default.
bool animations_enabled = true;

// BookmarkButtonBase -----------------------------------------------

// Base class for text buttons used on the bookmark bar.

class BookmarkButtonBase : public views::TextButton {
 public:
  BookmarkButtonBase(views::ButtonListener* listener,
                     const string16& title)
      : TextButton(listener, title) {
    show_animation_.reset(new ui::SlideAnimation(this));
    if (!animations_enabled) {
      // For some reason during testing the events generated by animating
      // throw off the test. So, don't animate while testing.
      show_animation_->Reset(1);
    } else {
      show_animation_->Show();
    }
  }

  virtual bool IsTriggerableEvent(const ui::Event& e) OVERRIDE {
    return e.type() == ui::ET_GESTURE_TAP ||
           e.type() == ui::ET_GESTURE_TAP_DOWN ||
           event_utils::IsPossibleDispositionEvent(e);
  }

 private:
  scoped_ptr<ui::SlideAnimation> show_animation_;

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
                 const string16& title,
                 Profile* profile)
      : BookmarkButtonBase(listener, title),
        url_(url),
        profile_(profile) {
  }

  virtual bool GetTooltipText(const gfx::Point& p,
                              string16* tooltip) const OVERRIDE {
    gfx::Point location(p);
    ConvertPointToScreen(this, &location);
    *tooltip = BookmarkBarView::CreateToolTipForURLAndTitle(
        location, url_, text(), profile_, GetWidget()->GetNativeView());
    return !tooltip->empty();
  }

  virtual std::string GetClassName() const OVERRIDE {
    return kViewClassName;
  }

 private:
  const GURL& url_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkButton);
};

// static for BookmarkButton
const char BookmarkButton::kViewClassName[] =
    "browser/ui/views/bookmarks/BookmarkButton";

// ShortcutButton -------------------------------------------------------------

// Buttons used for the shortcuts on the bookmark bar.

class ShortcutButton : public BookmarkButtonBase {
 public:
  // The internal view class name.
  static const char kViewClassName[];

  ShortcutButton(views::ButtonListener* listener,
                 const string16& title)
      : BookmarkButtonBase(listener, title) {
  }

  virtual std::string GetClassName() const OVERRIDE {
    return kViewClassName;
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(ShortcutButton);
};

// static for ShortcutButton
const char ShortcutButton::kViewClassName[] =
    "browser/ui/views/bookmarks/ShortcutButton";

// BookmarkFolderButton -------------------------------------------------------

// Buttons used for folders on the bookmark bar, including the 'other folders'
// button.
class BookmarkFolderButton : public views::MenuButton {
 public:
  BookmarkFolderButton(views::ButtonListener* listener,
                       const string16& title,
                       views::MenuButtonListener* menu_button_listener,
                       bool show_menu_marker)
      : MenuButton(listener, title, menu_button_listener, show_menu_marker) {
    show_animation_.reset(new ui::SlideAnimation(this));
    if (!animations_enabled) {
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

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
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

  virtual bool OnMousePressed(const ui::MouseEvent& e) OVERRIDE {
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

  AppLauncherHandler::RecordAppLaunchType(
      extension_misc::APP_LAUNCH_BOOKMARK_BAR);
}

int GetNewtabHorizontalPadding() {
  return chrome::search::IsInstantExtendedAPIEnabled()
         ? kSearchNewTabHorizontalPadding
         : BookmarkBarView::kNewtabHorizontalPadding;
}

int GetNewtabVerticalPadding() {
  return chrome::search::IsInstantExtendedAPIEnabled()
         ? kSearchNewTabVerticalPadding
         : BookmarkBarView::kNewtabVerticalPadding;
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

// Returns the image to use for starred folders.
static const gfx::ImageSkia& GetFolderIcon() {
  if (!kFolderIcon) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    kFolderIcon = rb.GetImageSkiaNamed(IDR_BOOKMARK_BAR_FOLDER);
  }
  return *kFolderIcon;
}

BookmarkBarView::BookmarkBarView(Browser* browser, BrowserView* browser_view)
    : page_navigator_(NULL),
      model_(NULL),
      bookmark_menu_(NULL),
      bookmark_drop_menu_(NULL),
      other_bookmarked_button_(NULL),
      apps_page_shortcut_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(show_folder_method_factory_(this)),
      overflow_button_(NULL),
      instructions_(NULL),
      bookmarks_separator_view_(NULL),
      browser_(browser),
      browser_view_(browser_view),
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

// static
void BookmarkBarView::DisableAnimationsForTesting(bool disabled) {
  animations_enabled = !disabled;
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
    Profile* profile,
    gfx::NativeView context) {
  int max_width = views::TooltipManager::GetMaxWidth(screen_loc.x(),
                                                     screen_loc.y(),
                                                     context);
  gfx::Font tt_font = views::TooltipManager::GetDefaultFont();
  string16 result;

  // First the title.
  if (!title.empty()) {
    string16 localized_title = title;
    base::i18n::AdjustStringForLocaleDirection(&localized_title);
    result.append(ui::ElideText(localized_title, tt_font, max_width,
                                ui::ELIDE_AT_END));
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
    width += 2 * static_cast<int>(GetNewtabHorizontalPadding() * current_state);
  }

  gfx::Size other_bookmarked_pref;
  if (other_bookmarked_button_->visible())
    other_bookmarked_pref = other_bookmarked_button_->GetPreferredSize();
  gfx::Size overflow_pref;
  if (overflow_button_->visible())
    overflow_pref = overflow_button_->GetPreferredSize();
  gfx::Size bookmarks_separator_pref;
  if (bookmarks_separator_view_->visible())
    bookmarks_separator_pref = bookmarks_separator_view_->GetPreferredSize();

  gfx::Size apps_page_shortcut_pref;
  if (apps_page_shortcut_->visible())
    apps_page_shortcut_pref = apps_page_shortcut_->GetPreferredSize();
  width += other_bookmarked_pref.width() + kButtonPadding +
      apps_page_shortcut_pref.width() + kButtonPadding +
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
  return chrome::DropBookmarks(browser_->profile(), data, parent_node, index);
}

void BookmarkBarView::ShowContextMenu(const gfx::Point& p,
                                      bool is_mouse_gesture) {
  ShowContextMenuForView(this, p);
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
  // |browser_view_| can be NULL during tests.
  if (browser_view_)
    browser_view_->ToolbarSizeChanged(true);
}

void BookmarkBarView::AnimationEnded(const ui::Animation* animation) {
  // |browser_view_| can be NULL during tests.
  if (browser_view_) {
    browser_view_->ToolbarSizeChanged(false);
    SchedulePaint();
  }
}

void BookmarkBarView::BookmarkMenuDeleted(BookmarkMenuController* controller) {
  if (controller == bookmark_menu_)
    bookmark_menu_ = NULL;
  else if (controller == bookmark_drop_menu_)
    bookmark_drop_menu_ = NULL;
}

void BookmarkBarView::ShowImportDialog() {
  chrome::ShowImportDialog(browser_);
}

void BookmarkBarView::OnBookmarkBubbleShown(const GURL& url) {
  StopThrobbing(true);
  const BookmarkNode* node = model_->GetMostRecentlyAddedNodeForURL(url);
  if (!node)
    return;  // Generally shouldn't happen.
  StartThrobbing(node, false);
}

void BookmarkBarView::OnBookmarkBubbleHidden() {
  StopThrobbing(false);
}

void BookmarkBarView::Loaded(BookmarkModel* model, bool ids_reassigned) {
  // There should be no buttons. If non-zero it means Load was invoked more than
  // once, or we didn't properly clear things. Either of which shouldn't happen.
  DCHECK_EQ(0, GetBookmarkButtonCount());
  const BookmarkNode* node = model_->bookmark_bar_node();
  DCHECK(node);
  // Create a button for each of the children on the bookmark bar.
  for (int i = 0, child_count = node->child_count(); i < child_count; ++i)
    AddChildViewAt(CreateBookmarkButton(node->GetChild(i)), i);
  DCHECK(model_->other_node());
  other_bookmarked_button_->SetAccessibleName(model_->other_node()->GetTitle());
  other_bookmarked_button_->SetText(model_->other_node()->GetTitle());
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
  // Close the menu if the menu is showing for the deleted node.
  if (bookmark_menu_ && bookmark_menu_->node() == node)
    bookmark_menu_->Cancel();
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
  content::RecordAction(UserMetricsAction("BookmarkBar_DragButton"));

  for (int i = 0; i < GetBookmarkButtonCount(); ++i) {
    if (sender == GetBookmarkButton(i)) {
      views::TextButton* button = GetBookmarkButton(i);
      scoped_ptr<gfx::Canvas> canvas(
          views::GetCanvasForDragImage(button->GetWidget(), button->size()));
      button->PaintButton(canvas.get(), views::TextButton::PB_FOR_DRAG);
      drag_utils::SetDragImageOnDataObject(*canvas, button->size(),
                                           press_pt.OffsetFromOrigin(),
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
  } else if (view == overflow_button_) {
    node = model_->bookmark_bar_node();
    start_index = GetFirstHiddenNodeIndex();
  } else {
    int button_index = GetIndexOf(view);
    DCHECK_NE(-1, button_index);
    node = model_->bookmark_bar_node()->GetChild(button_index);
  }

  bookmark_utils::RecordBookmarkFolderOpen(GetBookmarkLaunchLocation());
  bookmark_menu_ = new BookmarkMenuController(browser_,
      page_navigator_, GetWidget(), node, start_index);
  bookmark_menu_->set_observer(this);
  bookmark_menu_->RunMenuAt(this, false);
}

void BookmarkBarView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  if (sender->tag() == kAppsShortcutButtonTag) {
    chrome::ShowAppLauncherPage(browser_);
    bookmark_utils::RecordAppsPageOpen(GetBookmarkLaunchLocation());
    return;
  }

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
      ui::DispositionFromEventFlags(event.flags());

  if (node->is_url()) {
    RecordAppLaunch(browser_->profile(), node->url());
    OpenURLParams params(
        node->url(), Referrer(), disposition_from_event_flags,
        content::PAGE_TRANSITION_AUTO_BOOKMARK, false);
    page_navigator_->OpenURL(params);
  } else {
    chrome::OpenAll(GetWidget()->GetNativeWindow(), page_navigator_, node,
                    disposition_from_event_flags, browser_->profile());
  }

  bookmark_utils::RecordBookmarkLaunch(GetBookmarkLaunchLocation());
}

void BookmarkBarView::ShowContextMenuForView(views::View* source,
                                             const gfx::Point& point) {
  if (!model_->IsLoaded()) {
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
  Profile* profile = browser_->profile();
  bool close_on_remove =
      (parent == BookmarkModelFactory::GetForProfile(profile)->other_node()) &&
      (parent->child_count() == 1);
  context_menu_.reset(new BookmarkContextMenu(
      GetWidget(), browser_, profile,
      browser_->tab_strip_model()->GetActiveWebContents(),
      parent, nodes, close_on_remove));
  context_menu_->RunMenuAt(point);
}

void BookmarkBarView::Init() {
  // Note that at this point we're not in a hierarchy so GetThemeProvider() will
  // return NULL.  When we're inserted into a hierarchy, we'll call
  // UpdateColors(), which will set the appropriate colors for all the objects
  // added in this function.

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  if (!kDefaultFavicon)
    kDefaultFavicon = rb.GetImageSkiaNamed(IDR_DEFAULT_FAVICON);

  // Child views are traversed in the order they are added. Make sure the order
  // they are added matches the visual order.
  overflow_button_ = CreateOverflowButton();
  AddChildView(overflow_button_);

  other_bookmarked_button_ = CreateOtherBookmarkedButton();
  // We'll re-enable when the model is loaded.
  other_bookmarked_button_->SetEnabled(false);
  AddChildView(other_bookmarked_button_);

  apps_page_shortcut_ = CreateAppsPageShortcutButton();
  AddChildView(apps_page_shortcut_);
  profile_pref_registrar_.Init(browser_->profile()->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kShowAppsShortcutInBookmarkBar,
      base::Bind(&BookmarkBarView::OnAppsPageShortcutVisibilityChanged,
      base::Unretained(this)));
  apps_page_shortcut_->SetVisible(ShouldShowAppsShortcut());

  bookmarks_separator_view_ = new ButtonSeparatorView();
  AddChildView(bookmarks_separator_view_);
  UpdateBookmarksSeparatorVisibility();

  instructions_ = new BookmarkBarInstructionsView(this);
  AddChildView(instructions_);

  set_context_menu_controller(this);

  size_animation_.reset(new ui::SlideAnimation(this));

  model_ = BookmarkModelFactory::GetForProfile(browser_->profile());
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
  // separator, chevrons (for overflow), apps page, and the instruction label.
  return child_count() - 5;
}

views::TextButton* BookmarkBarView::GetBookmarkButton(int index) {
  DCHECK(index >= 0 && index < GetBookmarkButtonCount());
  return static_cast<views::TextButton*>(child_at(index));
}

bookmark_utils::BookmarkLaunchLocation
    BookmarkBarView::GetBookmarkLaunchLocation() const {
  return IsDetached() ? bookmark_utils::LAUNCH_DETACHED_BAR :
                        bookmark_utils::LAUNCH_ATTACHED_BAR;
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
  MenuButton* button = new BookmarkFolderButton(this, string16(), this, false);
  button->set_id(VIEW_ID_OTHER_BOOKMARKS);
  button->SetIcon(GetFolderIcon());
  button->set_context_menu_controller(this);
  button->set_tag(kOtherFolderButtonTag);
  return button;
}

MenuButton* BookmarkBarView::CreateOverflowButton() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  MenuButton* button = new OverFlowButton(this);
  button->SetIcon(*rb.GetImageSkiaNamed(IDR_BOOKMARK_BAR_CHEVRONS));

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

views::TextButton* BookmarkBarView::CreateAppsPageShortcutButton() {
  views::TextButton* button = new ShortcutButton(
      this, l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_APPS_SHORTCUT_NAME));
  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_BAR_APPS_SHORTCUT_TOOLTIP));
  button->set_id(VIEW_ID_BOOKMARK_BAR_ELEMENT);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  button->SetIcon(*rb.GetImageSkiaNamed(IDR_WEBSTORE_ICON_16));
  button->set_context_menu_controller(this);
  button->set_tag(kAppsShortcutButtonTag);
  return button;
}

void BookmarkBarView::ConfigureButton(const BookmarkNode* node,
                                      views::TextButton* button) {
  button->SetText(node->GetTitle());
  button->SetAccessibleName(node->GetTitle());
  button->set_id(VIEW_ID_BOOKMARK_BAR_ELEMENT);
  // We don't always have a theme provider (ui tests, for example).
  if (GetThemeProvider()) {
    button->SetEnabledColor(GetThemeProvider()->GetColor(
        ThemeProperties::COLOR_BOOKMARK_TEXT));
  }

  button->ClearMaxTextSize();
  button->set_context_menu_controller(this);
  button->set_drag_controller(this);
  if (node->is_url()) {
    const gfx::Image& favicon = model_->GetFavicon(node);
    if (!favicon.IsEmpty())
      button->SetIcon(*favicon.ToImageSkia());
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
  bookmark_drop_menu_ = new BookmarkMenuController(browser_,
      page_navigator_, GetWidget(), node, start_index);
  bookmark_drop_menu_->set_observer(this);
  bookmark_drop_menu_->RunMenuAt(this, true);
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
  MessageLoop::current()->PostDelayedTask(
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
  if (other_bookmarked_button_->visible() && other_delta_x >= 0 &&
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
    location->operation = chrome::GetPreferredBookmarkDropOperation(
        event.source_operations(), ops);
    return;
  }

  for (int i = 0; i < GetBookmarkButtonCount() &&
       GetBookmarkButton(i)->visible() && !found; i++) {
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
        data.GetFirstNode(profile) == parent) {
      // Don't open a menu if the node being dragged is the menu to open.
      location->on = false;
    }
  } else {
    location->operation = chrome::GetBookmarkDropOperation(profile, event,
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
      theme_provider->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT);
  for (int i = 0; i < GetBookmarkButtonCount(); ++i)
    GetBookmarkButton(i)->SetEnabledColor(text_color);
  other_bookmarked_button()->SetEnabledColor(text_color);
}

void BookmarkBarView::UpdateOtherBookmarksVisibility() {
  bool has_other_children = !model_->other_node()->empty();
  if (has_other_children == other_bookmarked_button_->visible())
    return;
  other_bookmarked_button_->SetVisible(has_other_children);
  UpdateBookmarksSeparatorVisibility();
  Layout();
  SchedulePaint();
}

void BookmarkBarView::UpdateBookmarksSeparatorVisibility() {
  // Ash does not paint the bookmarks separator line because it looks odd on
  // the flat background.  We keep it present for layout, but don't draw it.
  bookmarks_separator_view_->SetVisible(
      browser_->host_desktop_type() != chrome::HOST_DESKTOP_TYPE_ASH &&
      (other_bookmarked_button_->visible() ||
          apps_page_shortcut_->visible()));
}

gfx::Size BookmarkBarView::LayoutItems(bool compute_bounds_only) {
  gfx::Size prefsize;
  if (!parent() && !compute_bounds_only)
    return prefsize;

  int x = kLeftMargin;
  int top_margin = IsDetached() ? kDetachedTopMargin : 0;
  int y = top_margin;
  int width = View::width() - kRightMargin - kLeftMargin;
  int height = browser_defaults::kBookmarkBarHeight - kBottomMargin;
  int separator_margin = kSeparatorMargin;

  if (IsDetached()) {
    double current_state = 1 - size_animation_->GetCurrentValue();
    x += static_cast<int>(GetNewtabHorizontalPadding() * current_state);
    y += (View::height() - browser_defaults::kBookmarkBarHeight) / 2;
    width -= static_cast<int>(GetNewtabHorizontalPadding() * current_state);
    separator_margin -= static_cast<int>(kSeparatorMargin * current_state);
  } else {
    // For the attached appearance, pin the content to the bottom of the bar
    // when animating in/out, as shrinking its height instead looks weird.  This
    // also matches how we layout infobars.
    y += View::height() - browser_defaults::kBookmarkBarHeight;
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
  if (apps_page_shortcut_->visible())
    max_x -= apps_page_shortcut_pref.width() + kButtonPadding;

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
  const bool all_visible = (GetBookmarkButtonCount() == 0 ||
                            child_at(GetBookmarkButtonCount() - 1)->visible());

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
  if (bookmarks_separator_view_->visible()) {
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
  if (other_bookmarked_button_->visible()) {
    if (!compute_bounds_only) {
      other_bookmarked_button_->SetBounds(x, y, other_bookmarked_pref.width(),
                                          height);
    }
    x += other_bookmarked_pref.width() + kButtonPadding;
  }

  // The app page shortcut button.
  if (apps_page_shortcut_->visible()) {
    if (!compute_bounds_only) {
      apps_page_shortcut_->SetBounds(x, y, apps_page_shortcut_pref.width(),
                                     height);
    }
    x += apps_page_shortcut_pref.width() + kButtonPadding;
  }

  // Set the preferred size computed so far.
  if (compute_bounds_only) {
    x += kRightMargin;
    prefsize.set_width(x);
    if (IsDetached()) {
      x += static_cast<int>(GetNewtabHorizontalPadding() *
          (1 - size_animation_->GetCurrentValue()));
      int ntp_bookmark_bar_height =
          chrome::search::IsInstantExtendedAPIEnabled()
          ? kSearchNewTabBookmarkBarHeight : chrome::kNTPBookmarkBarHeight;
      prefsize.set_height(
          browser_defaults::kBookmarkBarHeight +
          static_cast<int>(
              (ntp_bookmark_bar_height -
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

bool BookmarkBarView::ShouldShowAppsShortcut() const {
  return chrome::search::IsInstantExtendedAPIEnabled() &&
      browser_->profile()->GetPrefs()->GetBoolean(
          prefs::kShowAppsShortcutInBookmarkBar);
}

void BookmarkBarView::OnAppsPageShortcutVisibilityChanged() {
  DCHECK(apps_page_shortcut_);
  apps_page_shortcut_->SetVisible(ShouldShowAppsShortcut());
  UpdateBookmarksSeparatorVisibility();
  Layout();
}
