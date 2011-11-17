// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/candidate_window.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/candidate_window_view.h"
#include "chrome/browser/chromeos/input_method/ibus_ui_controller.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/screen.h"
#include "ui/views/window/non_client_view.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/events/event.h"
#include "views/layout/fill_layout.h"
#include "views/layout/grid_layout.h"
#include "views/widget/widget.h"

namespace chromeos {
namespace input_method {

namespace {

// Colors used in the candidate window UI.
const SkColor kFrameColor = SkColorSetRGB(0x96, 0x96, 0x96);
const SkColor kShortcutBackgroundColor = SkColorSetARGB(0x10, 0x3, 0x4, 0xf);
const SkColor kSelectedRowBackgroundColor = SkColorSetRGB(0xd1, 0xea, 0xff);
const SkColor kDefaultBackgroundColor = SkColorSetRGB(0xff, 0xff, 0xff);
const SkColor kSelectedRowFrameColor = SkColorSetRGB(0x7f, 0xac, 0xdd);
const SkColor kFooterTopColor = SkColorSetRGB(0xff, 0xff, 0xff);
const SkColor kFooterBottomColor = SkColorSetRGB(0xee, 0xee, 0xee);
const SkColor kShortcutColor = SkColorSetRGB(0x61, 0x61, 0x61);
const SkColor kDisabledShortcutColor = SkColorSetRGB(0xcc, 0xcc, 0xcc);
const SkColor kAnnotationColor = SkColorSetRGB(0x88, 0x88, 0x88);
const SkColor kSelectedInfolistRowBackgroundColor =
    SkColorSetRGB(0xee, 0xee, 0xee);
const SkColor kSelectedInfolistRowFrameColor = SkColorSetRGB(0xcc, 0xcc, 0xcc);
const SkColor kInfolistTitleBackgroundColor = SkColorSetRGB(0xdd, 0xdd, 0xdd);

// We'll use a bigger font size, so Chinese characters are more readable
// in the candidate window.
#if defined(CROS_FONTS_USING_BCI)
const int kFontSizeDelta = 1;
#else
const int kFontSizeDelta = 2;
#endif

// The minimum width of candidate labels in the vertical candidate
// window. We use this value to prevent the candidate window from being
// too narrow when all candidates are short.
const int kMinCandidateLabelWidth = 100;
// The maximum width of candidate labels in the vertical candidate
// window. We use this value to prevent the candidate window from being
// too wide when one of candidates are long.
const int kMaxCandidateLabelWidth = 500;
// The minimum width of preedit area. We use this value to prevent the
// candidate window from being too narrow when candidate lists are not shown.
const int kMinPreeditAreaWidth = 134;

// The milliseconds of the delay to show the infolist window.
const int kInfolistShowDelayMilliSeconds = 500;
// The milliseconds of the delay to hide the infolist window.
const int kInfolistHideDelayMilliSeconds = 500;

// VerticalCandidateLabel is used for rendering candidate text in
// the vertical candidate window.
class VerticalCandidateLabel : public views::Label {
 public:
  VerticalCandidateLabel() {}

 private:
  virtual ~VerticalCandidateLabel() {}

  // Returns the preferred size, but guarantees that the width has at
  // least kMinCandidateLabelWidth pixels.
  virtual gfx::Size GetPreferredSize() {
    gfx::Size size = Label::GetPreferredSize();
    // Hack. +2 is needed to prevent labels from getting elided like
    // "abc..." in some cases. TODO(satorux): Figure out why it's
    // necessary.
    size.set_width(size.width() + 2);
    if (size.width() < kMinCandidateLabelWidth) {
      size.set_width(kMinCandidateLabelWidth);
    }
    if (size.width() > kMaxCandidateLabelWidth) {
      size.set_width(kMaxCandidateLabelWidth);
    }
    return size;
  }

