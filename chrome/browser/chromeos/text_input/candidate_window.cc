// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
#include "base/file_path.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library_loader.h"
#include "chrome/common/chrome_paths.h"
#include "gfx/canvas.h"
#include "gfx/font.h"
#include "third_party/cros/chromeos_cros_api.h"
#include "third_party/cros/chromeos_ime.h"
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
  // Should we show candidates vertically or horizontally?
  enum Orientation {
    kVertical,
    kHorizontal,
  };

  // The object can be monitored by the observer.
  class Observer {
   public:
    virtual ~Observer() {}
    // The function is called when a candidate is committed.
    // See comments at NotifyCandidateClicke() in chromeos_ime.h for
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
  void UpdateCandidates(const ImeLookupTable& lookup_table);

  // Resizes the parent frame and schedules painting. This needs to be
  // called when the visible contents of the candidate window are
  // modified.
  void ResizeAndSchedulePaint();

 private:
  // Initializes the candidate views if needed.
  void MaybeInitializeCandidateViews(int num_views,
                                     Orientation orientation);

  // Creates the footer area, where we show status information.
  // For instance, we show a cursor position like 2/19.
  views::View* CreateFooterArea();

  // Creates the header area, where we show auxiliary text.
  views::View* CreateHeaderArea();

  // The orientation of the candidate window.
  Orientation orientation_;

  // The lookup table (candidates).
  ImeLookupTable lookup_table_;

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
  // The footer area is where some status messages are shown if needed.
  views::View* footer_area_;
  // We use this when we show something in the footer area.
  scoped_ptr<views::View> footer_area_contents_;
  // We use this when we show nothing in the footer area.
  scoped_ptr<views::View> footer_area_place_holder_;
  // The header area is where the auxiliary text is shown, if the
  // auxiliary text is provided. If it is not provided, we show nothing.
  // For instance, we show pinyin text like "zhong'guo", but we show
  // nothing with Japanese IME.
  views::View* header_area_;
  // We use this when we show something in the header area.
  scoped_ptr<views::View> header_area_contents_;
  // We use this when we show nothing in the header area.
  scoped_ptr<views::View> header_area_place_holder_;
  // The candidate views are used for rendering candidates.
  std::vector<CandidateView*> candidate_views_;
  // The auxiliary text label is shown in the auxiliary text area.
  views::Label* auxiliary_text_label_;
  // The footer label is shown in the footer area.
  views::Label* footer_label_;
};

// CandidateRow renderes a row of a candidate.
class CandidateView : public views::View {
 public:
  CandidateView(CandidateWindowView* parent_candidate_window,
                int index_in_page,
                CandidateWindowView::Orientation orientation);
  virtual ~CandidateView() {}
  void Init();

  // Sets candidate text with the given text.
  void SetCandidateText(const std::wstring& text);

  // Selects the candidate row. Changes the appearance to make it look
  // like a selected candidate.
  void Select();

  // Unselects the candidate row. Changes the appearance to make it look
  // like an unselected candidate.
  void Unselect();

  // Disables the candidate row. Changes the appearance to make it look
  // like unclickable area.
  void Disable();

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
  CandidateWindowView::Orientation orientation_;

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
    return size;
  }
};

// CandidateWindowController controls the CandidateWindow.
class CandidateWindowController : public CandidateWindowView::Observer {
 public:
  CandidateWindowController();
  virtual ~CandidateWindowController();
  void Init();

  // Returns the work area of the monitor nearest the candidate window.
  gfx::Rect GetMonitorWorkAreaNearestWindow();

  // CandidateWindowView::Observer implementation.
  virtual void OnCandidateCommitted(int index,
                                    int button,
                                    int flags);

 private:
  // Creates the candidate window view.
  void CreateView();

  // The function is called when |HideAuxiliaryText| signal is received in
  // libcros. |ime_library| is a void pointer to this object.
  static void OnHideAuxiliaryText(void* ime_library);

  // The function is called when |HideLookupTable| signal is received in
  // libcros. |ime_library| is a void pointer to this object.
  static void OnHideLookupTable(void* ime_library);

  // The function is called when |SetCursorLocation| signal is received
  // in libcros. |ime_library| is a void pointer to this object.
  static void OnSetCursorLocation(void* ime_library,
                                  int x,
                                  int y,
                                  int width,
                                  int height);

