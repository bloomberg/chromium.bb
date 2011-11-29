// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_view.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/download/download_item_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "views/background.h"

// Max number of download views we'll contain. Any time a view is added and
// we already have this many download views, one is removed.
static const size_t kMaxDownloadViews = 15;

// Padding from left edge and first download view.
static const int kLeftPadding = 2;

// Padding from right edge and close button/show downloads link.
static const int kRightPadding = 10;

// Padding between the show all link and close button.
static const int kCloseAndLinkPadding = 14;

// Padding between the download views.
static const int kDownloadPadding = 10;

// Padding between the top/bottom and the content.
static const int kTopBottomPadding = 2;

// Padding between the icon and 'show all downloads' link
static const int kDownloadsTitlePadding = 4;

// Border color.
static const SkColor kBorderColor = SkColorSetRGB(214, 214, 214);

// New download item animation speed in milliseconds.
static const int kNewItemAnimationDurationMs = 800;

// Shelf show/hide speed.
static const int kShelfAnimationDurationMs = 120;

// Amount of time to delay if the mouse leaves the shelf by way of entering
// another window. This is much larger than the normal delay as openning a
// download is most likely going to trigger a new window to appear over the
// button. Delay the time so that the user has a chance to quickly close the
// other app and return to chrome with the download shelf still open.
static const int kNotifyOnExitTimeMS = 5000;

namespace {

// Sets size->width() to view's preferred width + size->width().s
// Sets size->height() to the max of the view's preferred height and
// size->height();
void AdjustSize(views::View* view, gfx::Size* size) {
  gfx::Size view_preferred = view->GetPreferredSize();
  size->Enlarge(view_preferred.width(), 0);
  size->set_height(std::max(view_preferred.height(), size->height()));
}

int CenterPosition(int size, int target_size) {
  return std::max((target_size - size) / 2, kTopBottomPadding);
}

}  // namespace

DownloadShelfView::DownloadShelfView(Browser* browser, BrowserView* parent)
    : browser_(browser),
      parent_(parent),
      auto_closed_(true),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          mouse_watcher_(this, this, gfx::Insets())) {
  mouse_watcher_.set_notify_on_exit_time_ms(kNotifyOnExitTimeMS);
  set_id(VIEW_ID_DOWNLOAD_SHELF);
  parent->AddChildView(this);
  Show();
}

DownloadShelfView::~DownloadShelfView() {
  parent_->RemoveChildView(this);
}

void DownloadShelfView::AddDownloadView(DownloadItemView* view) {
  mouse_watcher_.Stop();

  Show();

  DCHECK(view);
  download_views_.push_back(view);
  AddChildView(view);
  if (download_views_.size() > kMaxDownloadViews)
    RemoveDownloadView(*download_views_.begin());

  new_item_animation_->Reset();
  new_item_animation_->Show();
}

void DownloadShelfView::AddDownload(BaseDownloadItemModel* download_model) {
  DownloadItemView* view = new DownloadItemView(
      download_model->download(), this, download_model);
  AddDownloadView(view);
}

void DownloadShelfView::MouseMovedOutOfView() {
  Close();
}

void DownloadShelfView::OnWillChangeFocus(views::View* focused_before,
                                          views::View* focused_now) {
  SchedulePaintForDownloadItem(focused_before);
  SchedulePaintForDownloadItem(focused_now);
}

void DownloadShelfView::OnDidChangeFocus(views::View* focused_before,
                                         views::View* focused_now) {
  AccessiblePaneView::OnDidChangeFocus(focused_before, focused_now);
}

void DownloadShelfView::RemoveDownloadView(View* view) {
  DCHECK(view);
  std::vector<DownloadItemView*>::iterator i =
      find(download_views_.begin(), download_views_.end(), view);
  DCHECK(i != download_views_.end());
  download_views_.erase(i);
  RemoveChildView(view);
  delete view;
  if (download_views_.empty())
    Close();
  else if (CanAutoClose())
    mouse_watcher_.Start();
  Layout();
  SchedulePaint();
}

views::View* DownloadShelfView::GetDefaultFocusableChild() {
  return download_views_.empty() ?
      static_cast<View*>(show_all_view_) : download_views_[0];
}

void DownloadShelfView::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  OnPaintBorder(canvas);

  // Draw the focus rect here, since it's outside the bounds of the item.
  for (size_t i = 0; i < download_views_.size(); ++i) {
    if (download_views_[i]->HasFocus()) {
      gfx::Rect r = GetFocusRectBounds(download_views_[i]);
      r.Inset(0, 0, 0, 1);
      canvas->DrawFocusRect(r);
      break;
    }
  }
}

void DownloadShelfView::OnPaintBorder(gfx::Canvas* canvas) {
  canvas->FillRect(kBorderColor, gfx::Rect(0, 0, width(), 1));
}

void DownloadShelfView::OpenedDownload(DownloadItemView* view) {
  if (CanAutoClose())
    mouse_watcher_.Start();
}

