// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/input_window_dialog_win.h"

#include "app/l10n_util.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "grit/generated_resources.h"

// Width to make the text field, in pixels.
static const int kTextfieldWidth = 200;

InputWindowDialogWin::InputWindowDialogWin(gfx::NativeWindow parent,
                                           const std::wstring& window_title,
                                           const std::wstring& label,
                                           const std::wstring& contents,
                                           Delegate* delegate)
    : window_title_(window_title),
      label_(label),
      contents_(contents),
      delegate_(delegate) {
  window_ = views::Window::CreateChromeWindow(parent, gfx::Rect(),
                                              new ContentView(this));
  window_->GetClientView()->AsDialogClientView()->UpdateDialogButtons();
}

InputWindowDialogWin::~InputWindowDialogWin() {
}

void InputWindowDialogWin::Show() {
  window_->Show();
}

void InputWindowDialogWin::Close() {
  window_->Close();
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

std::wstring ContentView::GetWindowTitle() const {
  return delegate_->window_title();
}

///////////////////////////////////////////////////////////////////////////////
// ContentView, views::Textfield::Controller implementation:

void ContentView::ContentsChanged(views::Textfield* sender,
                                  const std::wstring& new_contents) {
  GetDialogClientView()->UpdateDialogButtons();
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

  using views::ColumnSet;
  using views::GridLayout;

  // TODO(sky): Vertical alignment should be baseline.
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  c1->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  c1->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                GridLayout::USE_PREF, kTextfieldWidth, kTextfieldWidth);

  layout->StartRow(0, 0);
  views::Label* label = new views::Label(delegate_->label());
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

// static
InputWindowDialog* InputWindowDialog::Create(HWND parent,
                                             const std::wstring& window_title,
                                             const std::wstring& label,
                                             const std::wstring& contents,
                                             Delegate* delegate) {
  return new InputWindowDialogWin(parent,
                                  window_title,
                                  label,
                                  contents,
                                  delegate);
}

