// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_view.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bar_control_button.h"
#include "chrome/browser/ui/views/download/download_item_view.h"
#include "chrome/browser/ui/views/download/download_item_view_md.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/page_navigator.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/mouse_watcher_view_host.h"

using content::DownloadItem;

namespace {

// Max number of download views we'll contain. Any time a view is added and
// we already have this many download views, one is removed.
const size_t kMaxDownloadViews = 15;

// Padding from left edge and first download view.
const int kLeftPadding = 2;
const int kStartPaddingMd = 4;

// Padding from right edge and close button/show downloads link.
const int kRightPadding = 10;
const int kEndPaddingMd = 6;

// Padding between the show all link and close button.
const int kCloseAndLinkPadding = 14;
const int kCloseAndLinkPaddingMd = 6;

// Padding between the download views.
const int kDownloadPadding = 10;

// Padding between the top/bottom and the content.
const int kTopBottomPadding = 2;

// Padding between the icon and 'show all downloads' link
const int kDownloadsTitlePadding = 4;

// Border color.
const SkColor kBorderColor = SkColorSetRGB(214, 214, 214);

// New download item animation speed in milliseconds.
const int kNewItemAnimationDurationMs = 800;

// Shelf show/hide speed.
const int kShelfAnimationDurationMs = 120;

// Amount of time to delay if the mouse leaves the shelf by way of entering
// another window. This is much larger than the normal delay as openning a
// download is most likely going to trigger a new window to appear over the
// button. Delay the time so that the user has a chance to quickly close the
// other app and return to chrome with the download shelf still open.
const int kNotifyOnExitTimeMS = 5000;

int GetStartPadding() {
  return ui::MaterialDesignController::IsModeMaterial() ? kStartPaddingMd
                                                        : kLeftPadding;
}

int GetEndPadding() {
  return ui::MaterialDesignController::IsModeMaterial() ? kEndPaddingMd
                                                        : kRightPadding;
}

int GetCloseAndLinkPadding() {
  return ui::MaterialDesignController::IsModeMaterial() ? kCloseAndLinkPaddingMd
                                                        : kCloseAndLinkPadding;
}

int GetBetweenItemPadding() {
  return ui::MaterialDesignController::IsModeMaterial() ? 0 : kDownloadPadding;
}

int GetTopBottomPadding() {
  return ui::MaterialDesignController::IsModeMaterial() ? 0 : kTopBottomPadding;
}

// Sets size->width() to view's preferred width + size->width().s
// Sets size->height() to the max of the view's preferred height and
// size->height();
void AdjustSize(views::View* view, gfx::Size* size) {
  gfx::Size view_preferred = view->GetPreferredSize();
  size->Enlarge(view_preferred.width(), 0);
  size->set_height(std::max(view_preferred.height(), size->height()));
}

int CenterPosition(int size, int target_size) {
  return std::max((target_size - size) / 2, GetTopBottomPadding());
}

}  // namespace

DownloadShelfView::DownloadShelfView(Browser* browser, BrowserView* parent)
    : browser_(browser),
      new_item_animation_(this),
      shelf_animation_(this),
      arrow_image_(nullptr),
      show_all_view_(nullptr),
      close_button_(nullptr),
      parent_(parent),
      mouse_watcher_(new views::MouseWatcherViewHost(this, gfx::Insets()),
                     this) {
  mouse_watcher_.set_notify_on_exit_time(
      base::TimeDelta::FromMilliseconds(kNotifyOnExitTimeMS));
  set_id(VIEW_ID_DOWNLOAD_SHELF);
  parent->AddChildView(this);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  arrow_image_ = new views::ImageView();
  AddChildView(arrow_image_);
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    arrow_image_->SetImage(rb.GetImageSkiaNamed(IDR_DOWNLOADS_FAVICON));

    views::Link* show_all_view =
        new views::Link(l10n_util::GetStringUTF16(IDS_SHOW_ALL_DOWNLOADS));
    show_all_view->set_listener(this);
    show_all_view_ = show_all_view;

    close_button_ = new views::ImageButton(this);
    close_button_->SetImage(views::CustomButton::STATE_NORMAL,
                            rb.GetImageSkiaNamed(IDR_CLOSE_1));
    close_button_->SetImage(views::CustomButton::STATE_HOVERED,
                            rb.GetImageSkiaNamed(IDR_CLOSE_1_H));
    close_button_->SetImage(views::CustomButton::STATE_PRESSED,
                            rb.GetImageSkiaNamed(IDR_CLOSE_1_P));
  } else {
    views::LabelButton* show_all_view = new views::LabelButton(
        this, l10n_util::GetStringUTF16(IDS_SHOW_ALL_DOWNLOADS_MD));
    show_all_view->SetFocusable(true);
    show_all_view->SetStyle(views::Button::STYLE_BUTTON);
    show_all_view_ = show_all_view;

    BarControlButton* close_button = new BarControlButton(this);
    close_button->SetIcon(gfx::VectorIconId::BAR_CLOSE,
                          base::Bind(&DownloadShelfView::GetTextColorForIconMd,
                                     base::Unretained(this)));
    close_button_ = close_button;
  }
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  AddChildView(show_all_view_);
  AddChildView(close_button_);

  new_item_animation_.SetSlideDuration(kNewItemAnimationDurationMs);
  shelf_animation_.SetSlideDuration(kShelfAnimationDurationMs);
}