gfx::Size DownloadShelfView::GetPreferredSize() {
  gfx::Size prefsize(kRightPadding + kLeftPadding + kCloseAndLinkPadding, 0);
  AdjustSize(close_button_, &prefsize);
  AdjustSize(show_all_view_, &prefsize);
  // Add one download view to the preferred size.
  if (!download_views_.empty()) {
    AdjustSize(*download_views_.begin(), &prefsize);
    prefsize.Enlarge(kDownloadPadding, 0);
  }
  prefsize.Enlarge(0, kTopBottomPadding + kTopBottomPadding);
  if (shelf_animation_->is_animating()) {
    prefsize.set_height(static_cast<int>(
        static_cast<double>(prefsize.height()) *
                            shelf_animation_->GetCurrentValue()));
  }
  return prefsize;
}

void DownloadShelfView::AnimationProgressed(const ui::Animation *animation) {
  if (animation == new_item_animation_.get()) {
    Layout();
    SchedulePaint();
  } else if (animation == shelf_animation_.get()) {
    // Force a re-layout of the parent, which will call back into
    // GetPreferredSize, where we will do our animation. In the case where the
    // animation is hiding, we do a full resize - the fast resizing would
    // otherwise leave blank white areas where the shelf was and where the
    // user's eye is. Thankfully bottom-resizing is a lot faster than
    // top-resizing.
    parent_->ToolbarSizeChanged(shelf_animation_->IsShowing());
  }
}

void DownloadShelfView::AnimationEnded(const ui::Animation *animation) {
  if (animation == shelf_animation_.get()) {
    parent_->SetDownloadShelfVisible(shelf_animation_->IsShowing());
    if (!shelf_animation_->IsShowing())
      Closed();
  }
}

void DownloadShelfView::Layout() {
  // Let our base class layout our child views
  views::View::Layout();

  // If there is not enough room to show the first download item, show the
  // "Show all downloads" link to the left to make it more visible that there is
  // something to see.
  bool show_link_only = !CanFitFirstDownloadItem();

  gfx::Size image_size = arrow_image_->GetPreferredSize();
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  gfx::Size show_all_size = show_all_view_->GetPreferredSize();
  int max_download_x =
      std::max<int>(0, width() - kRightPadding - close_button_size.width() -
                       kCloseAndLinkPadding - show_all_size.width() -
                       kDownloadsTitlePadding - image_size.width() -
                       kDownloadPadding);
  int next_x = show_link_only ? kLeftPadding :
                                max_download_x + kDownloadPadding;
  // Align vertically with show_all_view_.
  arrow_image_->SetBounds(next_x,
                          CenterPosition(show_all_size.height(), height()),
                          image_size.width(), image_size.height());
  next_x += image_size.width() + kDownloadsTitlePadding;
  show_all_view_->SetBounds(next_x,
                            CenterPosition(show_all_size.height(), height()),
                            show_all_size.width(),
                            show_all_size.height());
  next_x += show_all_size.width() + kCloseAndLinkPadding;
  // If the window is maximized, we want to expand the hitbox of the close
  // button to the right and bottom to make it easier to click.
  bool is_maximized = browser_->window()->IsMaximized();
  int y = CenterPosition(close_button_size.height(), height());
  close_button_->SetBounds(next_x, y,
      is_maximized ? width() - next_x : close_button_size.width(),
      is_maximized ? height() - y : close_button_size.height());
  if (show_link_only) {
    // Let's hide all the items.
    std::vector<DownloadItemView*>::reverse_iterator ri;
    for (ri = download_views_.rbegin(); ri != download_views_.rend(); ++ri)
      (*ri)->SetVisible(false);
    return;
  }

  next_x = kLeftPadding;
  std::vector<DownloadItemView*>::reverse_iterator ri;
  for (ri = download_views_.rbegin(); ri != download_views_.rend(); ++ri) {
    gfx::Size view_size = (*ri)->GetPreferredSize();

    int x = next_x;

    // Figure out width of item.
    int item_width = view_size.width();
    if (new_item_animation_->is_animating() && ri == download_views_.rbegin()) {
       item_width = static_cast<int>(static_cast<double>(view_size.width()) *
                     new_item_animation_->GetCurrentValue());
    }

    next_x += item_width;

    // Make sure our item can be contained within the shelf.
    if (next_x < max_download_x) {
      (*ri)->SetVisible(true);
      (*ri)->SetBounds(x, CenterPosition(view_size.height(), height()),
                       item_width, view_size.height());
    } else {
      (*ri)->SetVisible(false);
    }
  }
}