  DISALLOW_COPY_AND_ASSIGN(VerticalCandidateLabel);
};

// Wraps the given view with some padding, and returns it.
views::View* WrapWithPadding(views::View* view, const gfx::Insets& insets) {
  views::View* wrapper = new views::View;
  // Use GridLayout to give some insets inside.
  views::GridLayout* layout = new views::GridLayout(wrapper);
  wrapper->SetLayoutManager(layout);  // |wrapper| owns |layout|.
  layout->SetInsets(insets);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(
      views::GridLayout::FILL, views::GridLayout::FILL,
      1, views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);

  // Add the view contents.
  layout->AddView(view);  // |view| is owned by |wraper|, not |layout|.
  return wrapper;
}

// Creates shortcut text from the given index and the orientation.
string16 CreateShortcutText(int index,
                            InputMethodLookupTable::Orientation orientation) {
  // Choose the character used for the shortcut label.
  const char kShortcutCharacters[] = "1234567890ABCDEF";
  // The default character should not be used but just in case.
  char shortcut_character = L'?';
  // -1 to exclude the null character at the end.
  if (index < static_cast<int>(arraysize(kShortcutCharacters) - 1))
    shortcut_character = kShortcutCharacters[index];

  std::string shortcut_text(1, shortcut_character);
  if (orientation != InputMethodLookupTable::kVertical)
    shortcut_text += '.';
  return ASCIIToUTF16(shortcut_text);
}

// Creates the shortcut label, and returns it (never returns NULL).
// The label text is not set in this function.
views::Label* CreateShortcutLabel(
    InputMethodLookupTable::Orientation orientation) {
  // Create the shortcut label. The label will be owned by
  // |wrapped_shortcut_label|, hence it's deleted when
  // |wrapped_shortcut_label| is deleted.
  views::Label* shortcut_label = new views::Label;

  if (orientation == InputMethodLookupTable::kVertical) {
    shortcut_label->SetFont(
        shortcut_label->font().DeriveFont(kFontSizeDelta, gfx::Font::BOLD));
  } else {
    shortcut_label->SetFont(
        shortcut_label->font().DeriveFont(kFontSizeDelta));
  }
  // TODO(satorux): Maybe we need to use language specific fonts for
  // candidate_label, like Chinese font for Chinese input method?
  shortcut_label->SetEnabledColor(kShortcutColor);
  shortcut_label->SetDisabledColor(kDisabledShortcutColor);

  return shortcut_label;
}

// Wraps the shortcut label, then decorates wrapped shortcut label
// and returns it (never returns NULL).
// The label text is not set in this function.
views::View* CreateWrappedShortcutLabel(
    views::Label* shortcut_label,
    InputMethodLookupTable::Orientation orientation) {
  // Wrap it with padding.
  const gfx::Insets kVerticalShortcutLabelInsets(1, 6, 1, 6);
  const gfx::Insets kHorizontalShortcutLabelInsets(1, 3, 1, 0);
  const gfx::Insets insets =
      (orientation == InputMethodLookupTable::kVertical ?
       kVerticalShortcutLabelInsets :
       kHorizontalShortcutLabelInsets);
  views::View* wrapped_shortcut_label =
      WrapWithPadding(shortcut_label, insets);

  // Add decoration based on the orientation.
  if (orientation == InputMethodLookupTable::kVertical) {
    // Set the background color.
    wrapped_shortcut_label->set_background(
        views::Background::CreateSolidBackground(
            kShortcutBackgroundColor));
    shortcut_label->SetBackgroundColor(
        wrapped_shortcut_label->background()->get_color());
  }

  return wrapped_shortcut_label;
}

// Creates the candidate label, and returns it (never returns NULL).
// The label text is not set in this function.
views::Label* CreateCandidateLabel(
    InputMethodLookupTable::Orientation orientation) {
  views::Label* candidate_label = NULL;

  // Create the candidate label. The label will be added to |this| as a
  // child view, hence it's deleted when |this| is deleted.
  if (orientation == InputMethodLookupTable::kVertical) {
    candidate_label = new VerticalCandidateLabel;
  } else {
    candidate_label = new views::Label;
  }

  // Change the font size.
  candidate_label->SetFont(
      candidate_label->font().DeriveFont(kFontSizeDelta));
  candidate_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  return candidate_label;
}

// Creates the annotation label, and return it (never returns NULL).
// The label text is not set in this function.
views::Label* CreateAnnotationLabel(
    InputMethodLookupTable::Orientation orientation) {
  // Create the annotation label.
  views::Label* annotation_label = new views::Label;

  // Change the font size and color.
  annotation_label->SetFont(
      annotation_label->font().DeriveFont(kFontSizeDelta));
  annotation_label->SetEnabledColor(kAnnotationColor);
  annotation_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  return annotation_label;
}

// Computes shortcut column width.
int ComputeShortcutColumnWidth(
    const InputMethodLookupTable& lookup_table) {
  int shortcut_column_width = 0;
  // Create the shortcut label. The label will be owned by
  // |wrapped_shortcut_label|, hence it's deleted when
  // |wrapped_shortcut_label| is deleted.
  views::Label* shortcut_label = CreateShortcutLabel(lookup_table.orientation);
  scoped_ptr<views::View> wrapped_shortcut_label(
      CreateWrappedShortcutLabel(shortcut_label, lookup_table.orientation));

  // Compute the max width in shortcut labels.
  // We'll create temporary shortcut labels, and choose the largest width.
  for (int i = 0; i < lookup_table.page_size; ++i) {
    shortcut_label->SetText(CreateShortcutText(i, lookup_table.orientation));
    shortcut_column_width =
        std::max(shortcut_column_width,
                 wrapped_shortcut_label->GetPreferredSize().width());
  }

  return shortcut_column_width;
}

// Computes the page index. For instance, if the page size is 9, and the
// cursor is pointing to 13th candidate, the page index will be 1 (2nd
// page, as the index is zero-origin). Returns -1 on error.
int ComputePageIndex(const InputMethodLookupTable& lookup_table) {
  if (lookup_table.page_size > 0)
    return lookup_table.cursor_absolute_index / lookup_table.page_size;
  return -1;
}

// Computes candidate column width.
int ComputeCandidateColumnWidth(
    const InputMethodLookupTable& lookup_table) {
  int candidate_column_width = 0;
  scoped_ptr<views::Label> candidate_label(
      CreateCandidateLabel(lookup_table.orientation));

  // Compute the start index of |lookup_table_|.
  const int current_page_index = ComputePageIndex(lookup_table);
  if (current_page_index < 0)
    return 0;
  const size_t start_from = current_page_index * lookup_table.page_size;

  // Compute the max width in candidate labels.
  // We'll create temporary candidate labels, and choose the largest width.
  for (size_t i = 0; i + start_from < lookup_table.candidates.size(); ++i) {
    const size_t index = start_from + i;

    candidate_label->SetText(
        UTF8ToUTF16(lookup_table.candidates[index]));
    candidate_column_width =
        std::max(candidate_column_width,
                 candidate_label->GetPreferredSize().width());
  }

  return candidate_column_width;
}

// Computes annotation column width.
int ComputeAnnotationColumnWidth(const InputMethodLookupTable& lookup_table) {
  int annotation_column_width = 0;
  scoped_ptr<views::Label> annotation_label(
      CreateAnnotationLabel(lookup_table.orientation));

  // Compute the start index of |lookup_table_|.
  const int current_page_index = ComputePageIndex(lookup_table);
  if (current_page_index < 0)
    return 0;
  const size_t start_from = current_page_index * lookup_table.page_size;

  // Compute max width in annotation labels.
  // We'll create temporary annotation labels, and choose the largest width.
  for (size_t i = 0; i + start_from < lookup_table.annotations.size(); ++i) {
    const size_t index = start_from + i;

    annotation_label->SetText(
        UTF8ToUTF16(lookup_table.annotations[index]));
    annotation_column_width =
        std::max(annotation_column_width,
                 annotation_label->GetPreferredSize().width());
  }

  return annotation_column_width;
}

}  // namespace

// HidableArea is used as an area to place optional information that can be
// turned displaying off if it is unnecessary.
class HidableArea : public views::View {
 public:
  HidableArea() {
    // |place_holder_| will be deleted by scoped_ptr, rather than
    // the standard owning relation of views::View.
    //
    // This is because we swap the contents of HidableArea between
    // |place_holder_| (to show nothing) and |contents_| (to show something).
    // In other words, the HidableArea only contains one of the two views
    // hence cannot own the two views at the same time.
    place_holder_.reset(new views::View);
    place_holder_->set_parent_owned(false);  // Won't own

    // Initially show nothing.
    SetLayoutManager(new views::FillLayout);
    AddChildView(place_holder_.get());
  }

  // Sets the content view.
  void SetContents(views::View* contents) {
    contents_.reset(contents);
    contents_->set_parent_owned(false);  // Won't own
  }

  // Shows the content.
  void Show() {
    if (contents_.get() && contents_->parent() != this) {
      RemoveAllChildViews(false);  // Don't delete child views.
      AddChildView(contents_.get());
    }
  }

  // Hides the content.
  void Hide() {
    if (IsShown()) {
      RemoveAllChildViews(false);  // Don't delete child views.
      AddChildView(place_holder_.get());
    }
  }

  // Returns whether the content is already set and shown.
  bool IsShown() const {
    return contents_.get() && contents_->parent() == this;
  }

  // Returns the content.
  views::View* contents() {
    return contents_.get();
  }

 private:
  scoped_ptr<views::View> contents_;
  scoped_ptr<views::View> place_holder_;

