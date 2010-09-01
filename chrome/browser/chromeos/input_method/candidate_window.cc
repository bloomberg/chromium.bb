// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(satorux):
// - Implement a scroll bar or an indicator showing where you are in the
//   candidate window.

#include <algorithm>
#include <string>
#include <vector>

#include "app/app_paths.h"
#include "app/resource_bundle.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library_loader.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "gfx/canvas.h"
#include "gfx/font.h"
#include "cros/chromeos_cros_api.h"
#include "cros/chromeos_input_method_ui.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/event.h"
#include "views/fill_layout.h"
#include "views/focus/accelerator_handler.h"
#include "views/grid_layout.h"
#include "views/screen.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"
#include "views/widget/widget_gtk.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"
#include "views/window/window_delegate.h"

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

// The minimum width of candidate labels in the vertical candidate
// window. We use this value to prevent the candidate window from being
// too narrow when all candidates are short.
const int kMinCandidateLabelWidth = 100;
// The maximum width of candidate labels in the vertical candidate
// window. We use this value to prevent the candidate window from being
// too wide when one of candidates are long.
const int kMaxCandidateLabelWidth = 500;

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

}  // namespace

namespace chromeos {

class CandidateView;

// CandidateWindowView is the main container of the candidate window UI.
class CandidateWindowView : public views::View {
 public:
  // The object can be monitored by the observer.
  class Observer {
   public:
    virtual ~Observer() {}
    // The function is called when a candidate is committed.
    // See comments at NotifyCandidateClicke() in chromeos_input_method_ui.h for
    // details about the parameters.
    virtual void OnCandidateCommitted(int index, int button, int flag) = 0;
  };

  explicit CandidateWindowView(views::Widget* parent_frame);
  virtual ~CandidateWindowView() {}
  void Init();

  // Adds the given observer. The ownership is not transferred.
  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  // Removes the given observer.
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // Selects the candidate specified by the index in the current page
  // (zero-origin).  Changes the appearance of the selected candidate,
  // updates the information in the candidate window as needed.
  void SelectCandidateAt(int index_in_page);

  // The function is called when a candidate is being dragged. From the
  // given point, locates the candidate under the mouse cursor, and
  // selects it.
  void OnCandidateDragged(const gfx::Point& point);

  // Commits the candidate currently being selected.
  void CommitCandidate();

  // Hides the auxiliary text.
  void HideAuxiliaryText();

  // Shows the auxiliary text.
  void ShowAuxiliaryText();

  // Updates the auxiliary text.
  void UpdateAuxiliaryText(const std::string& utf8_text);

  // Updates candidates of the candidate window from |lookup_table|.
  // Candidates are arranged per |orientation|.
  void UpdateCandidates(const InputMethodLookupTable& lookup_table);

  // Resizes the parent frame and schedules painting. This needs to be
  // called when the visible contents of the candidate window are
  // modified.
  void ResizeAndSchedulePaint();

  // Returns the horizontal offset used for placing the vertical candidate
  // window so that the first candidate is aligned with the the text being
  // converted like:
  //
  //      XXX           <- The user is converting XXX
  //   +-----+
  //   |1 XXX|
  //   |2 YYY|
  //   |3 ZZZ|
  //
  // Returns 0 if no candidate is present.
  int GetHorizontalOffset();

 private:
  // Initializes the candidate views if needed.
  void MaybeInitializeCandidateViews(
      int num_views,
      InputMethodLookupTable::Orientation orientation);

  // Creates the footer area, where we show status information.
  // For instance, we show a cursor position like 2/19.
  views::View* CreateFooterArea();

  // Creates the header area, where we show auxiliary text.
  views::View* CreateHeaderArea();

  // The lookup table (candidates).
  InputMethodLookupTable lookup_table_;

  // Zero-origin index of the current page. If the cursor is on the first
  // page, the value will be 0.
  int current_page_index_;

  // The index in the current page of the candidate currently being selected.
  int selected_candidate_index_in_page_;

  // The observers of the object.
  ObserverList<Observer> observers_;

  // The parent frame.
  views::Widget* parent_frame_;