DownloadShelfView::~DownloadShelfView() {
  parent_->RemoveChildView(this);
}

void DownloadShelfView::AddDownloadView(views::View* view) {
  mouse_watcher_.Stop();

  DCHECK(view);
  download_views_.push_back(view);

  // Insert the new view as the first child, so the logical child order matches
  // the visual order.  This ensures that tabbing through downloads happens in
  // the order users would expect.
  AddChildViewAt(view, 0);
  if (download_views_.size() > kMaxDownloadViews)
    RemoveDownloadView(*download_views_.begin());

  new_item_animation_.Reset();
  new_item_animation_.Show();
}

void DownloadShelfView::DoAddDownload(DownloadItem* download) {
  if (ui::MaterialDesignController::IsModeMaterial())
    AddDownloadView(new DownloadItemViewMd(download, this));
  else
    AddDownloadView(new DownloadItemView(download, this));
}

void DownloadShelfView::MouseMovedOutOfHost() {
  Close(AUTOMATIC);
}

void DownloadShelfView::RemoveDownloadView(View* view) {
  DCHECK(view);
  auto i = find(download_views_.begin(), download_views_.end(), view);
  DCHECK(i != download_views_.end());
  download_views_.erase(i);
  RemoveChildView(view);
  delete view;
  if (download_views_.empty())
    Close(AUTOMATIC);
  else if (CanAutoClose())
    mouse_watcher_.Start();
  Layout();
  SchedulePaint();
}

views::View* DownloadShelfView::GetDefaultFocusableChild() {
  return download_views_.empty() ? show_all_view_ : download_views_.back();
}

void DownloadShelfView::OnPaintBorder(gfx::Canvas* canvas) {
  canvas->FillRect(gfx::Rect(0, 0, width(), 1), kBorderColor);
}

void DownloadShelfView::OpenedDownload() {
  if (CanAutoClose())
    mouse_watcher_.Start();
}

content::PageNavigator* DownloadShelfView::GetNavigator() {
  return browser_;
}

gfx::Size DownloadShelfView::GetPreferredSize() const {
  gfx::Size prefsize(
      GetEndPadding() + GetStartPadding() + GetCloseAndLinkPadding(), 0);
  AdjustSize(close_button_, &prefsize);
  AdjustSize(show_all_view_, &prefsize);
  // Add one download view to the preferred size.
  if (!download_views_.empty()) {
    AdjustSize(*download_views_.begin(), &prefsize);
    prefsize.Enlarge(GetBetweenItemPadding(), 0);
  }
  prefsize.Enlarge(0, 2 * GetTopBottomPadding());
  if (shelf_animation_.is_animating()) {
    prefsize.set_height(
        static_cast<int>(static_cast<double>(prefsize.height()) *
                         shelf_animation_.GetCurrentValue()));
  }
  return prefsize;
}

void DownloadShelfView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &new_item_animation_) {
    Layout();
    SchedulePaint();
  } else if (animation == &shelf_animation_) {
    // Force a re-layout of the parent, which will call back into
    // GetPreferredSize, where we will do our animation. In the case where the
    // animation is hiding, we do a full resize - the fast resizing would
    // otherwise leave blank white areas where the shelf was and where the
    // user's eye is. Thankfully bottom-resizing is a lot faster than
    // top-resizing.
    parent_->ToolbarSizeChanged(shelf_animation_.IsShowing());
  }
}