void DownloadShelfView::ViewHierarchyChanged(bool is_add,
                                             View* parent,
                                             View* child) {
  View::ViewHierarchyChanged(is_add, parent, child);

  if (is_add && (child == this)) {
    set_background(views::Background::CreateSolidBackground(
        GetThemeProvider()->GetColor(ThemeService::COLOR_TOOLBAR)));

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    arrow_image_ = new views::ImageView();
    arrow_image_->SetImage(rb.GetBitmapNamed(IDR_DOWNLOADS_FAVICON));
    AddChildView(arrow_image_);

    show_all_view_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_SHOW_ALL_DOWNLOADS));
    show_all_view_->set_listener(this);
    show_all_view_->SetBackgroundColor(background()->get_color());
    show_all_view_->SetEnabledColor(
        GetThemeProvider()->GetColor(ThemeService::COLOR_BOOKMARK_TEXT));
    AddChildView(show_all_view_);

    close_button_ = new views::ImageButton(this);
    close_button_->SetImage(views::CustomButton::BS_NORMAL,
                            rb.GetBitmapNamed(IDR_CLOSE_BAR));
    close_button_->SetImage(views::CustomButton::BS_HOT,
                            rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
    close_button_->SetImage(views::CustomButton::BS_PUSHED,
                            rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
    close_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
    UpdateButtonColors();
    AddChildView(close_button_);

    new_item_animation_.reset(new ui::SlideAnimation(this));
    new_item_animation_->SetSlideDuration(kNewItemAnimationDurationMs);

    shelf_animation_.reset(new ui::SlideAnimation(this));
    shelf_animation_->SetSlideDuration(kShelfAnimationDurationMs);
  }
}

bool DownloadShelfView::CanFitFirstDownloadItem() {
  if (download_views_.empty())
    return true;

  gfx::Size image_size = arrow_image_->GetPreferredSize();
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  gfx::Size show_all_size = show_all_view_->GetPreferredSize();

  // Let's compute the width available for download items, which is the width
  // of the shelf minus the "Show all downloads" link, arrow and close button
  // and the padding.
  int available_width = width() - kRightPadding - close_button_size.width() -
      kCloseAndLinkPadding - show_all_size.width() - kDownloadsTitlePadding -
      image_size.width() - kDownloadPadding - kLeftPadding;
  if (available_width <= 0)
    return false;

  gfx::Size item_size = (*download_views_.rbegin())->GetPreferredSize();
  return item_size.width() < available_width;
}

void DownloadShelfView::UpdateButtonColors() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (GetThemeProvider()) {
    close_button_->SetBackground(
        GetThemeProvider()->GetColor(ThemeService::COLOR_TAB_TEXT),
        rb.GetBitmapNamed(IDR_CLOSE_BAR),
        rb.GetBitmapNamed(IDR_CLOSE_BAR_MASK));
  }
}

void DownloadShelfView::OnThemeChanged() {
  UpdateButtonColors();
}

void DownloadShelfView::LinkClicked(views::Link* source, int event_flags) {
  browser_->ShowDownloadsTab();
}

void DownloadShelfView::ButtonPressed(
    views::Button* button, const views::Event& event) {
  auto_closed_ = false;
  Close();
}

bool DownloadShelfView::IsShowing() const {
  return shelf_animation_->IsShowing();
}

bool DownloadShelfView::IsClosing() const {
  return shelf_animation_->IsClosing();
}

void DownloadShelfView::Show() {
  shelf_animation_->Show();
}

void DownloadShelfView::Close() {
  int num_in_progress = 0;
  for (size_t i = 0; i < download_views_.size(); ++i) {
    if (download_views_[i]->download()->IsInProgress())
      ++num_in_progress;
  }
  download_stats::RecordShelfClose(
      download_views_.size(), num_in_progress, auto_closed_);
  parent_->SetDownloadShelfVisible(false);
  shelf_animation_->Hide();
  auto_closed_ = true;
}

Browser* DownloadShelfView::browser() const {
  return browser_;
}

void DownloadShelfView::Closed() {
  // When the close animation is complete, remove all completed downloads.
  size_t i = 0;
  while (i < download_views_.size()) {
    DownloadItem* download = download_views_[i]->download();
    bool is_transfer_done = download->IsComplete() ||
                            download->IsCancelled() ||
                            download->IsInterrupted();
    if (is_transfer_done &&
        download->GetSafetyState() != DownloadItem::DANGEROUS) {
      RemoveDownloadView(download_views_[i]);
    } else {
      // Treat the item as opened when we close. This way if we get shown again
      // the user need not open this item for the shelf to auto-close.
      download->SetOpened(true);
      ++i;
    }
  }
}

bool DownloadShelfView::CanAutoClose() {
  for (size_t i = 0; i < download_views_.size(); ++i) {
    if (!download_views_[i]->download()->GetOpened())
      return false;
  }
  return true;
}

void DownloadShelfView::SchedulePaintForDownloadItem(views::View* view) {
  // Make sure it's not NULL.  (Focus sometimes changes to or from NULL.)
  if (!view)
    return;

  // Make sure it's one of our DownloadItemViews.
  bool found = false;
  for (size_t i = 0; i < download_views_.size(); ++i) {
    if (download_views_[i] == view)
      found = true;
  }
  if (!found)
    return;

  // Invalidate it
  gfx::Rect invalid_rect =
      GetFocusRectBounds(static_cast<DownloadItemView*>(view));
  SchedulePaintInRect(invalid_rect);
}

gfx::Rect DownloadShelfView::GetFocusRectBounds(
    const DownloadItemView* download_item_view) {
  gfx::Rect bounds = download_item_view->bounds();

#if defined(TOOLKIT_VIEWS)
  bounds.set_height(bounds.height() - 1);
  bounds.Offset(0, 3);
#endif

  return bounds;
}