  // Views created in the class will be part of tree of |this|, so these
  // child views will be deleted when |this| is deleted.

  // The candidate area is where candidates are rendered.
  views::View* candidate_area_;
  // The footer area is where the auxiliary text is shown, if the
  // orientation is vertical. Usually the auxiliary text is used for
  // showing candidate number information like 2/19.
  views::View* footer_area_;
  // We use this when we show something in the footer area.
  scoped_ptr<views::View> footer_area_contents_;
  // We use this when we show nothing in the footer area.
  scoped_ptr<views::View> footer_area_place_holder_;
  // The header area is where the auxiliary text is shown, if the
  // orientation is horizontal. If the auxiliary text is not provided, we
  // show nothing.  For instance, we show pinyin text like "zhong'guo".
  views::View* header_area_;
  // We use this when we show something in the header area.
  scoped_ptr<views::View> header_area_contents_;
  // We use this when we show nothing in the header area.
  scoped_ptr<views::View> header_area_place_holder_;
  // The candidate views are used for rendering candidates.
  std::vector<CandidateView*> candidate_views_;
  // The header label is shown in the header area.
  views::Label* header_label_;
  // The footer label is shown in the footer area.
  views::Label* footer_label_;
};

// CandidateRow renderes a row of a candidate.
class CandidateView : public views::View {
 public:
  CandidateView(CandidateWindowView* parent_candidate_window,
                int index_in_page,
                InputMethodLookupTable::Orientation orientation);
  virtual ~CandidateView() {}
  void Init();

  // Sets candidate text to the given text.
  void SetCandidateText(const std::wstring& text);

  // Sets shortcut text to the given text.
  void SetShortcutText(const std::wstring& text);

  // Sets shortcut text from the given integer.
  void SetShortcutTextFromInt(int index);

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
  // View::OnMousePressed() implementation.
  virtual bool OnMousePressed(const views::MouseEvent& event);

  // View::OnMouseDragged() implementation.
  virtual bool OnMouseDragged(const views::MouseEvent& event);

  // View::OnMouseReleased() implementation.
  virtual void OnMouseReleased(const views::MouseEvent& event,
                               bool canceled);

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
};

// VerticalCandidateLabel is used for rendering candidate text in
// the vertical candidate window.
class VerticalCandidateLabel : public views::Label {
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
};

// CandidateWindowController controls the CandidateWindow.
class CandidateWindowController : public CandidateWindowView::Observer {
 public:
  CandidateWindowController();
  virtual ~CandidateWindowController();
  bool Init();

  // Returns the work area of the monitor nearest the candidate window.
  gfx::Rect GetMonitorWorkAreaNearestWindow();

  // Moves the candidate window per the the given cursor location, and the
  // horizontal offset.
  void MoveCandidateWindow(const gfx::Rect& cursor_location,
                           int horizontal_offset);

  // CandidateWindowView::Observer implementation.
  virtual void OnCandidateCommitted(int index,
                                    int button,
                                    int flags);

  const gfx::Rect& cursor_location() const { return cursor_location_; }
  void set_cursor_location(const gfx::Rect& cursor_location) {
    cursor_location_ = cursor_location;
  }

 private:
  // Creates the candidate window view.
  void CreateView();

  // The function is called when |HideAuxiliaryText| signal is received in
  // libcros. |input_method_library| is a void pointer to this object.
  static void OnHideAuxiliaryText(void* input_method_library);

  // The function is called when |HideLookupTable| signal is received in
  // libcros. |input_method_library| is a void pointer to this object.
  static void OnHideLookupTable(void* input_method_library);

  // The function is called when |SetCursorLocation| signal is received
  // in libcros. |input_method_library| is a void pointer to this object.
  static void OnSetCursorLocation(void* input_method_library,
                                  int x,
                                  int y,
                                  int width,
                                  int height);

  // The function is called when |UpdateAuxiliaryText| signal is received
  // in libcros. |input_method_library| is a void pointer to this object.
  static void OnUpdateAuxiliaryText(void* input_method_library,
                                    const std::string& utf8_text,
                                    bool visible);