void DownloadShelfView::AnimationEnded(const gfx::Animation *animation) {
  if (animation == &shelf_animation_) {
    parent_->SetDownloadShelfVisible(shelf_animation_.IsShowing());
    if (!shelf_animation_.IsShowing())
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
      std::max<int>(0, width() - GetEndPadding() - close_button_size.width() -
                           GetCloseAndLinkPadding() - show_all_size.width() -
                           kDownloadsTitlePadding - image_size.width() -
                           GetBetweenItemPadding());
  int next_x = show_link_only ? GetStartPadding()
                              : max_download_x + GetBetweenItemPadding();
  // Align vertically with show_all_view_.
  arrow_image_->SetBounds(next_x,
                          CenterPosition(image_size.height(), height()),
                          image_size.width(), image_size.height());
  next_x += image_size.width() + kDownloadsTitlePadding;
  show_all_view_->SetBounds(next_x,
                            CenterPosition(show_all_size.height(), height()),
                            show_all_size.width(),
                            show_all_size.height());
  next_x += show_all_size.width() + GetCloseAndLinkPadding();
  // If the window is maximized, we want to expand the hitbox of the close
  // button to the right and bottom to make it easier to click.
  bool is_maximized = browser_->window()->IsMaximized();
  int y = CenterPosition(close_button_size.height(), height());
  close_button_->SetBounds(next_x, y,
      is_maximized ? width() - next_x : close_button_size.width(),
      is_maximized ? height() - y : close_button_size.height());
  if (show_link_only) {
    // Let's hide all the items.
    for (auto ri = download_views_.rbegin(); ri != download_views_.rend(); ++ri)
      (*ri)->SetVisible(false);
    return;
  }

  next_x = GetStartPadding();
  for (auto ri = download_views_.rbegin(); ri != download_views_.rend(); ++ri) {
    gfx::Size view_size = (*ri)->GetPreferredSize();

    int x = next_x;

    // Figure out width of item.
    int item_width = view_size.width();
    if (new_item_animation_.is_animating() && ri == download_views_.rbegin()) {
      item_width = static_cast<int>(static_cast<double>(view_size.width()) *
                                    new_item_animation_.GetCurrentValue());
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

void DownloadShelfView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  View::ViewHierarchyChanged(details);
  if (details.is_add)
    UpdateColorsFromTheme();
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
  int available_width = width() - GetEndPadding() - close_button_size.width() -
                        GetCloseAndLinkPadding() - show_all_size.width() -
                        kDownloadsTitlePadding - image_size.width() -
                        GetBetweenItemPadding() - GetStartPadding();
  if (available_width <= 0)
    return false;

  gfx::Size item_size = (*download_views_.rbegin())->GetPreferredSize();
  return item_size.width() < available_width;
}

void DownloadShelfView::UpdateColorsFromTheme() {
  if (show_all_view_ && close_button_ && GetThemeProvider()) {
    set_background(views::Background::CreateSolidBackground(
        GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR)));

    if (!ui::MaterialDesignController::IsModeMaterial()) {
      views::Link* show_all_view = static_cast<views::Link*>(show_all_view_);
      show_all_view->SetBackgroundColor(background()->get_color());
      show_all_view->SetEnabledColor(
          GetThemeProvider()->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT));

      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      close_button_->SetBackground(
          GetThemeProvider()->GetColor(ThemeProperties::COLOR_TAB_TEXT),
          rb.GetImageSkiaNamed(IDR_CLOSE_1),
          rb.GetImageSkiaNamed(IDR_CLOSE_1_MASK));
    }
  }
}

void DownloadShelfView::OnThemeChanged() {
  UpdateColorsFromTheme();
}

void DownloadShelfView::LinkClicked(views::Link* source, int event_flags) {
  chrome::ShowDownloads(browser_);
}

void DownloadShelfView::ButtonPressed(
    views::Button* button, const ui::Event& event) {
  if (button == close_button_)
    Close(USER_ACTION);
  else if (button == show_all_view_)
    chrome::ShowDownloads(browser_);
  else
    NOTREACHED();
}

bool DownloadShelfView::IsShowing() const {
  return visible() && shelf_animation_.IsShowing();
}

bool DownloadShelfView::IsClosing() const {
  return shelf_animation_.IsClosing();
}

void DownloadShelfView::DoShow() {
  SetVisible(true);
  shelf_animation_.Show();
}

void DownloadShelfView::DoClose(CloseReason reason) {
  int num_in_progress = 0;
  for (size_t i = 0; i < download_views_.size(); ++i) {
    if (GetDownloadItemForView(i)->GetState() == DownloadItem::IN_PROGRESS)
      ++num_in_progress;
  }
  RecordDownloadShelfClose(
      download_views_.size(), num_in_progress, reason == AUTOMATIC);
  parent_->SetDownloadShelfVisible(false);
  shelf_animation_.Hide();
}

Browser* DownloadShelfView::browser() const {
  return browser_;
}

void DownloadShelfView::Closed() {
  // Don't remove completed downloads if the shelf is just being auto-hidden
  // rather than explicitly closed by the user.
  if (is_hidden())
    return;
  // When the close animation is complete, remove all completed downloads.
  size_t i = 0;
  while (i < download_views_.size()) {
    DownloadItem* download = GetDownloadItemForView(i);
    DownloadItem::DownloadState state = download->GetState();
    bool is_transfer_done = state == DownloadItem::COMPLETE ||
                            state == DownloadItem::CANCELLED ||
                            state == DownloadItem::INTERRUPTED;
    if (is_transfer_done && !download->IsDangerous()) {
      RemoveDownloadView(download_views_[i]);
    } else {
      // Treat the item as opened when we close. This way if we get shown again
      // the user need not open this item for the shelf to auto-close.
      download->SetOpened(true);
      ++i;
    }
  }
  SetVisible(false);
}

bool DownloadShelfView::CanAutoClose() {
  for (size_t i = 0; i < download_views_.size(); ++i) {
    if (!GetDownloadItemForView(i)->GetOpened())
      return false;
  }
  return true;
}

content::DownloadItem* DownloadShelfView::GetDownloadItemForView(size_t i) {
  if (ui::MaterialDesignController::IsModeMaterial())
    return static_cast<DownloadItemViewMd*>(download_views_[i])->download();
  return static_cast<DownloadItemView*>(download_views_[i])->download();
}

SkColor DownloadShelfView::GetTextColorForIconMd() {
  return DownloadItemViewMd::GetTextColorForThemeProvider(GetThemeProvider());
}