  // The function is called when |UpdateAuxiliaryText| signal is received
  // in libcros. |ime_library| is a void pointer to this object.
  static void OnUpdateAuxiliaryText(void* ime_library,
                                    const std::string& utf8_text,
                                    bool visible);

  // The function is called when |UpdateLookupTable| signal is received
  // in libcros. |ime_library| is a void pointer to this object.
  static void OnUpdateLookupTable(void* ime_library,
                                  const ImeLookupTable& lookup_table);

  // The connection is used for communicating with IME code in libcros.
  ImeStatusConnection* ime_status_connection_;

  // The candidate window view.
  CandidateWindowView* candidate_window_;

  // This is the outer frame of the candidate window view. The frame will
  // own |candidate_window_|.
  scoped_ptr<views::Widget> frame_;
};

CandidateView::CandidateView(
    CandidateWindowView* parent_candidate_window,
    int index_in_page,
    CandidateWindowView::Orientation orientation)
    : index_in_page_(index_in_page),
      orientation_(orientation),
      parent_candidate_window_(parent_candidate_window),
      shortcut_label_(NULL),
      candidate_label_(NULL) {
}

void CandidateView::Init() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);  // |this| owns |layout|.

  // Choose the character used for the shortcut label.
  const wchar_t kShortcutCharacters[] = L"1234567890ABCDEF";
  // The default character should not be used but just in case.
  wchar_t shortcut_character = L'?';
  if (index_in_page_ < static_cast<int>(arraysize(kShortcutCharacters) - 1)) {
    shortcut_character = kShortcutCharacters[index_in_page_];
  }

  // Create the shortcut label. The label will eventually be part of the
  // tree of |this| via |wrapped_shortcut_label|, hence it's deleted when
  // |this| is deleted.
  shortcut_label_ = new views::Label(
      StringPrintf(L"%lc",  shortcut_character));

  // Wrap it with padding.
  const gfx::Insets kVerticalShortcutLabelInsets(1, 6, 1, 6);
  const gfx::Insets kHorizontalShortcutLabelInsets(1, 1, 1, 1);
  const gfx::Insets insets = (orientation_ == CandidateWindowView::kVertical ?
                              kVerticalShortcutLabelInsets :
                              kHorizontalShortcutLabelInsets);
  views::View* wrapped_shortcut_label =
      WrapWithPadding(shortcut_label_, insets);
  // Make the font bold.
  gfx::Font font = shortcut_label_->font();
  gfx::Font bold_font = font.DeriveFont(0, gfx::Font::BOLD);
  shortcut_label_->SetFont(bold_font);
  // TODO(satorux): Maybe we need to use language specific fonts for
  // candidate_label, like Chinese font for Chinese IME?

  // Add decoration based on the orientation.
  if (orientation_ == CandidateWindowView::kVertical) {
    // Set the background color.
    wrapped_shortcut_label->set_background(
        views::Background::CreateSolidBackground(
            kShortcutBackgroundColor));
  }
  shortcut_label_->SetColor(kShortcutColor);

  // Create the candidate label. The label will be added to |this| as a
  // child view, hence it's deleted when |this| is deleted.
  if (orientation_ == CandidateWindowView::kVertical) {
    candidate_label_ = new VerticalCandidateLabel;
  } else {
    candidate_label_ = new views::Label;
  }
  candidate_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  // Initialize the column set with two columns.
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                        0, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 4);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                        0, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 4);

  // Add the shortcut label and the candidate label.
  layout->StartRow(0, 0);
  // |wrapped_shortcut_label| and |candidate_label_| will be owned by |this|.
  layout->AddView(wrapped_shortcut_label);
  layout->AddView(candidate_label_);
}