  DISALLOW_COPY_AND_ASSIGN(HidableArea);
};

// InformationTextArea is a HidableArea having a single Label in it.
class InformationTextArea : public HidableArea {
 public:
  // Specify the alignment and initialize the control.
  InformationTextArea(views::Label::Alignment align, int minWidth)
  : minWidth_(minWidth) {
    label_ = new views::Label;
    label_->SetHorizontalAlignment(align);

    const gfx::Insets kInsets(2, 2, 2, 4);
    views::View* contents = WrapWithPadding(label_, kInsets);
    SetContents(contents);
    contents->set_border(
        views::Border::CreateSolidBorder(1, kFrameColor));
    contents->set_background(
        views::Background::CreateVerticalGradientBackground(
            kFooterTopColor,
            kFooterBottomColor));
    label_->SetBackgroundColor(contents->background()->get_color());
  }

  // Set the displayed text.
  void SetText(const std::string& utf8_text) {
    label_->SetText(UTF8ToUTF16(utf8_text));
  }

 protected:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size = HidableArea::GetPreferredSize();
    // Hack. +2 is needed as the same reason as in VerticalCandidateLabel
    size.set_width(size.width() + 2);
    if (size.width() < minWidth_) {
      size.set_width(minWidth_);
    }
    return size;
  }

 private:
  views::Label* label_;
  int minWidth_;

  DISALLOW_COPY_AND_ASSIGN(InformationTextArea);
};

// CandidateRow renderes a row of a candidate.
class CandidateView : public views::View {
 public:
  CandidateView(CandidateWindowView* parent_candidate_window,
                int index_in_page,
                InputMethodLookupTable::Orientation orientation);
  virtual ~CandidateView() {}
  // Initializes the candidate view with the given column widths.
  // A width of 0 means that the column is resizable.
  void Init(int shortcut_column_width,
            int candidate_column_width,
            int annotation_column_width);

  // Sets candidate text to the given text.
  void SetCandidateText(const string16& text);

  // Sets shortcut text to the given text.
  void SetShortcutText(const string16& text);

  // Sets annotation text to the given text.
  void SetAnnotationText(const string16& text);

  // Sets infolist icon.
  void SetInfolistIcon(bool enable);

  // Selects the candidate row. Changes the appearance to make it look
  // like a selected candidate.
  void Select();

  // Unselects the candidate row. Changes the appearance to make it look
  // like an unselected candidate.
  void Unselect();

  // Enables or disables the candidate row based on |enabled|. Changes the
  // appearance to make it look like unclickable area.
  void SetRowEnabled(bool enabled);

  // Returns the relative position of the candidate label.
  gfx::Point GetCandidateLabelPosition() const;

 private:
  // Overridden from View:
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;

  // Notifies labels of their new background colors.  Called whenever the view's
  // background color changes.
  void UpdateLabelBackgroundColors();

  // Zero-origin index in the current page.
  int index_in_page_;

  // The orientation of the candidate view.
  InputMethodLookupTable::Orientation orientation_;

  // The parent candidate window that contains this view.
  CandidateWindowView* parent_candidate_window_;

  // Views created in the class will be part of tree of |this|, so these
  // child views will be deleted when |this| is deleted.

  // The shortcut label renders shortcut numbers like 1, 2, and 3.
  views::Label* shortcut_label_;
  // The candidate label renders candidates.
  views::Label* candidate_label_;
  // The annotation label renders annotations.
  views::Label* annotation_label_;

  // The infolist icon.
  views::Label* infolist_label_;
  bool infolist_icon_enabled_;

  DISALLOW_COPY_AND_ASSIGN(CandidateView);
};

class InfolistView;

// InfolistWindowView is the main container of the infolist window UI.
class InfolistWindowView : public views::View {
 public:
  InfolistWindowView(views::Widget* parent_frame,
                     views::Widget* candidate_window_frame);
  virtual ~InfolistWindowView();
  void Init();
  void Show();
  void DelayShow(unsigned int milliseconds);
  void Hide();
  void DelayHide(unsigned int milliseconds);
  void UpdateCandidates(const InputMethodLookupTable& lookup_table);

  void ResizeAndMoveParentFrame();

 protected:
  // Override View::VisibilityChanged()
  virtual void VisibilityChanged(View* starting_from, bool is_visible) OVERRIDE;

  // Override View::OnBoundsChanged()
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

 private:
  // Called by show_hide_timer_
  void OnShowHideTimer();

  // The lookup table (candidates).
  InputMethodLookupTable lookup_table_;

  // The parent frame.
  views::Widget* parent_frame_;

  // The candidate window frame.
  views::Widget* candidate_window_frame_;

  // The infolist area is where the meanings and the usages of the words are
  // rendered.
  views::View* infolist_area_;
  // The infolist views are used for rendering the meanings and the usages of
  // the words.
  std::vector<InfolistView*> infolist_views_;

  bool visible_;

  base::OneShotTimer<InfolistWindowView> show_hide_timer_;

  DISALLOW_COPY_AND_ASSIGN(InfolistWindowView);
};

// InfolistRow renderes a row of a infolist.
class InfolistView : public views::View {
 public:
  explicit InfolistView(InfolistWindowView* parent_infolist_window);
  virtual ~InfolistView() {}

  void Init();

  // Sets title text to the given text.
  void SetTitleText(const std::wstring& text);

  // Sets description text to the given text.
  void SetDescriptionText(const std::wstring& text);

  // Selects the infolist row. Changes the appearance to make it look
  // like a selected candidate.
  void Select();

  // Unselects the infolist row. Changes the appearance to make it look
  // like an unselected candidate.
  void Unselect();

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

  DISALLOW_COPY_AND_ASSIGN(InfolistView);
};

// The implementation of CandidateWindowController.
// CandidateWindowController controls the CandidateWindow.
class CandidateWindowController::Impl : public CandidateWindowView::Observer,
                                        public IBusUiController::Observer {
 public:
  Impl();
  virtual ~Impl();

  // Initializes the candidate window. Returns true on success.
  bool Init();

 private:
  // CandidateWindowView::Observer implementation.
  virtual void OnCandidateCommitted(int index,
                                    int button,
                                    int flags);

  // Creates the candidate window view.
  void CreateView();

  // IBusUiController::Observer overrides.
  virtual void OnHideAuxiliaryText();
  virtual void OnHideLookupTable();
  virtual void OnHidePreeditText();
  virtual void OnSetCursorLocation(int x, int y, int width, int height);
  virtual void OnUpdateAuxiliaryText(const std::string& utf8_text,
                                     bool visible);
  virtual void OnUpdateLookupTable(const InputMethodLookupTable& lookup_table);
  virtual void OnUpdatePreeditText(const std::string& utf8_text,
                                   unsigned int cursor, bool visible);
  virtual void OnConnectionChange(bool connected);

  // The controller is used for communicating with the IBus daemon.
  scoped_ptr<IBusUiController> ibus_ui_controller_;

  // The candidate window view.
  CandidateWindowView* candidate_window_;

  // This is the outer frame of the candidate window view. The frame will
  // own |candidate_window_|.
  scoped_ptr<views::Widget> frame_;

  // The infolist window view.
  InfolistWindowView* infolist_window_;

  // This is the outer frame of the infolist window view. The frame will
  // own |infolist_window_|.
  scoped_ptr<views::Widget> infolist_frame_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

CandidateView::CandidateView(
    CandidateWindowView* parent_candidate_window,
    int index_in_page,
    InputMethodLookupTable::Orientation orientation)
    : index_in_page_(index_in_page),
      orientation_(orientation),
      parent_candidate_window_(parent_candidate_window),
      shortcut_label_(NULL),
      candidate_label_(NULL),
      annotation_label_(NULL),
      infolist_label_(NULL),
      infolist_icon_enabled_(false) {
}

void CandidateView::Init(int shortcut_column_width,
                         int candidate_column_width,
                         int annotation_column_width) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);  // |this| owns |layout|.

