// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/infolist_window.h"

#include <string>
#include <vector>

#include "ash/ime/candidate_window_constants.h"
#include "base/logging.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace ime {

namespace {
// The width of an info-list.
const int kInfolistEntryWidth = 200;

// The milliseconds of the delay to show the infolist window.
const int kInfolistShowDelayMilliSeconds = 500;
// The milliseconds of the delay to hide the infolist window.
const int kInfolistHideDelayMilliSeconds = 500;

///////////////////////////////////////////////////////////////////////////////
// InfolistBorder
// The BubbleBorder subclass to draw the border and determine its position.
class InfolistBorder : public views::BubbleBorder {
 public:
  InfolistBorder();
  virtual ~InfolistBorder();

  // views::BubbleBorder implementation.
  virtual gfx::Rect GetBounds(const gfx::Rect& anchor_rect,
                              const gfx::Size& contents_size) const OVERRIDE;
  virtual gfx::Insets GetInsets() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(InfolistBorder);
};

InfolistBorder::InfolistBorder()
    : views::BubbleBorder(views::BubbleBorder::LEFT_CENTER,
                          views::BubbleBorder::NO_SHADOW,
                          SK_ColorTRANSPARENT) {
  set_paint_arrow(views::BubbleBorder::PAINT_NONE);
}

InfolistBorder::~InfolistBorder() {}

gfx::Rect InfolistBorder::GetBounds(const gfx::Rect& anchor_rect,
                                    const gfx::Size& contents_size) const {
  gfx::Rect bounds(contents_size);
  bounds.set_x(is_arrow_on_left(arrow()) ?
               anchor_rect.right() : anchor_rect.x() - contents_size.width());
  // InfolistBorder modifies the vertical position based on the arrow offset
  // although it doesn't draw the arrow. The arrow offset is the half of
  // |contents_size| by default but can be modified through the off-screen logic
  // in BubbleFrameView.
  bounds.set_y(anchor_rect.y() + contents_size.height() / 2 -
               GetArrowOffset(contents_size));
  return bounds;
}

gfx::Insets InfolistBorder::GetInsets() const {
  // This has to be specified and return empty insets to place the infolist
  // window without the gap.
  return gfx::Insets();
}

}  // namespace

// InfolistRow renderes a row of a infolist.
class InfolistEntryView : public views::View {
 public:
  InfolistEntryView(const ui::InfolistEntry& entry,
                    const gfx::FontList& title_font_list,
                    const gfx::FontList& description_font_list);
  virtual ~InfolistEntryView();

  void SetEntry(const ui::InfolistEntry& entry);

 private:
  // views::View implementation.
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

  void UpdateBackground();

  ui::InfolistEntry entry_;

  // The title label. Owned by views hierarchy.
  views::Label* title_label_;

  // The description label. Owned by views hierarchy.
  views::Label* description_label_;

  DISALLOW_COPY_AND_ASSIGN(InfolistEntryView);
};

InfolistEntryView::InfolistEntryView(const ui::InfolistEntry& entry,
                                     const gfx::FontList& title_font_list,
                                     const gfx::FontList& description_font_list)
    : entry_(entry) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  title_label_ = new views::Label(entry.title, title_font_list);
  title_label_->SetPosition(gfx::Point(0, 0));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetBorder(views::Border::CreateEmptyBorder(4, 7, 2, 4));

  description_label_ = new views::Label(entry.body, description_font_list);
  description_label_->SetPosition(gfx::Point(0, 0));
  description_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label_->SetMultiLine(true);
  description_label_->SizeToFit(kInfolistEntryWidth);
  description_label_->SetBorder(views::Border::CreateEmptyBorder(2, 17, 4, 4));
  AddChildView(title_label_);
  AddChildView(description_label_);
  UpdateBackground();
}

InfolistEntryView::~InfolistEntryView() {}

void InfolistEntryView::SetEntry(const ui::InfolistEntry& entry) {
  if (entry_ == entry)
    return;

  entry_ = entry;
  title_label_->SetText(entry_.title);
  description_label_->SetText(entry_.body);
  UpdateBackground();
}

