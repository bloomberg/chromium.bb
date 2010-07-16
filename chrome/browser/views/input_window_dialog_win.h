// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INPUT_WINDOW_DIALOG_WIN_H_
#define CHROME_BROWSER_VIEWS_INPUT_WINDOW_DIALOG_WIN_H_

#include "chrome/browser/input_window_dialog.h"

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

// The Windows implementation of the cross platform input dialog interface.
class InputWindowDialogWin : public InputWindowDialog {
 public:
  InputWindowDialogWin(gfx::NativeWindow parent,
                       const std::wstring& window_title,
                       const std::wstring& label,
                       const std::wstring& contents,
                       Delegate* delegate);
  virtual ~InputWindowDialogWin();

  virtual void Show();
  virtual void Close();

  const std::wstring& window_title() const { return window_title_; }
  const std::wstring& label() const { return label_; }
  const std::wstring& contents() const { return contents_; }

  InputWindowDialog::Delegate* delegate() { return delegate_.get(); }

 private:
  // Our chrome views window.
  views::Window* window_;

  // Strings to feed to the on screen window.
  std::wstring window_title_;
  std::wstring label_;
  std::wstring contents_;

  // Our delegate. Consumes the window's output.
  scoped_ptr<InputWindowDialog::Delegate> delegate_;
};

// ContentView, as the name implies, is the content view for the InputWindow.
// It registers accelerators that accept/cancel the input.
class ContentView : public views::View,
                    public views::DialogDelegate,
                    public views::Textfield::Controller {
 public:
  explicit ContentView(InputWindowDialogWin* delegate)
      : delegate_(delegate),
        ALLOW_THIS_IN_INITIALIZER_LIST(focus_grabber_factory_(this)) {
    DCHECK(delegate_);
  }

  // views::DialogDelegate overrides:
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual void DeleteDelegate() { delete delegate_; }
  virtual std::wstring GetWindowTitle() const;
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }

  // views::Textfield::Controller overrides:
  virtual void ContentsChanged(views::Textfield* sender,
                               const std::wstring& new_contents);
  virtual bool HandleKeystroke(views::Textfield*,
                               const views::Textfield::Keystroke&) {
    return false;
  }

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

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
  ScopedRunnableMethodFactory<ContentView> focus_grabber_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentView);
};

#endif  // CHROME_BROWSER_VIEWS_INPUT_WINDOW_DIALOG_WIN_H_