  // Create Labels.
  shortcut_label_ = CreateShortcutLabel(orientation_);
  views::View* wrapped_shortcut_label =
      CreateWrappedShortcutLabel(shortcut_label_, orientation_);
  candidate_label_ = CreateCandidateLabel(orientation_);
  annotation_label_ = CreateAnnotationLabel(orientation_);

  // Initialize the column set with three columns.
  views::ColumnSet* column_set = layout->AddColumnSet(0);

  // If orientation is vertical, each column width is fixed.
  // Otherwise the width is resizable.
  const views::GridLayout::SizeType column_type =
      orientation_ == InputMethodLookupTable::kVertical ?
          views::GridLayout::FIXED : views::GridLayout::USE_PREF;

  const int padding_column_width =
      orientation_ == InputMethodLookupTable::kVertical ? 4 : 6;

  // Set shortcut column type and width.
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0, column_type, shortcut_column_width, 0);
  column_set->AddPaddingColumn(0, padding_column_width);

  // Set candidate column type and width.
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0, column_type, candidate_column_width, 0);
  column_set->AddPaddingColumn(0, padding_column_width);

  // Set annotation column type and width.
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0, column_type, annotation_column_width, 0);

  if (orientation_ == InputMethodLookupTable::kVertical) {
    infolist_label_= new views::Label;
    infolist_label_->SetFont(
        infolist_label_->font().DeriveFont(kFontSizeDelta));
    column_set->AddPaddingColumn(0, 1);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                          0, views::GridLayout::FIXED, 4, 0);
    column_set->AddPaddingColumn(0, 2);
  } else {
    column_set->AddPaddingColumn(0, padding_column_width);
  }

  // Add the shortcut label, the candidate label, and annotation label.
  layout->StartRow(0, 0);
  // |wrapped_shortcut_label|, |candidate_label_|, and |annotation_label_|
  // will be owned by |this|.
  layout->AddView(wrapped_shortcut_label);
  layout->AddView(candidate_label_);
  layout->AddView(annotation_label_);
  if (orientation_ == InputMethodLookupTable::kVertical) {
    layout->AddView(WrapWithPadding(infolist_label_,
        gfx::Insets(2, 0, 2, 0)));
  }
  UpdateLabelBackgroundColors();
}

void CandidateView::SetCandidateText(const string16& text) {
  candidate_label_->SetText(text);
}

void CandidateView::SetShortcutText(const string16& text) {
  shortcut_label_->SetText(text);
}

void CandidateView::SetAnnotationText(const string16& text) {
  annotation_label_->SetText(text);
}

void CandidateView::SetInfolistIcon(bool enable) {
  if (!infolist_label_ || (infolist_icon_enabled_ == enable))
    return;
  infolist_icon_enabled_ = enable;
  infolist_label_->set_background(enable ?
      views::Background::CreateSolidBackground(kSelectedRowFrameColor) : NULL);
  UpdateLabelBackgroundColors();
  SchedulePaint();
}

void CandidateView::Select() {
  set_background(
      views::Background::CreateSolidBackground(kSelectedRowBackgroundColor));
  set_border(views::Border::CreateSolidBorder(1, kSelectedRowFrameColor));
  UpdateLabelBackgroundColors();
  // Need to call SchedulePaint() for background and border color changes.
  SchedulePaint();
}

void CandidateView::Unselect() {
  set_background(NULL);
  set_border(NULL);
  UpdateLabelBackgroundColors();
  SchedulePaint();  // See comments at Select().
}

void CandidateView::SetRowEnabled(bool enabled) {
  shortcut_label_->SetEnabled(enabled);
}

gfx::Point CandidateView::GetCandidateLabelPosition() const {
  return candidate_label_->GetMirroredPosition();
}

bool CandidateView::OnMousePressed(const views::MouseEvent& event) {
  // TODO(kinaba): investigate a way to delay the commit until OnMouseReleased.
  // Mouse-down selection is a temporally workaround for crosbug.com/11423.
  //
  // Typical Windows/Mac input methods select candidates at the point of mouse-
  // up event. This would be implemented in our CandidateWindow like this:
  //   1. Return true form CandidateView::OnMousePressed, to indicate that we
  //     need to capture mouse and to receive drag/mouse-up events.
  //   2. In response to the drag events (OnMouseDragged()), we update our
  //     selection by calling parent_candidate_window_->OOnCandidatePressed().
  //   3. In response to the mouse-up event (OnMouseReleased()), we commit the
  //     selection by parent_candidate_window_->CommitCandidate().
  //
  // The unfortunate thing is that before the step 2 and 3...
  //   1.1. The mouse is captured by gtk_grab_add() inside the views framework.
  //   1.2. The render widget watches the grab via the callback function
  //        RenderWidgetHostViewGtkWidget::OnGrabNotify(), and, even though
  //        the candidate window itself does not steal focus (since it is a
  //        popup widget), the render widget explicitly regards the grab as
  //        a signal of focus-out and calls im_context_->OnFocusOut().
  //   1.3. It forces the input method to fully commit the composition.
  // Hence, the composition is committed before the user do any selection.
  //
  // The step 1.1 is somehow unavoidable, and the step 1.2 looks like an
  // intended behavior, though it is not pleasant for an in-process candidate
  // window (note that grab-notify is triggered only when a window in the
  // same application took a grab, which explains why we didn't see the issue
  // before r72934). So, for now, we give up the mouse-up selection and use
  // mouse-down selection, which doen't require grabbing.
  //
  // Moreover, there seems to be another issue when grabbing windows is hidden
  // http://crosbug.com/11422.
  // TODO(yusukes): investigate if we could fix Views so it always releases grab
  // when a popup window gets hidden. http://crosbug.com/11422

  gfx::Point location_in_candidate_window = event.location();
  views::View::ConvertPointToView(this, parent_candidate_window_,
                                  &location_in_candidate_window);
  parent_candidate_window_->OnCandidatePressed(location_in_candidate_window);
  parent_candidate_window_->CommitCandidate();
  return false;
}

