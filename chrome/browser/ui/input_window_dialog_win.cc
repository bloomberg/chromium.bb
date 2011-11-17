// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/input_window_dialog.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "chrome/browser/ui/webui/input_window_dialog_webui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/window/dialog_delegate.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/widget/widget.h"

namespace {

// Width of the text field, in pixels.
const int kTextfieldWidth = 200;

}  // namespace

// The Windows implementation of the cross platform input dialog interface.
class InputWindowDialogWin : public InputWindowDialog {
 public:
  InputWindowDialogWin(gfx::NativeWindow parent,
                       const string16& window_title,
                       const string16& label,
                       const string16& contents,
                       Delegate* delegate,
                       ButtonType type);
  virtual ~InputWindowDialogWin();

  // Overridden from InputWindowDialog:
  virtual void Show() OVERRIDE;
  virtual void Close() OVERRIDE;

  const string16& window_title() const { return window_title_; }
  const string16& label() const { return label_; }
  const string16& contents() const { return contents_; }

  Delegate* delegate() { return delegate_.get(); }
  ButtonType type() const { return type_; }

 private:
  // Our chrome views window.
  views::Widget* window_;

  // Strings to feed to the on screen window.
  string16 window_title_;
  string16 label_;
  string16 contents_;

  // Our delegate. Consumes the window's output.
  scoped_ptr<InputWindowDialog::Delegate> delegate_;
  const ButtonType type_;

  DISALLOW_COPY_AND_ASSIGN(InputWindowDialogWin);
};

// ContentView, as the name implies, is the content view for the InputWindow.
// It registers accelerators that accept/cancel the input.
class ContentView : public views::DialogDelegateView,
                    public views::TextfieldController {
 public:
  explicit ContentView(InputWindowDialogWin* delegate);

  // views::DialogDelegateView:
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;
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
  InputWindowDialogWin* delegate_;

  // Helps us set focus to the first Textfield in the window.
  base::WeakPtrFactory<ContentView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentView);
};

///////////////////////////////////////////////////////////////////////////////
// ContentView
ContentView::ContentView(InputWindowDialogWin* delegate)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
    DCHECK(delegate_);
}

///////////////////////////////////////////////////////////////////////////////
// ContentView, views::DialogDelegate implementation:

string16 ContentView::GetDialogButtonLabel(ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    return l10n_util::GetStringUTF16(
        delegate_->type() == InputWindowDialog::BUTTON_TYPE_ADD ? IDS_ADD
                                                                : IDS_SAVE);
  }
  return string16();
}

bool ContentView::IsDialogButtonEnabled(ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    InputWindowDialog::InputTexts texts;
    texts.push_back(text_field_->text());
    if (!delegate_->delegate()->IsValid(texts)) {
      return false;
    }
  }
  return true;
}

bool ContentView::Accept() {
  InputWindowDialog::InputTexts texts;
  texts.push_back(text_field_->text());
  delegate_->delegate()->InputAccepted(texts);
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
  text_field_->SetText(delegate_->contents());
  text_field_->SetController(this);

  // TODO(sky): Vertical alignment should be baseline.
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  views::ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);
  c1->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  c1->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                views::GridLayout::USE_PREF, kTextfieldWidth, kTextfieldWidth);

  layout->StartRow(0, 0);
  views::Label* label = new views::Label(delegate_->label());
  layout->AddView(label);
  layout->AddView(text_field_);

  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ContentView::FocusFirstFocusableControl,
                            weak_factory_.GetWeakPtr()));
}

void ContentView::FocusFirstFocusableControl() {
  text_field_->SelectAll();
  text_field_->RequestFocus();
}

InputWindowDialogWin::InputWindowDialogWin(gfx::NativeWindow parent,
                                           const string16& window_title,
                                           const string16& label,
                                           const string16& contents,
                                           Delegate* delegate,
                                           ButtonType type)
    : window_title_(window_title),
      label_(label),
      contents_(contents),
      delegate_(delegate),
      type_(type) {
  window_ = views::Widget::CreateWindowWithParent(new ContentView(this),
                                                  parent);
  window_->client_view()->AsDialogClientView()->UpdateDialogButtons();
}

InputWindowDialogWin::~InputWindowDialogWin() {
}

void InputWindowDialogWin::Show() {
  window_->Show();
}

void InputWindowDialogWin::Close() {
  window_->Close();
}

// static
InputWindowDialog* InputWindowDialog::Create(
    gfx::NativeWindow parent,
    const string16& window_title,
    const LabelContentsPairs& label_contents_pairs,
    Delegate* delegate,
    ButtonType type) {
  if (ChromeWebUI::IsMoreWebUI()) {
    return new InputWindowDialogWebUI(window_title,
                                      label_contents_pairs,
                                      delegate,
                                      type);
  } else {
    DCHECK_EQ(1U, label_contents_pairs.size());
    return new InputWindowDialogWin(parent,
                                    window_title,
                                    label_contents_pairs[0].first,
                                    label_contents_pairs[0].second,
                                    delegate,
                                    type);
  }
}