void CandidateView::SetCandidateText(const std::wstring& text) {
  shortcut_label_->SetColor(kShortcutColor);
  candidate_label_->SetText(text);
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

void CandidateView::Disable() {
  shortcut_label_->SetColor(kDisabledShortcutColor);
  candidate_label_->SetText(L"");
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
    : orientation_(kVertical),
      current_page_index_(0),
      selected_candidate_index_in_page_(0),
      parent_frame_(parent_frame),
      candidate_area_(NULL),
      footer_area_(NULL),
      header_area_(NULL),
      auxiliary_text_label_(NULL),
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
  // Put the place holder to the header area.
  header_area_->RemoveAllChildViews(false);  // Don't delete child views.
  header_area_->AddChildView(header_area_place_holder_.get());
  ResizeAndSchedulePaint();
}

void CandidateWindowView::ShowAuxiliaryText() {
  // Put contents to the header area.
  header_area_->RemoveAllChildViews(false);  // Don't delete child views.
  header_area_->AddChildView(header_area_contents_.get());
  ResizeAndSchedulePaint();
}

void CandidateWindowView::UpdateAuxiliaryText(const std::string& utf8_text) {
  auxiliary_text_label_->SetText(UTF8ToWide(utf8_text));
}

void CandidateWindowView::UpdateCandidates(
    const ImeLookupTable& lookup_table) {
  // HACK: ibus-pinyin sets page_size to 5. For now, we use the magic
  // number here to determine the orientation.
  // TODO(satorux): We should get the orientation information from
  // lookup_table.
  Orientation orientation = kVertical;
  if (lookup_table.page_size == 5) {
    orientation = kHorizontal;
  }
  // Initialize candidate views if necessary.
  MaybeInitializeCandidateViews(lookup_table.page_size,
                                orientation);
  lookup_table_ = lookup_table;
  orientation_ = orientation;

  // Compute the index of the current page.
  current_page_index_ =
      lookup_table.cursor_absolute_index / lookup_table.page_size;

  // Update the candidates in the current page.
  const int start_from = current_page_index_ * lookup_table.page_size;

  for (size_t i = 0; i < candidate_views_.size(); ++i) {
    const size_t index_in_page = i;
    const size_t candidate_index = start_from + index_in_page;
    if (candidate_index < lookup_table_.candidates.size()) {
      candidate_views_[index_in_page]->SetCandidateText(
          UTF8ToWide(lookup_table_.candidates[candidate_index]));
    } else {
      // Disable the empty row.
      candidate_views_[index_in_page]->Disable();
    }
  }

  // Select the first candidate candidate in the page.
  const int first_candidate_in_page =
      lookup_table.cursor_absolute_index % lookup_table.page_size;
  SelectCandidateAt(first_candidate_in_page);
}

void CandidateWindowView::MaybeInitializeCandidateViews(
    int num_views,
    Orientation orientation) {
  // If the requested number of views matches the number of current views,
  // just reuse these.
  if (num_views == static_cast<int>(candidate_views_.size()) &&
      orientation == orientation_) {
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
  if (orientation == kVertical) {
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
  if (orientation == kHorizontal) {
    layout->StartRow(0, 0);
  }
  for (int i = 0; i < num_views; ++i) {
    CandidateView* candidate_row = new CandidateView(this, i, orientation);
    candidate_row->Init();
    candidate_views_.push_back(candidate_row);
    if (orientation == kVertical) {
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

  // |auxiliary_text_label_| will be owned by |header_area_contents_|.
  auxiliary_text_label_ = new views::Label;
  auxiliary_text_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  const gfx::Insets kHeaderInsets(2, 2, 2, 4);
  // |header_area_contents_| will not be owned by another view.
  // See a comment at |header_area_place_holder_| for why.
  header_area_contents_.reset(
      WrapWithPadding(auxiliary_text_label_, kHeaderInsets));
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

  // Update the footer area.
  footer_area_->RemoveAllChildViews(false);  // Don't delete child views.
  if (orientation_ == kVertical) {
    // Show information about the cursor and the page in the footer area.
    // TODO(satorux): This only works with engines that return all
    // candidates (i.e. ibus-anthy).
    footer_label_->SetText(
        StringPrintf(L"%d/%d",
                     lookup_table_.cursor_absolute_index + 1,
                     lookup_table_.candidates.size()));
    footer_area_->AddChildView(footer_area_contents_.get());
  } else {
    // Show nothing in the footer area if the orientation is horizontal.
    footer_area_->AddChildView(footer_area_place_holder_.get());
  }

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

void CandidateWindowController::Init() {
  // Initialize the IME status connection.
  ImeStatusMonitorFunctions functions;
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
  ime_status_connection_ = MonitorImeStatus(functions, this);
  CHECK(ime_status_connection_)
      << "MonitorImeStatus() failed.";

  // Create the candidate window view.
  CreateView();
}

void CandidateWindowController::CreateView() {
  // Create a non-decorated frame.
  frame_.reset(views::Widget::CreatePopupWidget(
      views::Widget::NotTransparent,
      views::Widget::AcceptEvents,
      views::Widget::DeleteOnDestroy));
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
    : ime_status_connection_(NULL),
      frame_(NULL) {
}

CandidateWindowController::~CandidateWindowController() {
  candidate_window_->RemoveObserver(this);
  chromeos::DisconnectImeStatus(ime_status_connection_);
}

gfx::Rect CandidateWindowController::GetMonitorWorkAreaNearestWindow() {
  return views::Screen::GetMonitorWorkAreaNearestWindow(
      frame_->GetNativeView());
}

void CandidateWindowController::OnHideAuxiliaryText(void* ime_library) {
  CandidateWindowController* controller =
      static_cast<CandidateWindowController*>(ime_library);

  controller->candidate_window_->HideAuxiliaryText();
}

void CandidateWindowController::OnHideLookupTable(void* ime_library) {
  CandidateWindowController* controller =
      static_cast<CandidateWindowController*>(ime_library);

  controller->frame_->Hide();
}

void CandidateWindowController::OnSetCursorLocation(void* ime_library,
                                                    int x,
                                                    int y,
                                                    int width,
                                                    int height) {
  CandidateWindowController* controller =
      static_cast<CandidateWindowController*>(ime_library);

  // TODO(satorux): This has to be computed runtime.
  const int kHorizontalOffset = 30;

  gfx::Rect frame_bounds;
  controller->frame_->GetBounds(&frame_bounds, false);

  gfx::Rect screen_bounds =
      controller->GetMonitorWorkAreaNearestWindow();

  // The default position.
  frame_bounds.set_x(x - kHorizontalOffset);
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
  controller->frame_->SetBounds(frame_bounds);
  // The call is needed to ensure that the candidate window is redrawed
  // properly after the cursor location is changed.
  controller->candidate_window_->ResizeAndSchedulePaint();
}

void CandidateWindowController::OnUpdateAuxiliaryText(
    void* ime_library,
    const std::string& utf8_text,
    bool visible) {
  CandidateWindowController* controller =
      static_cast<CandidateWindowController*>(ime_library);
  // HACK for ibus-anthy: ibus-anthy sends us page information like
  // "( 1 / 19 )" as auxiliary text. We should ignore this as we show the
  // same information in the footer area (i.e. don't want to show the same
  // information in two places).
  //
  // TODO(satorux): Remove this once we remove ibus-anthy from Chromium OS.
  if (utf8_text.size() >= 2 &&
      utf8_text[0] == '(' && utf8_text[utf8_text.size() -1] == ')') {
    // Hide the auxiliary text in case something is shown already.
    controller->candidate_window_->HideAuxiliaryText();
    return;  // Ignore the given auxiliary text.
  }

  // If it's not visible, hide the auxiliary text and return.
  if (!visible) {
    controller->candidate_window_->HideAuxiliaryText();
    return;
  }
  controller->candidate_window_->UpdateAuxiliaryText(utf8_text);
  controller->candidate_window_->ShowAuxiliaryText();
}

void CandidateWindowController::OnUpdateLookupTable(
    void* ime_library,
    const ImeLookupTable& lookup_table) {
  CandidateWindowController* controller =
      static_cast<CandidateWindowController*>(ime_library);

  // If it's not visible, hide the window and return.
  if (!lookup_table.visible) {
    controller->frame_->Hide();
    return;
  }

  controller->candidate_window_->UpdateCandidates(lookup_table);
  controller->frame_->Show();
}

void CandidateWindowController::OnCandidateCommitted(int index,
                                                     int button,
                                                     int flags) {
  NotifyCandidateClicked(ime_status_connection_, index, button, flags);
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
  ResourceBundle::InitSharedInstance(L"en-US");

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
  controller.Init();

  // Start the main loop.
  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->Run(&accelerator_handler);

  return 0;
}