  // The function is called when |UpdateLookupTable| signal is received
  // in libcros. |input_method_library| is a void pointer to this object.
  static void OnUpdateLookupTable(void* input_method_library,
                                  const InputMethodLookupTable& lookup_table);

  // The connection is used for communicating with input method UI logic
  // in libcros.
  InputMethodUiStatusConnection* ui_status_connection_;

  // The candidate window view.
  CandidateWindowView* candidate_window_;

  // This is the outer frame of the candidate window view. The frame will
  // own |candidate_window_|.
  scoped_ptr<views::Widget> frame_;

  // The last cursor location received in OnSetCursorLocation().
  gfx::Rect cursor_location_;
};

CandidateView::CandidateView(
    CandidateWindowView* parent_candidate_window,
    int index_in_page,
    InputMethodLookupTable::Orientation orientation)
    : index_in_page_(index_in_page),
      orientation_(orientation),
      parent_candidate_window_(parent_candidate_window),
      shortcut_label_(NULL),
      candidate_label_(NULL) {
}

void CandidateView::Init() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);  // |this| owns |layout|.

  // Create the shortcut label. The label will eventually be part of the
  // tree of |this| via |wrapped_shortcut_label|, hence it's deleted when
  // |this| is deleted.
  shortcut_label_ = new views::Label();

  // Wrap it with padding.
  const gfx::Insets kVerticalShortcutLabelInsets(1, 6, 1, 6);
  const gfx::Insets kHorizontalShortcutLabelInsets(1, 3, 1, 0);
  const gfx::Insets insets =
      (orientation_ == InputMethodLookupTable::kVertical ?
       kVerticalShortcutLabelInsets :
       kHorizontalShortcutLabelInsets);
  views::View* wrapped_shortcut_label =
      WrapWithPadding(shortcut_label_, insets);
  // We'll use a bigger font size, so Chinese characters are more readable
  // in the candidate window.
  const int kFontSizeDelta = 2;  // Two size bigger.
  // Make the font bold, and change the size.
  if (orientation_ == InputMethodLookupTable::kVertical) {
    shortcut_label_->SetFont(
        shortcut_label_->font().DeriveFont(kFontSizeDelta, gfx::Font::BOLD));
  } else {
    shortcut_label_->SetFont(
        shortcut_label_->font().DeriveFont(kFontSizeDelta));
  }
  // TODO(satorux): Maybe we need to use language specific fonts for
  // candidate_label, like Chinese font for Chinese input method?

  // Add decoration based on the orientation.
  if (orientation_ == InputMethodLookupTable::kVertical) {
    // Set the background color.
    wrapped_shortcut_label->set_background(
        views::Background::CreateSolidBackground(
            kShortcutBackgroundColor));
  }
  shortcut_label_->SetColor(kShortcutColor);

  // Create the candidate label. The label will be added to |this| as a
  // child view, hence it's deleted when |this| is deleted.
  if (orientation_ == InputMethodLookupTable::kVertical) {
    candidate_label_ = new VerticalCandidateLabel;
  } else {
    candidate_label_ = new views::Label;
  }
  // Change the font size.
  candidate_label_->SetFont(
      candidate_label_->font().DeriveFont(kFontSizeDelta));

  candidate_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  // Initialize the column set with two columns.
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0, views::GridLayout::USE_PREF, 0, 0);
  if (orientation_ == InputMethodLookupTable::kVertical) {
    column_set->AddPaddingColumn(0, 4);
  }
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0, views::GridLayout::USE_PREF, 0, 0);
  if (orientation_ == InputMethodLookupTable::kVertical) {
    column_set->AddPaddingColumn(0, 4);
  } else {
    column_set->AddPaddingColumn(0, 6);
  }

  // Add the shortcut label and the candidate label.
  layout->StartRow(0, 0);
  // |wrapped_shortcut_label| and |candidate_label_| will be owned by |this|.
  layout->AddView(wrapped_shortcut_label);
  layout->AddView(candidate_label_);
}

void CandidateView::SetCandidateText(const std::wstring& text) {
  candidate_label_->SetText(text);
}

