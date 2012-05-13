// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(USE_AURA)
#include "ui/aura/client/dispatcher_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#endif

namespace {

class SimpleMessageBoxViews : public views::DialogDelegate,
                              public MessageLoop::Dispatcher,
                              public base::RefCounted<SimpleMessageBoxViews> {
 public:
  static void ShowMessageBox(gfx::NativeWindow parent_window,
                             const string16& title,
                             const string16& message,
                             browser::MessageBoxType type);
  static bool ShowQuestionMessageBox(gfx::NativeWindow parent_window,
                                     const string16& title,
                                     const string16& message);

  // Returns true if the Accept button was clicked.
  bool accepted() const { return disposition_ == DISPOSITION_OK; }

 private:
  friend class base::RefCounted<SimpleMessageBoxViews>;

  enum DialogType {
    DIALOG_TYPE_INFORMATION,
    DIALOG_TYPE_WARNING,
    DIALOG_TYPE_QUESTION,
  };

  // The state of the dialog when closing.
  enum Disposition {
    DISPOSITION_UNKNOWN,
    DISPOSITION_CANCEL,
    DISPOSITION_OK
  };

  SimpleMessageBoxViews(gfx::NativeWindow parent_window,
                        const string16& title,
                        const string16& message,
                        DialogType dialog_type);
  virtual ~SimpleMessageBoxViews();

  // Overridden from views::DialogDelegate:
  virtual int GetDialogButtons() const OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // Overridden from MessageLoop::Dispatcher:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  const string16 window_title_;
  const DialogType dialog_type_;
  Disposition disposition_;
  views::MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMessageBoxViews);
};

////////////////////////////////////////////////////////////////////////////////
// SimpleMessageBoxViews, public:

// static
void SimpleMessageBoxViews::ShowMessageBox(
    gfx::NativeWindow parent_window,
    const string16& title,
    const string16& message,
    browser::MessageBoxType type) {
  DialogType message_type = DIALOG_TYPE_INFORMATION;
  if (type == browser::MESSAGE_BOX_TYPE_WARNING)
    message_type = DIALOG_TYPE_WARNING;

  // This is a reference counted object so it is given an initial increment
  // in the constructor with a corresponding decrement in DeleteDelegate().
  new SimpleMessageBoxViews(parent_window, title, message, message_type);
}

bool SimpleMessageBoxViews::ShowQuestionMessageBox(
    gfx::NativeWindow parent_window,
    const string16& title,
    const string16& message) {
  // This is a reference counted object so it is given an initial increment
  // in the constructor plus an extra one below to ensure the dialog persists
  // until we retrieve the user response..
  scoped_refptr<SimpleMessageBoxViews> dialog = new SimpleMessageBoxViews(
      parent_window, title, message, DIALOG_TYPE_QUESTION);

  // Make sure Chrome doesn't attempt to shut down with the dialog up.
  g_browser_process->AddRefModule();

#if defined(USE_AURA)
  aura::client::GetDispatcherClient(parent_window->GetRootWindow())->
      RunWithDispatcher(dialog, parent_window, true);
#else
  {
    MessageLoop::ScopedNestableTaskAllower allow(MessageLoopForUI::current());
    MessageLoopForUI::current()->RunWithDispatcher(dialog);
  }
#endif

  g_browser_process->ReleaseModule();

  return dialog->accepted();
}

////////////////////////////////////////////////////////////////////////////////
// SimpleMessageBoxViews, private:

SimpleMessageBoxViews::SimpleMessageBoxViews(gfx::NativeWindow parent_window,
                                             const string16& title,
                                             const string16& message,
                                             DialogType dialog_type)
    : window_title_(title),
      dialog_type_(dialog_type),
      disposition_(DISPOSITION_UNKNOWN),
      message_box_view_(new views::MessageBoxView(
          views::MessageBoxView::InitParams(message))) {
  views::Widget::CreateWindowWithParent(this, parent_window)->Show();

  // Add reference to be released in DeleteDelegate().
  AddRef();
}

SimpleMessageBoxViews::~SimpleMessageBoxViews() {
}

int SimpleMessageBoxViews::GetDialogButtons() const {
  if (dialog_type_ == DIALOG_TYPE_INFORMATION ||
      dialog_type_ == DIALOG_TYPE_WARNING)
    return ui::DIALOG_BUTTON_OK;
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

string16 SimpleMessageBoxViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16((button == ui::DIALOG_BUTTON_OK) ?
      IDS_OK : IDS_CLOSE);
}

bool SimpleMessageBoxViews::Cancel() {
  disposition_ = DISPOSITION_CANCEL;
  return true;
}

bool SimpleMessageBoxViews::Accept() {
  disposition_ = DISPOSITION_OK;
  return true;
}

string16 SimpleMessageBoxViews::GetWindowTitle() const {
  return window_title_;
}

void SimpleMessageBoxViews::DeleteDelegate() {
  Release();
}

ui::ModalType SimpleMessageBoxViews::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

views::View* SimpleMessageBoxViews::GetContentsView() {
  return message_box_view_;
}

views::Widget* SimpleMessageBoxViews::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* SimpleMessageBoxViews::GetWidget() const {
  return message_box_view_->GetWidget();
}

bool SimpleMessageBoxViews::Dispatch(const base::NativeEvent& event) {
#if defined(OS_WIN)
  TranslateMessage(&event);
  DispatchMessage(&event);
#elif defined(USE_AURA)
  aura::Env::GetInstance()->GetDispatcher()->Dispatch(event);
#endif
  return disposition_ == DISPOSITION_UNKNOWN;
}

}  // namespace

namespace browser {

void ShowMessageBox(gfx::NativeWindow parent,
                    const string16& title,
                    const string16& message,
                    MessageBoxType type) {
  SimpleMessageBoxViews::ShowMessageBox(parent, title, message, type);
}

bool ShowQuestionMessageBox(gfx::NativeWindow parent,
                            const string16& title,
                            const string16& message) {
  return SimpleMessageBoxViews::ShowQuestionMessageBox(parent, title, message);
}

}  // namespace browser
