// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/input_method/infolist_window_view.h"

#include <string>
#include <vector>

#include "ash/shell.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/candidate_window_constants.h"
#include "chrome/browser/chromeos/input_method/hidable_area.h"
#include "chrome/browser/chromeos/input_method/ibus_ui_controller.h"
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
class InfolistView : public views::View {
 public:
  InfolistView();
  virtual ~InfolistView() {}

  void Init(const gfx::Font& title_font, const gfx::Font& description_font);

  // Sets title text and description text.
  void SetTitleAndDescriptionText(const string16& title,
                                  const string16& description);

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

  DISALLOW_COPY_AND_ASSIGN(InfolistView);
};

InfolistView::InfolistView()
    : title_label_(NULL),
      description_label_(NULL),
      selected_(false) {
}

void InfolistView::Init(const gfx::Font& title_font,
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

void InfolistView::SetTitleAndDescriptionText(const string16& title,
                                              const string16& description) {
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

void InfolistView::Select() {
  if (selected_)
    return;
  selected_ = true;
  set_background(views::Background::CreateSolidBackground(
      kSelectedInfolistRowBackgroundColor));
  set_border(
      views::Border::CreateSolidBorder(1, kSelectedInfolistRowFrameColor));
  UpdateLabelBackgroundColors();
}

void InfolistView::Unselect() {
  if (!selected_)
    return;
  selected_ = false;
  set_background(NULL);
  set_border(views::Border::CreateEmptyBorder(1, 1, 1, 1));
  UpdateLabelBackgroundColors();
}

void InfolistView::UpdateLabelBackgroundColors() {
  SkColor color = background() ?
      background()->get_color() : kDefaultBackgroundColor;
  title_label_->SetBackgroundColor(color);
  description_label_->SetBackgroundColor(color);
}

InfolistWindowView::InfolistWindowView(views::Widget* parent_frame,
                                       views::Widget* candidate_window_frame)
    : parent_frame_(parent_frame),
      candidate_window_frame_(candidate_window_frame),
      infolist_area_(NULL),
      visible_(false),
      title_font_(new gfx::Font(kJapaneseFontName, kFontSizeDelta + 15)),
      description_font_(new gfx::Font(kJapaneseFontName, kFontSizeDelta + 11)) {
}

InfolistWindowView::~InfolistWindowView() {
  if (infolist_area_ != NULL)
    infolist_area_->RemoveAllChildViews(false);

  for (size_t i = 0; i < infolist_views_.size(); ++i) {
    delete infolist_views_[i];
  }
}

void InfolistWindowView::Init() {
  set_background(
      views::Background::CreateSolidBackground(kDefaultBackgroundColor));
  set_border(views::Border::CreateSolidBorder(1, kFrameColor));
  infolist_area_ = new views::View;

  views::BoxLayout* layout = new views::BoxLayout(views::BoxLayout::kVertical,
                                                  0, 0, 0);
  SetLayoutManager(layout);  // |this| owns |layout|.

  views::Label* caption_label = new views::Label;
  caption_label->SetFont(caption_label->font().DeriveFont(kFontSizeDelta - 2));
  caption_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  caption_label->SetText(
      l10n_util::GetStringUTF16(IDS_INPUT_METHOD_INFOLIST_WINDOW_TITLE));
  caption_label->set_border(
      views::Border::CreateEmptyBorder(2, 2, 2, 2));
  caption_label->set_background(
      views::Background::CreateSolidBackground(kInfolistTitleBackgroundColor));
  caption_label->SetBackgroundColor(
      caption_label->background()->get_color());

  AddChildView(caption_label);
  AddChildView(infolist_area_);
}

void InfolistWindowView::Hide() {
  visible_ = false;
  show_hide_timer_.Stop();
  parent_frame_->Hide();
}

void InfolistWindowView::DelayHide(unsigned int milliseconds) {
  visible_ = false;
  show_hide_timer_.Stop();
  show_hide_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(milliseconds),
                         this,
                         &InfolistWindowView::OnShowHideTimer);
}

void InfolistWindowView::Show() {
  visible_ = true;
  show_hide_timer_.Stop();
  ResizeAndMoveParentFrame();
  parent_frame_->Show();
}

void InfolistWindowView::DelayShow(unsigned int milliseconds) {
  visible_ = true;
  show_hide_timer_.Stop();
  show_hide_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(milliseconds),
                         this,
                         &InfolistWindowView::OnShowHideTimer);
}

