// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/input_window_dialog.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/widget/widget.h"
#include "views/window/dialog_delegate.h"

namespace {

// Width of the text field, in pixels.
const int kTextfieldWidth = 200;

}  // namespace

// The Windows implementation of the cross platform input dialog interface.
class WinInputWindowDialog : public InputWindowDialog {
 public:
  WinInputWindowDialog(gfx::NativeWindow parent,
                       const string16& window_title,
                       const string16& label,
                       const string16& contents,
                       Delegate* delegate);
  virtual ~WinInputWindowDialog();

  // Overridden from InputWindowDialog:
  virtual void Show() OVERRIDE;
  virtual void Close() OVERRIDE;

  const string16& window_title() const { return window_title_; }
  const string16& label() const { return label_; }
  const string16& contents() const { return contents_; }

  InputWindowDialog::Delegate* delegate() { return delegate_.get(); }

 private:
  // Our chrome views window.
  views::Widget* window_;

  // Strings to feed to the on screen window.
  string16 window_title_;
  string16 label_;
  string16 contents_;

  // Our delegate. Consumes the window's output.
  scoped_ptr<InputWindowDialog::Delegate> delegate_;
};

// ContentView, as the name implies, is the content view for the InputWindow.
// It registers accelerators that accept/cancel the input.
class ContentView : public views::DialogDelegateView,
                    public views::TextfieldController {
 public:
  explicit ContentView(WinInputWindowDialog* delegate);

  // views::DialogDelegateView:
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual bool IsModal() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  // views::TextfieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(views::Textfield*,
                              const views::KeyEvent&) OVERRIDE;

 protected:
  // views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;

 private:
  // Set up dialog controls and layout.
  void InitControlLayout();

  // Sets focus to the first focusable element within the dialog.
  void FocusFirstFocusableControl();

  // The Textfield that the user can type into.
  views::Textfield* text_field_;

  // The delegate that the ContentView uses to communicate changes to the
  // caller.
  WinInputWindowDialog* delegate_;

  // Helps us set focus to the first Textfield in the window.
  ScopedRunnableMethodFactory<ContentView> focus_grabber_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentView);
};

///////////////////////////////////////////////////////////////////////////////
// ContentView
ContentView::ContentView(WinInputWindowDialog* delegate)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(focus_grabber_factory_(this)) {
    DCHECK(delegate_);
}

///////////////////////////////////////////////////////////////////////////////
// ContentView, views::DialogDelegate implementation:

bool ContentView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK &&
      !delegate_->delegate()->IsValid(text_field_->text())) {
    return false;
  }
  return true;
}

bool ContentView::Accept() {
  delegate_->delegate()->InputAccepted(text_field_->text());
  return true;
}

bool ContentView::Cancel() {
  delegate_->delegate()->InputCanceled();
  return true;
}

void ContentView::DeleteDelegate() {
  delete delegate_;
}

string16 ContentView::GetWindowTitle() const {
  return delegate_->window_title();
}

bool ContentView::IsModal() const {
  return true;
}

views::View* ContentView::GetContentsView() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// ContentView, views::TextfieldController implementation:

void ContentView::ContentsChanged(views::Textfield* sender,
                                  const string16& new_contents) {
  GetDialogClientView()->UpdateDialogButtons();
}

bool ContentView::HandleKeyEvent(views::Textfield*,
                                 const views::KeyEvent&) {
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// ContentView, protected:

void ContentView::ViewHierarchyChanged(bool is_add,
                                       views::View* parent,
                                       views::View* child) {
  if (is_add && child == this)
    InitControlLayout();
}

///////////////////////////////////////////////////////////////////////////////
// ContentView, private:

void ContentView::InitControlLayout() {
  text_field_ = new views::Textfield;
  text_field_->SetText(UTF16ToWideHack(delegate_->contents()));
  text_field_->SetController(this);

  using views::GridLayout;

  // TODO(sky): Vertical alignment should be baseline.
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  views::ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  c1->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  c1->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                GridLayout::USE_PREF, kTextfieldWidth, kTextfieldWidth);

  layout->StartRow(0, 0);
  views::Label* label = new views::Label(UTF16ToWideHack(delegate_->label()));
  layout->AddView(label);
  layout->AddView(text_field_);

  MessageLoop::current()->PostTask(FROM_HERE,
      focus_grabber_factory_.NewRunnableMethod(
          &ContentView::FocusFirstFocusableControl));
}

void ContentView::FocusFirstFocusableControl() {
  text_field_->SelectAll();
  text_field_->RequestFocus();
}

WinInputWindowDialog::WinInputWindowDialog(gfx::NativeWindow parent,
                                           const string16& window_title,
                                           const string16& label,
                                           const string16& contents,
                                           Delegate* delegate)
    : window_title_(window_title),
      label_(label),
      contents_(contents),
      delegate_(delegate) {
  window_ = views::Widget::CreateWindowWithParent(new ContentView(this),
                                                  parent);
  window_->client_view()->AsDialogClientView()->UpdateDialogButtons();
}

WinInputWindowDialog::~WinInputWindowDialog() {
}

void WinInputWindowDialog::Show() {
  window_->Show();
}

void WinInputWindowDialog::Close() {
  window_->Close();
}

// static
InputWindowDialog* InputWindowDialog::Create(gfx::NativeWindow parent,
                                             const string16& window_title,
                                             const string16& label,
                                             const string16& contents,
                                             Delegate* delegate) {
  return new WinInputWindowDialog(
      parent, window_title, label, contents, delegate);
}