void CandidateView::UpdateLabelBackgroundColors() {
  SkColor color = background() ?
      background()->get_color() : kDefaultBackgroundColor;
  if (orientation_ != InputMethodLookupTable::kVertical)
    shortcut_label_->SetBackgroundColor(color);
  candidate_label_->SetBackgroundColor(color);
  annotation_label_->SetBackgroundColor(color);
  if (infolist_label_) {
    infolist_label_->SetBackgroundColor(infolist_label_->background() ?
        infolist_label_->background()->get_color() : color);
  }
}

CandidateWindowView::CandidateWindowView(views::Widget* parent_frame)
    : selected_candidate_index_in_page_(0),
      parent_frame_(parent_frame),
      preedit_area_(NULL),
      header_area_(NULL),
      candidate_area_(NULL),
      footer_area_(NULL),
      previous_shortcut_column_width_(0),
      previous_candidate_column_width_(0),
      previous_annotation_column_width_(0) {
}

void CandidateWindowView::Init() {
  // Set the background and the border of the view.
  set_background(
      views::Background::CreateSolidBackground(kDefaultBackgroundColor));
  set_border(views::Border::CreateSolidBorder(1, kFrameColor));

  // Create areas.
  preedit_area_ = new InformationTextArea(views::Label::ALIGN_LEFT,
                                          kMinPreeditAreaWidth);
  header_area_ = new InformationTextArea(views::Label::ALIGN_LEFT, 0);
  candidate_area_ = new HidableArea;
  candidate_area_->SetContents(new views::View);
  footer_area_ = new InformationTextArea(views::Label::ALIGN_RIGHT, 0);

  // Set the window layout of the view
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);  // |this| owns |layout|.
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0, views::GridLayout::USE_PREF, 0, 0);

  // Add the preedit area
  layout->StartRow(0, 0);
  layout->AddView(preedit_area_);  // |preedit_area_| is owned by |this|.

  // Add the header area.
  layout->StartRow(0, 0);
  layout->AddView(header_area_);  // |header_area_| is owned by |this|.

  // Add the candidate area.
  layout->StartRow(0, 0);
  layout->AddView(candidate_area_);  // |candidate_area_| is owned by |this|.

  // Add the footer area.
  layout->StartRow(0, 0);
  layout->AddView(footer_area_);  // |footer_area_| is owned by |this|.
}

void CandidateWindowView::HideAll() {
  parent_frame_->Hide();
}

void CandidateWindowView::HideLookupTable() {
  candidate_area_->Hide();
  if (preedit_area_->IsShown())
    ResizeAndMoveParentFrame();
  else
    parent_frame_->Hide();
}

InformationTextArea* CandidateWindowView::GetAuxiliaryTextArea() {
  return (lookup_table_.orientation == InputMethodLookupTable::kHorizontal ?
          header_area_ : footer_area_);
}

void CandidateWindowView::HideAuxiliaryText() {
  GetAuxiliaryTextArea()->Hide();
  ResizeAndMoveParentFrame();
}

void CandidateWindowView::ShowAuxiliaryText() {
  GetAuxiliaryTextArea()->Show();
  ResizeAndMoveParentFrame();
}

void CandidateWindowView::UpdateAuxiliaryText(const std::string& utf8_text) {
  GetAuxiliaryTextArea()->SetText(utf8_text);
}

void CandidateWindowView::HidePreeditText() {
  preedit_area_->Hide();
  if (candidate_area_->IsShown())
    ResizeAndMoveParentFrame();
  else
    parent_frame_->Hide();
}

void CandidateWindowView::ShowPreeditText() {
  preedit_area_->Show();
  ResizeAndMoveParentFrame();
  parent_frame_->Show();
}

void CandidateWindowView::UpdatePreeditText(const std::string& utf8_text) {
  preedit_area_->SetText(utf8_text);
}

void CandidateWindowView::ShowLookupTable() {
  candidate_area_->Show();
  ResizeAndMoveParentFrame();
  parent_frame_->Show();
}

bool CandidateWindowView::ShouldUpdateCandidateViews(
    const InputMethodLookupTable& old_table,
    const InputMethodLookupTable& new_table) {
  // Check if most table contents are identical.
  if (old_table.page_size == new_table.page_size &&
      old_table.orientation == new_table.orientation &&
      old_table.candidates == new_table.candidates &&
      old_table.labels == new_table.labels &&
      old_table.annotations == new_table.annotations &&
      // Check if the page indexes are identical.
      ComputePageIndex(old_table) == ComputePageIndex(new_table)) {
    // If all of the conditions are met, we don't have to update candidate
    // views.
    return false;
  }
  return true;
}

void CandidateWindowView::UpdateCandidates(
    const InputMethodLookupTable& new_lookup_table) {
  const bool should_update = ShouldUpdateCandidateViews(lookup_table_,
                                                        new_lookup_table);
  // Updating the candidate views is expensive. We'll skip this if possible.
  if (should_update) {
    // Initialize candidate views if necessary.
    MaybeInitializeCandidateViews(new_lookup_table);

    // Compute the index of the current page.
    const int current_page_index = ComputePageIndex(new_lookup_table);
    if (current_page_index < 0) {
      LOG(ERROR) << "Invalid lookup_table: " << new_lookup_table.ToString();
      return;
    }

    // Update the candidates in the current page.
    const size_t start_from = current_page_index * new_lookup_table.page_size;

    // In some cases, engines send empty shortcut labels. For instance,
    // ibus-mozc sends empty labels when they show suggestions. In this
    // case, we should not show shortcut labels.
    const bool no_shortcut_mode =
        (start_from < new_lookup_table.labels.size() &&
         new_lookup_table.labels[start_from].empty());
    for (size_t i = 0; i < candidate_views_.size(); ++i) {
      const size_t index_in_page = i;
      const size_t candidate_index = start_from + index_in_page;
      CandidateView* candidate_view = candidate_views_[index_in_page];
      // Set the shortcut text.
      if (no_shortcut_mode) {
        candidate_view->SetShortcutText(string16());
      } else {
        // At this moment, we don't use labels sent from engines for UX
        // reasons. First, we want to show shortcut labels in empty rows
        // (ex. show 6, 7, 8, ... in empty rows when the number of
        // candidates is 5). Second, we want to add a period after each
        // shortcut label when the candidate window is horizontal.
        candidate_view->SetShortcutText(
            CreateShortcutText(i, new_lookup_table.orientation));
      }
      // Set the candidate text.
      if (candidate_index < new_lookup_table.candidates.size() &&
          candidate_index < new_lookup_table.annotations.size()) {
        candidate_view->SetCandidateText(
            UTF8ToUTF16(new_lookup_table.candidates[candidate_index]));
        candidate_view->SetAnnotationText(
            UTF8ToUTF16(new_lookup_table.annotations[candidate_index]));
        candidate_view->SetRowEnabled(true);

        if ((new_lookup_table.mozc_candidates.candidate_size() >
                 static_cast<int>(i)) &&
            (new_lookup_table.mozc_candidates.
                 candidate(i).has_information_id())) {
          candidate_view->SetInfolistIcon(true);
        } else {
          candidate_view->SetInfolistIcon(false);
        }
      } else {
        // Disable the empty row.
        candidate_view->SetCandidateText(string16());
        candidate_view->SetAnnotationText(string16());
        candidate_view->SetRowEnabled(false);
        candidate_view->SetInfolistIcon(false);
      }
    }
  }
  // Update the current lookup table. We'll use lookup_table_ from here.
  // Note that SelectCandidateAt() uses lookup_table_.
  lookup_table_ = new_lookup_table;

  // Select the current candidate in the page.
  const int current_candidate_in_page =
      lookup_table_.cursor_absolute_index % lookup_table_.page_size;
  SelectCandidateAt(current_candidate_in_page);
}