void CandidateView::SetShortcutText(const std::wstring& text) {
  shortcut_label_->SetText(text);
}

void CandidateView::SetShortcutTextFromInt(int index) {
  // Choose the character used for the shortcut label.
  const wchar_t kShortcutCharacters[] = L"1234567890ABCDEF";
  // The default character should not be used but just in case.
  wchar_t shortcut_character = L'?';
  if (index < static_cast<int>(arraysize(kShortcutCharacters) - 1)) {
    shortcut_character = kShortcutCharacters[index];
  }
  if (orientation_ == InputMethodLookupTable::kVertical) {
    shortcut_label_->SetText(StringPrintf(L"%lc", shortcut_character));
  } else {
    shortcut_label_->SetText(StringPrintf(L"%lc.", shortcut_character));
  }
}

void CandidateView::Select() {
  set_background(
      views::Background::CreateSolidBackground(kSelectedRowBackgroundColor));
  set_border(views::Border::CreateSolidBorder(1, kSelectedRowFrameColor));
}

void CandidateView::Unselect() {
  set_background(NULL);
  set_border(NULL);
}

void CandidateView::SetRowEnabled(bool enabled) {
  shortcut_label_->SetColor(
      enabled ? kShortcutColor : kDisabledShortcutColor);
}

gfx::Point CandidateView::GetCandidateLabelPosition() const {
  return candidate_label_->GetPosition();
}

bool CandidateView::OnMousePressed(const views::MouseEvent& event) {
  // Select the candidate. We'll commit the candidate when the mouse
  // button is released.
  parent_candidate_window_->SelectCandidateAt(index_in_page_);
  // Request MouseDraggged and MouseReleased events.
  return true;
}

bool CandidateView::OnMouseDragged(const views::MouseEvent& event) {
  gfx::Point location_in_candidate_window = event.location();
  views::View::ConvertPointToView(this, parent_candidate_window_,
                                  &location_in_candidate_window);
  // Notify the candidate window that a candidate is now being dragged.
  parent_candidate_window_->OnCandidateDragged(location_in_candidate_window);
  // Request MouseReleased event.
  return true;
}

void CandidateView::OnMouseReleased(const views::MouseEvent& event,
                                       bool canceled) {
  // Commit the current candidate unless it's canceled.
  if (!canceled) {
    parent_candidate_window_->CommitCandidate();
  }
}

CandidateWindowView::CandidateWindowView(
    views::Widget* parent_frame)
    : current_page_index_(0),
      selected_candidate_index_in_page_(0),
      parent_frame_(parent_frame),
      candidate_area_(NULL),
      footer_area_(NULL),
      header_area_(NULL),
      header_label_(NULL),
      footer_label_(NULL) {
}

void CandidateWindowView::Init() {
  // Set the background and the border of the view.
  set_background(
      views::Background::CreateSolidBackground(kDefaultBackgroundColor));
  set_border(views::Border::CreateSolidBorder(1, kFrameColor));

  // Create the header area.
  header_area_ = CreateHeaderArea();
  // Create the candidate area.
  candidate_area_ = new views::View;
  // Create the footer area.
  footer_area_ = CreateFooterArea();

  // Set the window layout of the view
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);  // |this| owns layout|.
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0, views::GridLayout::USE_PREF, 0, 0);

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

void CandidateWindowView::HideAuxiliaryText() {
  views::View* target_area = (
      lookup_table_.orientation == InputMethodLookupTable::kHorizontal ?
      header_area_ : footer_area_);
  views::View* target_place_holder = (
      lookup_table_.orientation == InputMethodLookupTable::kHorizontal ?
      header_area_place_holder_.get() :
      footer_area_place_holder_.get());
  // Put the place holder to the target display area.
  target_area->RemoveAllChildViews(false);  // Don't delete child views.
  target_area->AddChildView(target_place_holder);
  ResizeAndSchedulePaint();
}

