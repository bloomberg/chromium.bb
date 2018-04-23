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
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/download/download_item_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_item.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/mouse_watcher_view_host.h"

using download::DownloadItem;

namespace {

// Max number of download views we'll contain. Any time a view is added and
// we already have this many download views, one is removed.
const size_t kMaxDownloadViews = 15;

// Padding from left edge and first download view.
const int kStartPadding = 4;

// Padding from right edge and close button/show downloads link.
const int kEndPadding = 6;

// Padding between the show all link and close button.
const int kCloseAndLinkPadding = 6;

// Padding above the content.
const int kTopPadding = 1;

// Border color.
const SkColor kBorderColor = SkColorSetRGB(214, 214, 214);

// New download item animation speed in milliseconds.
const int kNewItemAnimationDurationMs = 800;

// Shelf show/hide speed.
const int kShelfAnimationDurationMs = 120;

// Amount of time to delay if the mouse leaves the shelf by way of entering
// another window. This is much larger than the normal delay as opening a
// download is most likely going to trigger a new window to appear over the
// button. Delay the time so that the user has a chance to quickly close the
// other app and return to chrome with the download shelf still open.
const int kNotifyOnExitTimeMS = 5000;

// Sets size->width() to view's preferred width + size->width().
// Sets size->height() to the max of the view's preferred height and
// size->height();
void AdjustSize(views::View* view, gfx::Size* size) {
  gfx::Size view_preferred = view->GetPreferredSize();
  size->Enlarge(view_preferred.width(), 0);
  size->set_height(std::max(view_preferred.height(), size->height()));
}

int CenterPosition(int size, int target_size) {
  return std::max((target_size - size) / 2, kTopPadding);
}

}  // namespace

DownloadShelfView::DownloadShelfView(Browser* browser, BrowserView* parent)
    : browser_(browser),
      new_item_animation_(this),
      shelf_animation_(this),
      show_all_view_(nullptr),
      close_button_(views::CreateVectorImageButton(this)),
      parent_(parent),
      mouse_watcher_(
          std::make_unique<views::MouseWatcherViewHost>(this, gfx::Insets()),
          this) {
  // Start out hidden: the shelf might be created but never shown in some
  // cases, like when installing a theme. See DownloadShelf::AddDownload().
  SetVisible(false);

  mouse_watcher_.set_notify_on_exit_time(
      base::TimeDelta::FromMilliseconds(kNotifyOnExitTimeMS));
  set_id(VIEW_ID_DOWNLOAD_SHELF);
  parent->AddChildView(this);

  show_all_view_ = views::MdTextButton::Create(
      this, l10n_util::GetStringUTF16(IDS_SHOW_ALL_DOWNLOADS));
  AddChildView(show_all_view_);

  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  accessible_alert_ = new views::View();
  AddChildView(accessible_alert_);

  new_item_animation_.SetSlideDuration(kNewItemAnimationDurationMs);
  shelf_animation_.SetSlideDuration(kShelfAnimationDurationMs);

  GetViewAccessibility().OverrideName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_DOWNLOADS_BAR));
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kGroup);
}

DownloadShelfView::~DownloadShelfView() {
  parent_->RemoveChildView(this);
}

void DownloadShelfView::AddDownloadView(DownloadItemView* view) {
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
  AddDownloadView(new DownloadItemView(download, this, accessible_alert_));
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

void DownloadShelfView::ConfigureButtonForTheme(views::MdTextButton* button) {
  DCHECK(GetThemeProvider());

  button->SetEnabledTextColors(
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT));
  // For the normal theme, just use the default button bg color.
  base::Optional<SkColor> bg_color;
  if (!ThemeServiceFactory::GetForProfile(browser_->profile())
          ->UsingDefaultTheme()) {
    // For custom themes, we have to make up a background color for the
    // button. Use a slight tint of the shelf background.
    bg_color = color_utils::BlendTowardOppositeLuma(
        GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR), 0x10);
  }
  button->SetBgColorOverride(bg_color);
}