void CandidateWindowView::MaybeInitializeCandidateViews(
    const InputMethodLookupTable& lookup_table) {
  const InputMethodLookupTable::Orientation orientation =
      lookup_table.orientation;
  const int page_size = lookup_table.page_size;
  views::View* candidate_area_contents = candidate_area_->contents();

  // Current column width.
  int shortcut_column_width = 0;
  int candidate_column_width = 0;
  int annotation_column_width = 0;

  // If orientation is horizontal, don't need to compute width,
  // because each label is left aligned.
  if (orientation == InputMethodLookupTable::kVertical) {
    shortcut_column_width = ComputeShortcutColumnWidth(lookup_table);
    candidate_column_width = ComputeCandidateColumnWidth(lookup_table);
    annotation_column_width = ComputeAnnotationColumnWidth(lookup_table);
  }

  // If the requested number of views matches the number of current views, and
  // previous and current column width are same, just reuse these.
  //
  // Note that the early exit logic is not only useful for improving
  // performance, but also necessary for the horizontal candidate window
  // to be redrawn properly. If we get rid of the logic, the horizontal
  // candidate window won't get redrawn properly for some reason when
  // there is no size change. You can test this by removing "return" here
  // and type "ni" with Pinyin input method.
  if (static_cast<int>(candidate_views_.size()) == page_size &&
      lookup_table_.orientation == orientation &&
      previous_shortcut_column_width_ == shortcut_column_width &&
      previous_candidate_column_width_ == candidate_column_width &&
      previous_annotation_column_width_ == annotation_column_width) {
    return;
  }

  // Update the previous column widths.
  previous_shortcut_column_width_ = shortcut_column_width;
  previous_candidate_column_width_ = candidate_column_width;
  previous_annotation_column_width_ = annotation_column_width;

  // Clear the existing candidate_views if any.
  for (size_t i = 0; i < candidate_views_.size(); ++i) {
    candidate_area_contents->RemoveChildView(candidate_views_[i]);
    // Delete the view after getting out the current message loop iteration.
    MessageLoop::current()->DeleteSoon(FROM_HERE, candidate_views_[i]);
  }
  candidate_views_.clear();

  views::GridLayout* layout = new views::GridLayout(candidate_area_contents);
  // |candidate_area_contents| owns |layout|.
  candidate_area_contents->SetLayoutManager(layout);
  // Initialize the column set.
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  if (orientation == InputMethodLookupTable::kVertical) {
    column_set->AddColumn(views::GridLayout::FILL,
                          views::GridLayout::FILL,
                          0, views::GridLayout::USE_PREF, 0, 0);
  } else {
    for (int i = 0; i < page_size; ++i) {
      column_set->AddColumn(views::GridLayout::FILL,
                            views::GridLayout::FILL,
                            0, views::GridLayout::USE_PREF, 0, 0);
    }
  }

  // Set insets so the border of the selected candidate is drawn inside of
  // the border of the main candidate window, but we don't have the inset
  // at the top and the bottom as we have the borders of the header and
  // footer areas.
  const gfx::Insets kCandidateAreaInsets(0, 1, 0, 1);
  layout->SetInsets(kCandidateAreaInsets.top(),
                    kCandidateAreaInsets.left(),
                    kCandidateAreaInsets.bottom(),
                    kCandidateAreaInsets.right());

  // Add views to the candidate area.
  if (orientation == InputMethodLookupTable::kHorizontal) {
    layout->StartRow(0, 0);
  }

  for (int i = 0; i < page_size; ++i) {
    CandidateView* candidate_row = new CandidateView(this, i, orientation);
    candidate_row->Init(shortcut_column_width,
                        candidate_column_width,
                        annotation_column_width);
    candidate_views_.push_back(candidate_row);
    if (orientation == InputMethodLookupTable::kVertical) {
      layout->StartRow(0, 0);
    }
    // |candidate_row| will be owned by |candidate_area_contents|.
    layout->AddView(candidate_row);
  }

  // Compute views size in |layout|.
  // If we don't call this function, GetHorizontalOffset() often
  // returns invalid value (returns 0), then candidate window
  // moves right from the correct position in ResizeAndMoveParentFrame().
  // TODO(nhiroki): Figure out why it returns invalid value.
  // It seems that the x-position of the candidate labels is not set.
  layout->Layout(candidate_area_contents);
}

void CandidateWindowView::SelectCandidateAt(int index_in_page) {
  const int current_page_index = ComputePageIndex(lookup_table_);
  if (current_page_index < 0) {
    LOG(ERROR) << "Invalid lookup_table: " << lookup_table_.ToString();
    return;
  }

  const int cursor_absolute_index =
      lookup_table_.page_size * current_page_index + index_in_page;
  // Ignore click on out of range views.
  if (cursor_absolute_index < 0 ||
      cursor_absolute_index >=
      static_cast<int>(lookup_table_.candidates.size())) {
    return;
  }

  // Unselect the currently selected candidate.
  candidate_views_[selected_candidate_index_in_page_]->Unselect();
  // Remember the currently selected candidate index in the current page.
  selected_candidate_index_in_page_ = index_in_page;

  // Select the candidate specified by index_in_page.
  candidate_views_[index_in_page]->Select();

  // Update the cursor indexes in the model.
  lookup_table_.cursor_absolute_index = cursor_absolute_index;
}

void CandidateWindowView::OnCandidatePressed(
    const gfx::Point& location) {
  for (size_t i = 0; i < candidate_views_.size(); ++i) {
    gfx::Point converted_location = location;
    views::View::ConvertPointToView(this, candidate_views_[i],
                                    &converted_location);
    if (candidate_views_[i]->HitTest(converted_location)) {
      SelectCandidateAt(i);
      break;
    }
  }
}

void CandidateWindowView::CommitCandidate() {
  // For now, we don't distinguish left and right clicks.
  const int button = 1;  // Left button.
  const int key_modifilers = 0;
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnCandidateCommitted(selected_candidate_index_in_page_,
                                         button,
                                         key_modifilers));
}

