// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/input_method/infolist_window_view.h"

#include <limits>
#include <string>
#include <vector>

#include "ash/shell.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/candidate_window_constants.h"
#include "chrome/browser/chromeos/input_method/hidable_area.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace input_method {

// InfolistRow renderes a row of a infolist.
class InfolistEntryView : public views::View {
 public:
  InfolistEntryView();
  virtual ~InfolistEntryView() {}

  void Init(const gfx::Font& title_font, const gfx::Font& description_font);

  // Sets title text and description text.
  void Relayout(const InfolistWindowView::Entry& entry);

  // Selects the infolist row. Changes the appearance to make it look
  // like a selected candidate.
  void Select();

  // Unselects the infolist row. Changes the appearance to make it look
  // like an unselected candidate.
  void Unselect();

 protected:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return title_and_description_size_;
  }

 private:
  // Notifies labels of their new background colors.  Called whenever the view's
  // background color changes.
  void UpdateLabelBackgroundColors();

  // The parent candidate window that contains this view.
  InfolistWindowView* parent_infolist_window_;

  // Views created in the class will be part of tree of |this|, so these
  // child views will be deleted when |this| is deleted.

  // The title label.
  views::Label* title_label_;
  // The description label.
  views::Label* description_label_;
  // Whether the item is selected.
  bool selected_;
  // The size of the area which contains the title and the description.
  gfx::Size title_and_description_size_;

  DISALLOW_COPY_AND_ASSIGN(InfolistEntryView);
};

InfolistEntryView::InfolistEntryView()
    : title_label_(NULL),
      description_label_(NULL),
      selected_(false) {
}

void InfolistEntryView::Init(const gfx::Font& title_font,
                             const gfx::Font& description_font) {
  title_label_ = new views::Label;
  title_label_->SetPosition(gfx::Point(0, 0));
  title_label_->SetFont(title_font);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->set_border(
      views::Border::CreateEmptyBorder(4, 7, 2, 4));

  description_label_ = new views::Label;
  description_label_->SetPosition(gfx::Point(0, 0));
  description_label_->SetFont(description_font);
  description_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label_->SetMultiLine(true);
  description_label_->set_border(
      views::Border::CreateEmptyBorder(2, 17, 4, 4));
  AddChildView(title_label_);
  AddChildView(description_label_);
  UpdateLabelBackgroundColors();
}

void InfolistEntryView::Relayout(const InfolistWindowView::Entry& entry) {
  const string16 title = UTF8ToUTF16(entry.title);
  const string16 description = UTF8ToUTF16(entry.body);

  if ((title_label_->text() == title) &&
      (description_label_->text() == description)) {
    return;
  }

  title_label_->SetText(title);
  const gfx::Size title_size = title_label_->GetPreferredSize();
  title_label_->SetSize(title_size);

  description_label_->SetText(description);
  description_label_->SizeToFit(200);
  const gfx::Size description_size = description_label_->size();
  description_label_->SetPosition(gfx::Point(0, title_size.height()));

  title_and_description_size_ =
      gfx::Size(200, description_size.height() + title_size.height());
}

void InfolistEntryView::Select() {
  if (selected_)
    return;
  selected_ = true;
  set_background(views::Background::CreateSolidBackground(
      kSelectedInfolistRowBackgroundColor));
  set_border(
      views::Border::CreateSolidBorder(1, kSelectedInfolistRowFrameColor));
  UpdateLabelBackgroundColors();
}

void InfolistEntryView::Unselect() {
  if (!selected_)
    return;
  selected_ = false;
  set_background(NULL);
  set_border(views::Border::CreateEmptyBorder(1, 1, 1, 1));
  UpdateLabelBackgroundColors();
}

void InfolistEntryView::UpdateLabelBackgroundColors() {
  SkColor color = background() ?
      background()->get_color() : kDefaultBackgroundColor;
  title_label_->SetBackgroundColor(color);
  description_label_->SetBackgroundColor(color);
}

///////////////////////////////////////////////////////////////////////////////
// InfolistWindowView

InfolistWindowView::InfolistWindowView()
    : infolist_area_(new views::View),
      title_font_(new gfx::Font(kJapaneseFontName, kFontSizeDelta + 15)),
      description_font_(new gfx::Font(kJapaneseFontName, kFontSizeDelta + 11)) {
}

InfolistWindowView::~InfolistWindowView() {
  infolist_area_->RemoveAllChildViews(false);
}

void InfolistWindowView::Init() {
  set_background(
      views::Background::CreateSolidBackground(kDefaultBackgroundColor));
  set_border(views::Border::CreateSolidBorder(1, kFrameColor));

  views::BoxLayout* layout = new views::BoxLayout(views::BoxLayout::kVertical,
                                                  0, 0, 0);
  SetLayoutManager(layout);  // |this| owns |layout|.

  views::Label* caption_label = new views::Label;
  caption_label->SetFont(caption_label->font().DeriveFont(kFontSizeDelta - 2));
  caption_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  caption_label->SetText(
      l10n_util::GetStringUTF16(IDS_INPUT_METHOD_INFOLIST_WINDOW_TITLE));
  caption_label->set_border(views::Border::CreateEmptyBorder(2, 2, 2, 2));
  caption_label->set_background(
      views::Background::CreateSolidBackground(kInfolistTitleBackgroundColor));
  caption_label->SetBackgroundColor(caption_label->background()->get_color());

  AddChildView(caption_label);
  AddChildView(infolist_area_);
}

void InfolistWindowView::Relayout(const std::vector<Entry>& entries,
                                  size_t focused_index) {
  infolist_area_->RemoveAllChildViews(false);
  if (entries.empty())
    return;

  for (size_t i = infolist_views_.size(); i < entries.size(); ++i) {
    InfolistEntryView* infolist_row = new InfolistEntryView();
    infolist_row->Init(*title_font_.get(), *description_font_.get());
    infolist_views_.push_back(infolist_row);
  }

  views::BoxLayout* layout = new views::BoxLayout(views::BoxLayout::kVertical,
                                                  0, 0, 0);
  // |infolist_area_| owns |layout|.
  infolist_area_->SetLayoutManager(layout);

  for (size_t i = 0; i < entries.size(); ++i) {
    InfolistEntryView* infolist_row = infolist_views_[i];
    infolist_row->Relayout(entries[i]);
    infolist_row->Unselect();
    infolist_area_->AddChildView(infolist_row);
  }
  if (focused_index < infolist_views_.size())
    infolist_views_[focused_index]->Select();
  infolist_area_->SchedulePaint();
}

// static
const size_t InfolistWindowView::InvalidFocusIndex() {
  return std::numeric_limits<size_t>::max();
}

}  // namespace input_method
}  // namespace chromeos
