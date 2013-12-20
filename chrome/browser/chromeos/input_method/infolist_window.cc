// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/infolist_window.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/chromeos/input_method/candidate_window_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/corewm/window_animations.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace input_method {

namespace {
// The width of an info-list.
const int kInfolistEntryWidth = 200;

// The milliseconds of the delay to show the infolist window.
const int kInfolistShowDelayMilliSeconds = 500;
// The milliseconds of the delay to hide the infolist window.
const int kInfolistHideDelayMilliSeconds = 500;
}

// InfolistRow renderes a row of a infolist.
class InfolistEntryView : public views::View {
 public:
  InfolistEntryView(const InfolistEntry& entry,
                    const gfx::FontList& title_font,
                    const gfx::FontList& description_font);
  virtual ~InfolistEntryView();

  void SetEntry(const InfolistEntry& entry);

 private:
  // views::View implementation.
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  void UpdateBackground();

  InfolistEntry entry_;

  // The title label. Owned by views hierarchy.
  views::Label* title_label_;

  // The description label. Owned by views hierarchy.
  views::Label* description_label_;

  DISALLOW_COPY_AND_ASSIGN(InfolistEntryView);
};

InfolistEntryView::InfolistEntryView(const InfolistEntry& entry,
                                     const gfx::FontList& title_font,
                                     const gfx::FontList& description_font)
    : entry_(entry) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  title_label_ = new views::Label(entry.title);
  title_label_->SetPosition(gfx::Point(0, 0));
  title_label_->SetFontList(title_font);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->set_border(
      views::Border::CreateEmptyBorder(4, 7, 2, 4));

  description_label_ = new views::Label(entry.body);
  description_label_->SetPosition(gfx::Point(0, 0));
  description_label_->SetFontList(description_font);
  description_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label_->SetMultiLine(true);
  description_label_->SizeToFit(kInfolistEntryWidth);
  description_label_->set_border(
      views::Border::CreateEmptyBorder(2, 17, 4, 4));
  AddChildView(title_label_);
  AddChildView(description_label_);
  UpdateBackground();
}

InfolistEntryView::~InfolistEntryView() {}

void InfolistEntryView::SetEntry(const InfolistEntry& entry) {
  if (entry_ == entry)
    return;

  entry_ = entry;
  title_label_->SetText(entry_.title);
  description_label_->SetText(entry_.body);
  UpdateBackground();
}

gfx::Size InfolistEntryView::GetPreferredSize() {
  return gfx::Size(kInfolistEntryWidth, GetHeightForWidth(kInfolistEntryWidth));
}

void InfolistEntryView::UpdateBackground() {
  if (entry_.highlighted) {
    set_background(
      views::Background::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused)));
    set_border(
        views::Border::CreateSolidBorder(1, GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_FocusedBorderColor)));
  } else {
    set_background(NULL);
    set_border(views::Border::CreateEmptyBorder(1, 1, 1, 1));
  }
  SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// InfolistEntry model

InfolistEntry::InfolistEntry(const base::string16& title,
                             const base::string16& body)
    : title(title), body(body), highlighted(false) {}

bool InfolistEntry::operator==(const InfolistEntry& other) const {
  return title == other.title && body == other.body &&
      highlighted == other.highlighted;
}

bool InfolistEntry::operator!=(const InfolistEntry& other) const {
  return !(*this == other);
}

///////////////////////////////////////////////////////////////////////////////
// InfolistWindow

InfolistWindow::InfolistWindow(const std::vector<InfolistEntry>& entries)
    : title_font_(gfx::Font(kJapaneseFontName, kFontSizeDelta + 15)),
      description_font_(gfx::Font(kJapaneseFontName, kFontSizeDelta + 11)) {
  set_background(
      views::Background::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_WindowBackground)));
  set_border(
      views::Border::CreateSolidBorder(1, GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_MenuBorderColor)));

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        0, 0, 0));

  views::Label* caption_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_INPUT_METHOD_INFOLIST_WINDOW_TITLE));
  caption_label->SetFontList(
      caption_label->font_list().DeriveFontList(kFontSizeDelta - 2));
  caption_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  caption_label->SetEnabledColor(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelEnabledColor));
  caption_label->set_border(views::Border::CreateEmptyBorder(2, 2, 2, 2));
  caption_label->set_background(views::Background::CreateSolidBackground(
      color_utils::AlphaBlend(SK_ColorBLACK,
                              GetNativeTheme()->GetSystemColor(
                                  ui::NativeTheme::kColorId_WindowBackground),
                              0x10)));

  AddChildView(caption_label);

  Relayout(entries);
}

InfolistWindow::~InfolistWindow() {
}

void InfolistWindow::InitWidget(gfx::NativeWindow parent,
                                const gfx::Rect& initial_bounds) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.parent = parent;
  params.bounds = initial_bounds;
  params.delegate = this;
  widget->Init(params);

  views::corewm::SetWindowVisibilityAnimationType(
      widget->GetNativeView(),
      views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
}

void InfolistWindow::Relayout(const std::vector<InfolistEntry>& entries) {
  size_t i = 0;
  for (; i < entries.size(); ++i) {
    if (i < entry_views_.size()) {
      entry_views_[i]->SetEntry(entries[i]);
    } else {
      InfolistEntryView* new_entry = new InfolistEntryView(
          entries[i], title_font_, description_font_);
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

views::View* InfolistWindow::GetContentsView() {
  return this;
}

void InfolistWindow::WindowClosing() {
  show_hide_timer_.Stop();
}

}  // namespace input_method
}  // namespace chromeos