void CandidateWindowView::ShowAuxiliaryText() {
  views::View* target_area = (
      lookup_table_.orientation == InputMethodLookupTable::kHorizontal ?
      header_area_ : footer_area_);
  views::View* target_contents = (
      lookup_table_.orientation == InputMethodLookupTable::kHorizontal ?
      header_area_contents_.get() :
      footer_area_contents_.get());
  // Put contents to the target display area.
  target_area->RemoveAllChildViews(false);  // Don't delete child views.
  target_area->AddChildView(target_contents);
  ResizeAndSchedulePaint();
}

void CandidateWindowView::UpdateAuxiliaryText(const std::string& utf8_text) {
  views::Label* target_label = (
      lookup_table_.orientation == InputMethodLookupTable::kHorizontal ?
      header_label_ : footer_label_);
  target_label->SetText(UTF8ToWide(utf8_text));
}

void CandidateWindowView::UpdateCandidates(
    const InputMethodLookupTable& lookup_table) {
  // Initialize candidate views if necessary.
  MaybeInitializeCandidateViews(lookup_table.page_size,
                                lookup_table.orientation);
  lookup_table_ = lookup_table;

  // Compute the index of the current page.
  current_page_index_ =
      lookup_table.cursor_absolute_index / lookup_table.page_size;

  // Update the candidates in the current page.
  const size_t start_from = current_page_index_ * lookup_table.page_size;

  // In some cases, engines send empty shortcut labels. For instance,
  // ibus-mozc sends empty labels when they show suggestions. In this
  // case, we should not show shortcut labels.
  const bool no_shortcut_mode = (start_from < lookup_table_.labels.size() &&
                                 lookup_table_.labels[start_from] == "");
  for (size_t i = 0; i < candidate_views_.size(); ++i) {
    const size_t index_in_page = i;
    const size_t candidate_index = start_from + index_in_page;
    CandidateView* candidate_view = candidate_views_[index_in_page];
    // Set the shortcut text.
    if (no_shortcut_mode) {
      candidate_view->SetShortcutText(L"");
    } else {
      // At this moment, we don't use labels sent from engines for UX
      // reasons. First, we want to show shortcut labels in empty rows
      // (ex. show 6, 7, 8, ... in empty rows when the number of
      // candidates is 5). Second, we want to add a period after each
      // shortcut label when the candidate window is horizontal.
      candidate_view->SetShortcutTextFromInt(i);
    }
    // Set the candidate text.
    if (candidate_index < lookup_table_.candidates.size()) {
      candidate_view->SetCandidateText(
          UTF8ToWide(lookup_table_.candidates[candidate_index]));
      candidate_view->SetRowEnabled(true);
    } else {
      // Disable the empty row.
      candidate_view->SetCandidateText(L"");
      candidate_view->SetRowEnabled(false);
    }
  }

  // Select the first candidate candidate in the page.
  const int first_candidate_in_page =
      lookup_table.cursor_absolute_index % lookup_table.page_size;
  SelectCandidateAt(first_candidate_in_page);
}