void CandidateWindowView::ResizeAndMoveParentFrame() {
  const int x = cursor_location_.x();
  const int y = cursor_location_.y();
  const int height = cursor_location_.height();
  const int horizontal_offset = GetHorizontalOffset();

  gfx::Rect old_bounds = parent_frame_->GetClientAreaScreenBounds();
  gfx::Rect screen_bounds = gfx::Screen::GetMonitorWorkAreaNearestWindow(
      parent_frame_->GetNativeView());
  // The size.
  gfx::Rect frame_bounds = old_bounds;
  frame_bounds.set_size(GetPreferredSize());

  // The default position.
  frame_bounds.set_x(x + horizontal_offset);
  frame_bounds.set_y(y + height);

  // Handle overflow at the left and the top.
  frame_bounds.set_x(std::max(frame_bounds.x(), screen_bounds.x()));
  frame_bounds.set_y(std::max(frame_bounds.y(), screen_bounds.y()));

  // Handle overflow at the right.
  const int right_overflow = frame_bounds.right() - screen_bounds.right();
  if (right_overflow > 0) {
    frame_bounds.set_x(frame_bounds.x() - right_overflow);
  }

  // Handle overflow at the bottom.
  const int bottom_overflow = frame_bounds.bottom() - screen_bounds.bottom();
  if (bottom_overflow > 0) {
    frame_bounds.set_y(frame_bounds.y() - height - frame_bounds.height());
  }

  // Move the window per the cursor location.
  // SetBounds() is not cheap. Only call this when it is really changed.
  if (frame_bounds != old_bounds)
    parent_frame_->SetBounds(frame_bounds);
}

int CandidateWindowView::GetHorizontalOffset() {
  // Compute the horizontal offset if the lookup table is vertical.
  if (!candidate_views_.empty() &&
      lookup_table_.orientation == InputMethodLookupTable::kVertical) {
    return - candidate_views_[0]->GetCandidateLabelPosition().x();
  }
  return 0;
}

void CandidateWindowView::VisibilityChanged(View* starting_from,
                                            bool is_visible) {
  if (is_visible) {
    // If the visibility of candidate window is changed,
    // we should move the frame to the right position.
    ResizeAndMoveParentFrame();
  }
}

void CandidateWindowView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // If the bounds(size) of candidate window is changed,
  // we should move the frame to the right position.
  View::OnBoundsChanged(previous_bounds);
  ResizeAndMoveParentFrame();
}


InfolistView::InfolistView(
    InfolistWindowView* parent_infolist_window)
    : parent_infolist_window_(parent_infolist_window),
      title_label_(NULL),
      description_label_(NULL) {
}

void InfolistView::Init() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);  // |this| owns |layout|.
  title_label_ = new views::Label;
  title_label_->SetFont(
      title_label_->font().DeriveFont(kFontSizeDelta + 2));
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  views::View* wrapped_title_label =
      WrapWithPadding(title_label_, gfx::Insets(4, 7, 2, 4));

  description_label_ = new views::Label;
  description_label_->SetFont(
      description_label_->font().DeriveFont(kFontSizeDelta - 2));
  description_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  description_label_->SetMultiLine(true);
  views::View* wrapped_description_label =
      WrapWithPadding(description_label_, gfx::Insets(2, 17, 4, 4));

  // Initialize the column set with three columns.
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                         0, views::GridLayout::FIXED, 200, 0);

  layout->StartRow(0, 0);
  layout->AddView(wrapped_title_label);
  layout->StartRow(0, 0);
  layout->AddView(wrapped_description_label);
  UpdateLabelBackgroundColors();
}


void InfolistView::SetTitleText(const std::wstring& text) {
  title_label_->SetText(WideToUTF16Hack(text));
}

void InfolistView::SetDescriptionText(const std::wstring& text) {
  description_label_->SetText(WideToUTF16Hack(text));
}

void InfolistView::Select() {
  set_background(views::Background::CreateSolidBackground(
      kSelectedInfolistRowBackgroundColor));
  set_border(
      views::Border::CreateSolidBorder(1, kSelectedInfolistRowFrameColor));
  UpdateLabelBackgroundColors();
  // Need to call SchedulePaint() for background and border color changes.
  SchedulePaint();
}

void InfolistView::Unselect() {
  set_background(NULL);
  set_border(NULL);
  UpdateLabelBackgroundColors();
  SchedulePaint();  // See comments at Select().
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
      visible_(false) {
}

InfolistWindowView::~InfolistWindowView() {
  if (infolist_area_ != NULL) {
    infolist_area_->RemoveAllChildViews(false);
  }

  for (size_t i = 0; i < infolist_views_.size(); ++i) {
    delete infolist_views_[i];
  }
}

void InfolistWindowView::Init() {
  // Set the background and the border of the view.
  set_background(
      views::Background::CreateSolidBackground(kDefaultBackgroundColor));
  set_border(views::Border::CreateSolidBorder(1, kFrameColor));
  infolist_area_ = new views::View;

  // Set the window layout of the view
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);  // |this| owns |layout|.
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0, views::GridLayout::USE_PREF, 0, 0);
  layout->SetInsets(1, 1, 1, 1);

  // Add the infolist area
  layout->StartRow(0, 0);
  views::Label* caption_label = NULL;
  caption_label = new views::Label;
  caption_label->SetFont(caption_label->font().DeriveFont(kFontSizeDelta - 2));
  caption_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  caption_label->SetText(
      l10n_util::GetStringUTF16(IDS_INPUT_METHOD_INFOLIST_WINDOW_TITLE));
  views::View* wrapped_caption_label =
      WrapWithPadding(caption_label, gfx::Insets(2, 2, 2, 2));
  wrapped_caption_label->set_background(
      views::Background::CreateSolidBackground(kInfolistTitleBackgroundColor));
  caption_label->SetBackgroundColor(
      wrapped_caption_label->background()->get_color());
  layout->AddView(wrapped_caption_label);

  layout->StartRow(0, 0);
  layout->AddView(infolist_area_);  // |infolist_area_| is owned by |this|.
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
  if (visible_) {
    Show();
  } else {
    Hide();
  }
}

void InfolistWindowView::UpdateCandidates(
    const InputMethodLookupTable& new_lookup_table) {
  lookup_table_ = new_lookup_table;
  if (!lookup_table_.mozc_candidates.has_usages()) {
    return;
  }
  const mozc::commands::InformationList& usages =
      lookup_table_.mozc_candidates.usages();
  if (usages.information_size() <= 0) {
    return;
  }
  for (int i = infolist_views_.size(); i < usages.information_size(); ++i) {
    InfolistView* infolist_row = new InfolistView(this);
    infolist_row->Init();
    infolist_views_.push_back(infolist_row);
  }
  infolist_area_->RemoveAllChildViews(false);

  views::GridLayout* layout = new views::GridLayout(infolist_area_);
  // |infolist_area_| owns |layout|.
  infolist_area_->SetLayoutManager(layout);

  // Initialize the column set.
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        0, views::GridLayout::USE_PREF, 0, 0);
  layout->SetInsets(0, 0, 0, 0);

  for (int i = 0; i < usages.information_size(); ++i) {
    InfolistView* infolist_row = new InfolistView(this);
    infolist_row->Init();
    infolist_row->SetTitleText(
      UTF8ToWide(usages.information(i).title()));
    infolist_row->SetDescriptionText(
      UTF8ToWide(usages.information(i).description()));
    if (usages.has_focused_index() &&
        (static_cast<int>(usages.focused_index()) == i)) {
      infolist_row->Select();
    } else {
      infolist_row->Unselect();
    }
    infolist_views_.push_back(infolist_row);
    layout->StartRow(0, 0);
    layout->AddView(infolist_row);
  }

  layout->Layout(infolist_area_);
}