views::View* DownloadShelfView::GetDefaultFocusableChild() {
  if (!download_views_.empty())
    return download_views_.back();

  return show_all_view_;
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

gfx::Size DownloadShelfView::CalculatePreferredSize() const {
  gfx::Size prefsize(kEndPadding + kStartPadding + kCloseAndLinkPadding, 0);
  AdjustSize(close_button_, &prefsize);
  AdjustSize(show_all_view_, &prefsize);
  // Add one download view to the preferred size.
  if (!download_views_.empty())
    AdjustSize(*download_views_.begin(), &prefsize);
  prefsize.Enlarge(0, kTopPadding);
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

  gfx::Size close_button_size = close_button_->GetPreferredSize();
  gfx::Size show_all_size = show_all_view_->GetPreferredSize();
  int max_download_x =
      std::max<int>(0, width() - kEndPadding - close_button_size.width() -
                           kCloseAndLinkPadding - show_all_size.width());
  int next_x = show_link_only ? kStartPadding : max_download_x;
  show_all_view_->SetBounds(next_x,
                            CenterPosition(show_all_size.height(), height()),
                            show_all_size.width(),
                            show_all_size.height());
  next_x += show_all_size.width() + kCloseAndLinkPadding;
  close_button_->SizeToPreferredSize();
  close_button_->SetPosition(
      gfx::Point(next_x, CenterPosition(close_button_->height(), height())));
  if (show_link_only) {
    // Let's hide all the items.
    for (auto ri = download_views_.rbegin(); ri != download_views_.rend(); ++ri)
      (*ri)->SetVisible(false);
    return;
  }

  next_x = kStartPadding;
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

  gfx::Size close_button_size = close_button_->GetPreferredSize();
  gfx::Size show_all_size = show_all_view_->GetPreferredSize();

  // Let's compute the width available for download items, which is the width
  // of the shelf minus the "Show all downloads" link, arrow and close button
  // and the padding.
  int available_width = width() - kEndPadding - close_button_size.width() -
                        kCloseAndLinkPadding - show_all_size.width() -
                        kStartPadding;
  if (available_width <= 0)
    return false;

  gfx::Size item_size = (*download_views_.rbegin())->GetPreferredSize();
  return item_size.width() < available_width;
}

void DownloadShelfView::UpdateColorsFromTheme() {
  if (!GetThemeProvider())
    return;

  if (show_all_view_)
    ConfigureButtonForTheme(show_all_view_);

  SetBackground(views::CreateSolidBackground(
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR)));

  views::SetImageFromVectorIcon(
      close_button_, vector_icons::kCloseRoundedIcon,
      DownloadItemView::GetTextColorForThemeProvider(GetThemeProvider()));
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

void DownloadShelfView::DoOpen() {
  SetVisible(true);
  shelf_animation_.Show();
}

void DownloadShelfView::DoClose(CloseReason reason) {
  int num_in_progress = 0;
  for (size_t i = 0; i < download_views_.size(); ++i) {
    if (download_views_[i]->download()->GetState() == DownloadItem::IN_PROGRESS)
      ++num_in_progress;
  }
  RecordDownloadShelfClose(
      download_views_.size(), num_in_progress, reason == AUTOMATIC);
  parent_->SetDownloadShelfVisible(false);
  shelf_animation_.Hide();
}

void DownloadShelfView::DoHide() {
  SetVisible(false);
  parent_->ToolbarSizeChanged(false);
  parent_->SetDownloadShelfVisible(false);
}

void DownloadShelfView::DoUnhide() {
  SetVisible(true);
  parent_->ToolbarSizeChanged(true);
  parent_->SetDownloadShelfVisible(true);
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
    DownloadItem* download = download_views_[i]->download();
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
    if (!download_views_[i]->download()->GetOpened())
      return false;
  }
  return true;
}