void InfolistWindowView::OnShowHideTimer() {
  if (visible_)
    Show();
  else
    Hide();
}

void InfolistWindowView::UpdateCandidates(const std::vector<Entry>& entries,
                                          size_t focused_index) {
  if (entries.empty())
    return;

  for (size_t i = infolist_views_.size(); i < entries.size(); ++i) {
    InfolistView* infolist_row = new InfolistView();
    infolist_row->Init(*title_font_.get(), *description_font_.get());
    infolist_views_.push_back(infolist_row);
  }
  infolist_area_->RemoveAllChildViews(false);

  views::BoxLayout* layout = new views::BoxLayout(views::BoxLayout::kVertical,
                                                  0, 0, 0);
  // |infolist_area_| owns |layout|.
  infolist_area_->SetLayoutManager(layout);

  for (size_t i = 0; i < entries.size(); ++i) {
    InfolistView* infolist_row = infolist_views_[i];
    infolist_row->SetTitleAndDescriptionText(
        UTF8ToUTF16(entries[i].title),
        UTF8ToUTF16(entries[i].body));
    infolist_row->Unselect();
    infolist_area_->AddChildView(infolist_row);
  }
  if (focused_index < infolist_views_.size())
    infolist_views_[focused_index]->Select();
  infolist_area_->SchedulePaint();
  ResizeAndMoveParentFrame();
}

void InfolistWindowView::ResizeAndMoveParentFrame() {
  int x, y;
  gfx::Rect old_bounds = parent_frame_->GetClientAreaBoundsInScreen();
  gfx::Rect screen_bounds = ash::Shell::GetScreen()->GetDisplayNearestWindow(
      parent_frame_->GetNativeView()).work_area();
  // The size.
  gfx::Rect frame_bounds = old_bounds;
  gfx::Size size = GetPreferredSize();
  frame_bounds.set_size(size);

  gfx::Rect candidatewindow_bounds;
  if (candidate_window_frame_ != NULL) {
    candidatewindow_bounds =
        candidate_window_frame_->GetClientAreaBoundsInScreen();
  }

  if (screen_bounds.height() == 0 || screen_bounds.width() == 0) {
    x = candidatewindow_bounds.right();
    y = candidatewindow_bounds.y();
  }
  if (candidatewindow_bounds.right() + frame_bounds.width() >
      screen_bounds.right()) {
    x = candidatewindow_bounds.x() - frame_bounds.width();
  } else {
    x = candidatewindow_bounds.right();
  }
  if (candidatewindow_bounds.y() + frame_bounds.height() >
      screen_bounds.bottom()) {
    y = screen_bounds.bottom() - frame_bounds.height();
  } else {
    y = candidatewindow_bounds.y();
  }

  frame_bounds.set_x(x);
  frame_bounds.set_y(y);

  // Move the window per the cursor location.
  // SetBounds() is not cheap. Only call this when it is really changed.
  if (frame_bounds != old_bounds)
    parent_frame_->SetBounds(frame_bounds);
}

void InfolistWindowView::VisibilityChanged(View* starting_from,
                                           bool is_visible) {
  if (is_visible) {
    // If the visibility of candidate window is changed,
    // we should move the frame to the right position.
    ResizeAndMoveParentFrame();
  }
}

void InfolistWindowView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  View::OnBoundsChanged(previous_bounds);
  ResizeAndMoveParentFrame();
}

}  // namespace input_method
}  // namespace chromeos