void CandidateWindowView::MaybeInitializeCandidateViews(
    int num_views,
    InputMethodLookupTable::Orientation orientation) {
  // If the requested number of views matches the number of current views,
  // just reuse these.
  if (num_views == static_cast<int>(candidate_views_.size()) &&
      orientation == lookup_table_.orientation) {
    return;
  }

  // Clear the existing candidate_views if any.
  for (size_t i = 0; i < candidate_views_.size(); ++i) {
    candidate_area_->RemoveChildView(candidate_views_[i]);
  }
  candidate_views_.clear();

  views::GridLayout* layout = new views::GridLayout(candidate_area_);
  // |candidate_area_| owns |layout|.
  candidate_area_->SetLayoutManager(layout);
  // Initialize the column set.
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  if (orientation == InputMethodLookupTable::kVertical) {
    column_set->AddColumn(views::GridLayout::FILL,
                          views::GridLayout::FILL,
                          0, views::GridLayout::USE_PREF, 0, 0);
  } else {
    for (int i = 0; i < num_views; ++i) {
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
  for (int i = 0; i < num_views; ++i) {
    CandidateView* candidate_row = new CandidateView(this, i, orientation);
    candidate_row->Init();
    candidate_views_.push_back(candidate_row);
    if (orientation == InputMethodLookupTable::kVertical) {
      layout->StartRow(0, 0);
    }
    // |candidate_row| will be owned by candidate_area_|.
    layout->AddView(candidate_row);
  }
}

views::View* CandidateWindowView::CreateHeaderArea() {
  // |header_area_place_holder_| will not be owned by another view.
  // This will be deleted by scoped_ptr.
  //
  // This is because we swap the contents of |header_area_| between
  // |header_area_place_holder_| (to show nothing) and
  // |header_area_contents_| (to show something). In other words,
  // |header_area_| only contains one of the two views hence cannot own
  // the two views at the same time.
  header_area_place_holder_.reset(new views::View);
  header_area_place_holder_->set_parent_owned(false);  // Won't be owened.

  // |header_label_| will be owned by |header_area_contents_|.
  header_label_ = new views::Label;
  header_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  const gfx::Insets kHeaderInsets(2, 2, 2, 4);
  // |header_area_contents_| will not be owned by another view.
  // See a comment at |header_area_place_holder_| for why.
  header_area_contents_.reset(
      WrapWithPadding(header_label_, kHeaderInsets));
  header_area_contents_->set_parent_owned(false);  // Won't be owened.
  header_area_contents_->set_border(
      views::Border::CreateSolidBorder(1, kFrameColor));
  header_area_contents_->set_background(
      views::Background::CreateVerticalGradientBackground(
          kFooterTopColor,
          kFooterBottomColor));

  views::View* header_area = new views::View;
  header_area->SetLayoutManager(new views::FillLayout);
  // Initialize the header area with the place holder (i.e. show nothing).
  header_area->AddChildView(header_area_place_holder_.get());
  return header_area;
}

views::View* CandidateWindowView::CreateFooterArea() {
  // |footer_area_place_holder_| will not be owned by another view.
  // See also the comment about |header_area_place_holder_| in
  // CreateHeaderArea().
  footer_area_place_holder_.reset(new views::View);
  footer_area_place_holder_->set_parent_owned(false);  // Won't be owened.

  footer_label_ = new views::Label();
  footer_label_->SetHorizontalAlignment(views::Label::ALIGN_RIGHT);

  const gfx::Insets kFooterInsets(2, 2, 2, 4);
  footer_area_contents_.reset(
      WrapWithPadding(footer_label_, kFooterInsets));
  footer_area_contents_->set_parent_owned(false);  // Won't be owened.
  footer_area_contents_->set_border(
      views::Border::CreateSolidBorder(1, kFrameColor));
  footer_area_contents_->set_background(
      views::Background::CreateVerticalGradientBackground(
          kFooterTopColor,
          kFooterBottomColor));

  views::View* footer_area = new views::View;
  footer_area->SetLayoutManager(new views::FillLayout);
  // Initialize the footer area with the place holder (i.e. show nothing).
  footer_area->AddChildView(footer_area_place_holder_.get());
  return footer_area;
}

void CandidateWindowView::SelectCandidateAt(int index_in_page) {
  int cursor_absolute_index =
      lookup_table_.page_size * current_page_index_ + index_in_page;
  // Ignore click on out of range views.
  if (cursor_absolute_index < 0 ||
      cursor_absolute_index >=
      static_cast<int>(lookup_table_.candidates.size())) {
    return;
  }

  // Remember the currently selected candidate index in the current page.
  selected_candidate_index_in_page_ = index_in_page;

  // Unselect all the candidate first. Theoretically, we could remember
  // the lastly selected candidate and only unselect it, but unselecting
  // everything is simpler.
  for (size_t i = 0; i < candidate_views_.size(); ++i) {
    candidate_views_[i]->Unselect();
  }
  // Select the candidate specified by index_in_page.
  candidate_views_[index_in_page]->Select();

  // Update the cursor indexes in the model.
  lookup_table_.cursor_absolute_index = cursor_absolute_index;

  ResizeAndSchedulePaint();
}

void CandidateWindowView::OnCandidateDragged(
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

void CandidateWindowView::ResizeAndSchedulePaint() {
  // Resize the parent frame, with the current candidate window size.
  gfx::Size size = GetPreferredSize();
  gfx::Rect bounds;
  parent_frame_->GetBounds(&bounds, false);
  bounds.set_width(size.width());
  bounds.set_height(size.height());
  parent_frame_->SetBounds(bounds);

  SchedulePaint();
}

int CandidateWindowView::GetHorizontalOffset() {
  // Compute the horizontal offset if the lookup table is vertical.
  if (!candidate_views_.empty() &&
      lookup_table_.orientation == InputMethodLookupTable::kVertical) {
    return - candidate_views_[0]->GetCandidateLabelPosition().x();
  }
  return 0;
}


bool CandidateWindowController::Init() {
  // Initialize the input method UI status connection.
  InputMethodUiStatusMonitorFunctions functions;
  functions.hide_auxiliary_text =
      &CandidateWindowController::OnHideAuxiliaryText;
  functions.hide_lookup_table =
      &CandidateWindowController::OnHideLookupTable;
  functions.set_cursor_location =
      &CandidateWindowController::OnSetCursorLocation;
  functions.update_auxiliary_text =
      &CandidateWindowController::OnUpdateAuxiliaryText;
  functions.update_lookup_table =
      &CandidateWindowController::OnUpdateLookupTable;
  ui_status_connection_ = MonitorInputMethodUiStatus(functions, this);
  if (!ui_status_connection_) {
    LOG(ERROR) << "MonitorInputMethodUiStatus() failed.";
    return false;
  }

  // Create the candidate window view.
  CreateView();

  return true;
}

void CandidateWindowController::CreateView() {
  // Create a non-decorated frame.
  frame_.reset(views::Widget::CreatePopupWidget(
      views::Widget::NotTransparent,
      views::Widget::AcceptEvents,
      views::Widget::DeleteOnDestroy,
      views::Widget::MirrorOriginInRTL));
  // The size is initially zero.
  frame_->Init(NULL, gfx::Rect(0, 0));

  // Create the candidate window.
  candidate_window_ = new CandidateWindowView(frame_.get());
  candidate_window_->Init();
  candidate_window_->AddObserver(this);

  // Put the candidate window view on the frame.  The frame is resized
  // later when the candidate window is shown.
  views::RootView* root_view = frame_->GetRootView();
  // |root_view| owns the |candidate_window_|, thus |frame_| effectively
  // owns |candidate_window_|.
  root_view->SetContentsView(candidate_window_);
}

CandidateWindowController::CandidateWindowController()
    : ui_status_connection_(NULL),
      frame_(NULL) {
}

CandidateWindowController::~CandidateWindowController() {
  candidate_window_->RemoveObserver(this);
  chromeos::DisconnectInputMethodUiStatus(ui_status_connection_);
}

gfx::Rect CandidateWindowController::GetMonitorWorkAreaNearestWindow() {
  return views::Screen::GetMonitorWorkAreaNearestWindow(
      frame_->GetNativeView());
}

void CandidateWindowController::MoveCandidateWindow(
    const gfx::Rect& cursor_location,
    int horizontal_offset) {
  const int x = cursor_location.x();
  const int y = cursor_location.y();
  const int height = cursor_location.height();

  gfx::Rect frame_bounds;
  frame_->GetBounds(&frame_bounds, false);

  gfx::Rect screen_bounds = GetMonitorWorkAreaNearestWindow();

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
  frame_->SetBounds(frame_bounds);
}

void CandidateWindowController::OnHideAuxiliaryText(
    void* input_method_library) {
  CandidateWindowController* controller =
      static_cast<CandidateWindowController*>(input_method_library);

  controller->candidate_window_->HideAuxiliaryText();
}

void CandidateWindowController::OnHideLookupTable(
    void* input_method_library) {
  CandidateWindowController* controller =
      static_cast<CandidateWindowController*>(input_method_library);

  controller->frame_->Hide();
}

void CandidateWindowController::OnSetCursorLocation(
    void* input_method_library,
    int x,
    int y,
    int width,
    int height) {
  CandidateWindowController* controller =
      static_cast<CandidateWindowController*>(input_method_library);

  // Remember the cursor location.
  controller->set_cursor_location(gfx::Rect(x, y, width, height));
  // Move the window per the cursor location.
  controller->MoveCandidateWindow(
      controller->cursor_location(),
      controller->candidate_window_->GetHorizontalOffset());
  // The call is needed to ensure that the candidate window is redrawed
  // properly after the cursor location is changed.
  controller->candidate_window_->ResizeAndSchedulePaint();
}

void CandidateWindowController::OnUpdateAuxiliaryText(
    void* input_method_library,
    const std::string& utf8_text,
    bool visible) {
  CandidateWindowController* controller =
      static_cast<CandidateWindowController*>(input_method_library);
  // If it's not visible, hide the auxiliary text and return.
  if (!visible) {
    controller->candidate_window_->HideAuxiliaryText();
    return;
  }
  controller->candidate_window_->UpdateAuxiliaryText(utf8_text);
  controller->candidate_window_->ShowAuxiliaryText();
}

void CandidateWindowController::OnUpdateLookupTable(
    void* input_method_library,
    const InputMethodLookupTable& lookup_table) {
  CandidateWindowController* controller =
      static_cast<CandidateWindowController*>(input_method_library);

  // If it's not visible, hide the window and return.
  if (!lookup_table.visible) {
    controller->frame_->Hide();
    return;
  }

  controller->candidate_window_->UpdateCandidates(lookup_table);
  controller->frame_->Show();
  // We should call MoveCandidateWindow() after
  // controller->frame_->Show(), as GetHorizontalOffset() returns a valid
  // value only after the Show() method is called.
  controller->MoveCandidateWindow(
      controller->cursor_location(),
      controller->candidate_window_->GetHorizontalOffset());
}

void CandidateWindowController::OnCandidateCommitted(int index,
                                                     int button,
                                                     int flags) {
  NotifyCandidateClicked(ui_status_connection_, index, button, flags);
}

}  // namespace chromeos

int main(int argc, char** argv) {
  // Initialize gtk stuff.
  g_thread_init(NULL);
  g_type_init();
  gtk_init(&argc, &argv);

  // Initialize Chrome stuff.
  base::AtExitManager exit_manager;
  base::EnableTerminationOnHeapCorruption();
  app::RegisterPathProvider();
  CommandLine::Init(argc, argv);
  // TODO(markusheintz): The command line switch --Lang is now processed
  // by the CommandLinePrefStore and mapped to the preference
  // prefs::kApplicationLocale. This preferences can be read through the
  // PrefService. l10n_util::GetApplicationLocale() which is called by the
  // ResourceBundle code now ignores the --Lang flag.
  // In order to support the --Lang flag here the preference
  // prefs::kApplicationLocale must be read and passed instead of L"en-US".
  ResourceBundle::InitSharedInstance("en-US");

  // Write logs to a file for debugging, if --logtofile=FILE_NAME is given.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string log_file_name =
      command_line.GetSwitchValueASCII(switches::kChromeosLogToFile);
  if (!log_file_name.empty()) {
    logging::SetMinLogLevel(logging::LOG_INFO);
    logging::InitLogging(log_file_name.c_str(),
                         logging::LOG_ONLY_TO_FILE,
                         logging::DONT_LOCK_LOG_FILE,
                         logging::DELETE_OLD_LOG_FILE);
    // Redirect stderr to log_file_name. This is neeed to capture the
    // logging from libcros.so.
    if (!freopen(log_file_name.c_str(), "a", stderr)) {
      LOG(INFO) << "Failed to redirect stderr to " << log_file_name.c_str();
    }
  }

  // Load libcros.
  chrome::RegisterPathProvider();  // for libcros.so.
  chromeos::CrosLibraryLoader lib_loader;
  std::string error_string;
  CHECK(lib_loader.Load(&error_string))
      << "Failed to load libcros, " << error_string;

  // Create the main message loop.
  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  // Create the candidate window controller.
  chromeos::CandidateWindowController controller;
  if (!controller.Init()) {
    return 1;
  }

  // Start the main loop.
  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->Run(&accelerator_handler);

  return 0;
}