gfx::Size InfolistEntryView::GetPreferredSize() const {
  return gfx::Size(kInfolistEntryWidth, GetHeightForWidth(kInfolistEntryWidth));
}

void InfolistEntryView::UpdateBackground() {
  if (entry_.highlighted) {
    set_background(
      views::Background::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused)));
    SetBorder(views::Border::CreateSolidBorder(
        1,
        GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_FocusedBorderColor)));
  } else {
    set_background(NULL);
    SetBorder(views::Border::CreateEmptyBorder(1, 1, 1, 1));
  }
  SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// InfolistWindow

InfolistWindow::InfolistWindow(views::View* candidate_window,
                               const std::vector<ui::InfolistEntry>& entries)
    : views::BubbleDelegateView(candidate_window, views::BubbleBorder::NONE),
      title_font_list_(gfx::Font(kJapaneseFontName, kFontSizeDelta + 15)),
      description_font_list_(gfx::Font(kJapaneseFontName,
                                       kFontSizeDelta + 11)) {
  set_use_focusless(true);
  set_accept_events(false);
  set_margins(gfx::Insets());

  set_background(
      views::Background::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_WindowBackground)));
  SetBorder(views::Border::CreateSolidBorder(
      1,
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_MenuBorderColor)));

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  views::Label* caption_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_IME_INFOLIST_WINDOW_TITLE));
  caption_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  caption_label->SetEnabledColor(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelEnabledColor));
  caption_label->SetBorder(views::Border::CreateEmptyBorder(2, 2, 2, 2));
  caption_label->set_background(views::Background::CreateSolidBackground(
      color_utils::AlphaBlend(SK_ColorBLACK,
                              GetNativeTheme()->GetSystemColor(
                                  ui::NativeTheme::kColorId_WindowBackground),
                              0x10)));

  AddChildView(caption_label);

  for (size_t i = 0; i < entries.size(); ++i) {
    entry_views_.push_back(new InfolistEntryView(
        entries[i], title_font_list_, description_font_list_));
    AddChildView(entry_views_.back());
  }
}

InfolistWindow::~InfolistWindow() {
}

void InfolistWindow::InitWidget() {
  views::Widget* widget = views::BubbleDelegateView::CreateBubble(this);
  wm::SetWindowVisibilityAnimationType(
      widget->GetNativeView(),
      wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);

  // BubbleFrameView will be initialized through CreateBubble.
  GetBubbleFrameView()->SetBubbleBorder(
      scoped_ptr<views::BubbleBorder>(new InfolistBorder()));
  SizeToContents();
}

void InfolistWindow::Relayout(const std::vector<ui::InfolistEntry>& entries) {
  size_t i = 0;
  for (; i < entries.size(); ++i) {
    if (i < entry_views_.size()) {
      entry_views_[i]->SetEntry(entries[i]);
    } else {
      InfolistEntryView* new_entry = new InfolistEntryView(
          entries[i], title_font_list_, description_font_list_);
      AddChildView(new_entry);
      entry_views_.push_back(new_entry);
    }
  }

  if (i < entry_views_.size()) {
    for (; i < entry_views_.size(); ++i)
      delete entry_views_[i];
    entry_views_.resize(entries.size());
  }

  Layout();
  GetBubbleFrameView()->bubble_border()->set_arrow_offset(0);
  SizeToContents();
}

void InfolistWindow::ShowWithDelay() {
  show_hide_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kInfolistShowDelayMilliSeconds),
      GetWidget(),
      &views::Widget::Show);
}

void InfolistWindow::HideWithDelay() {
  show_hide_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kInfolistHideDelayMilliSeconds),
      GetWidget(),
      &views::Widget::Close);
}

void InfolistWindow::ShowImmediately() {
  show_hide_timer_.Stop();
  GetWidget()->Show();
}

void InfolistWindow::HideImmediately() {
  show_hide_timer_.Stop();
  GetWidget()->Close();
}

void InfolistWindow::WindowClosing() {
  show_hide_timer_.Stop();
}

}  // namespace ime
}  // namespace ash