void InfolistWindowView::ResizeAndMoveParentFrame() {
  int x, y;
  gfx::Rect old_bounds = parent_frame_->GetClientAreaScreenBounds();
  gfx::Rect screen_bounds = gfx::Screen::GetMonitorWorkAreaNearestWindow(
      parent_frame_->GetNativeView());
  // The size.
  gfx::Rect frame_bounds = old_bounds;
  frame_bounds.set_size(GetPreferredSize());

  gfx::Rect candidatewindow_bounds;
  if (candidate_window_frame_ != NULL) {
    candidatewindow_bounds =
        candidate_window_frame_->GetClientAreaScreenBounds();
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


bool CandidateWindowController::Impl::Init() {
  // Create the candidate window view.
  CreateView();

  // The observer should be added before Connect() so we can capture the
  // initial connection change.
  ibus_ui_controller_->AddObserver(this);
  ibus_ui_controller_->Connect();
  return true;
}

void CandidateWindowController::Impl::CreateView() {
  // Create a non-decorated frame.
  frame_.reset(new views::Widget);
  // The size is initially zero.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  // Show the candidate window always on top
  params.keep_on_top = true;
  frame_->Init(params);

  // Create the candidate window.
  candidate_window_ = new CandidateWindowView(frame_.get());
  candidate_window_->Init();
  candidate_window_->AddObserver(this);

  frame_->SetContentsView(candidate_window_);


  // Create the infolist window.
  infolist_frame_.reset(new views::Widget);
  infolist_frame_->Init(params);
  infolist_window_ = new InfolistWindowView(
      infolist_frame_.get(), frame_.get());
  infolist_window_->Init();
  infolist_frame_->SetContentsView(infolist_window_);
}

CandidateWindowController::Impl::Impl()
    : ibus_ui_controller_(IBusUiController::Create()),
      candidate_window_(NULL),
      infolist_window_(NULL) {
}

CandidateWindowController::Impl::~Impl() {
  ibus_ui_controller_->RemoveObserver(this);
  candidate_window_->RemoveObserver(this);
  // ibus_ui_controller_'s destructor will close the connection.
}

void CandidateWindowController::Impl::OnHideAuxiliaryText() {
  candidate_window_->HideAuxiliaryText();
}

void CandidateWindowController::Impl::OnHideLookupTable() {
  candidate_window_->HideLookupTable();
  infolist_window_->Hide();
}

void CandidateWindowController::Impl::OnHidePreeditText() {
  candidate_window_->HidePreeditText();
}

void CandidateWindowController::Impl::OnSetCursorLocation(
    int x,
    int y,
    int width,
    int height) {
  // A workaround for http://crosbug.com/6460. We should ignore very short Y
  // move to prevent the window from shaking up and down.
  const int kKeepPositionThreshold = 2;  // px
  const gfx::Rect& last_location =
      candidate_window_->cursor_location();
  const int delta_y = abs(last_location.y() - y);
  if ((last_location.x() == x) && (delta_y <= kKeepPositionThreshold)) {
    DLOG(INFO) << "Ignored set_cursor_location signal to prevent window shake";
    return;
  }

  // Remember the cursor location.
  candidate_window_->set_cursor_location(
      gfx::Rect(x, y, width, height));
  // Move the window per the cursor location.
  candidate_window_->ResizeAndMoveParentFrame();
  infolist_window_->ResizeAndMoveParentFrame();
}

void CandidateWindowController::Impl::OnUpdateAuxiliaryText(
    const std::string& utf8_text,
    bool visible) {
  // If it's not visible, hide the auxiliary text and return.
  if (!visible) {
    candidate_window_->HideAuxiliaryText();
    return;
  }
  candidate_window_->UpdateAuxiliaryText(utf8_text);
  candidate_window_->ShowAuxiliaryText();
}

void CandidateWindowController::Impl::OnUpdateLookupTable(
    const InputMethodLookupTable& lookup_table) {
  // If it's not visible, hide the lookup table and return.
  if (!lookup_table.visible) {
    candidate_window_->HideLookupTable();
    infolist_window_->Hide();
    return;
  }

  candidate_window_->UpdateCandidates(lookup_table);
  candidate_window_->ShowLookupTable();

  const mozc::commands::Candidates& candidates = lookup_table.mozc_candidates;

  if (lookup_table.mozc_candidates.has_usages() &&
      lookup_table.mozc_candidates.usages().information_size() > 0) {
    infolist_window_->UpdateCandidates(lookup_table);
    infolist_window_->ResizeAndMoveParentFrame();
    if (candidates.has_focused_index() && candidates.candidate_size() > 0) {
      const int focused_row =
        candidates.focused_index() - candidates.candidate(0).index();
      if (candidates.candidate_size() >= focused_row &&
          candidates.candidate(focused_row).has_information_id()) {
        infolist_window_->DelayShow(kInfolistShowDelayMilliSeconds);
      } else {
        infolist_window_->DelayHide(kInfolistHideDelayMilliSeconds);
      }
    } else {
      infolist_window_->DelayHide(kInfolistHideDelayMilliSeconds);
    }
  } else {
    infolist_window_->Hide();
  }
}

void CandidateWindowController::Impl::OnUpdatePreeditText(
    const std::string& utf8_text, unsigned int cursor, bool visible) {
  // If it's not visible, hide the preedit text and return.
  if (!visible || utf8_text.empty()) {
    candidate_window_->HidePreeditText();
    return;
  }
  candidate_window_->UpdatePreeditText(utf8_text);
  candidate_window_->ShowPreeditText();
}

void CandidateWindowController::Impl::OnCandidateCommitted(int index,
                                                           int button,
                                                           int flags) {
  ibus_ui_controller_->NotifyCandidateClicked(index, button, flags);
}

void CandidateWindowController::Impl::OnConnectionChange(bool connected) {
  if (!connected) {
    candidate_window_->HideAll();
    infolist_window_->Hide();
  }
}

CandidateWindowController::CandidateWindowController()
    : impl_(new CandidateWindowController::Impl) {
}

CandidateWindowController::~CandidateWindowController() {
  delete impl_;
}

bool CandidateWindowController::Init() {
  return impl_->Init();
}

}  // namespace input_method
}  // namespace chromeos
