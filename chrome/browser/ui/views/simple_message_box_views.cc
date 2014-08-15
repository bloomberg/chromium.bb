// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/wm/public/dispatcher_client.h"

#if defined(OS_WIN)
#include "ui/base/win/message_box_win.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace chrome {

namespace {

class SimpleMessageBoxViews : public views::DialogDelegate {
 public:
  SimpleMessageBoxViews(const base::string16& title,
                        const base::string16& message,
                        MessageBoxType type,
                        const base::string16& yes_text,
                        const base::string16& no_text,
                        bool is_system_modal);
  virtual ~SimpleMessageBoxViews();

  MessageBoxResult RunDialogAndGetResult();

  // Overridden from views::DialogDelegate:
  virtual int GetDialogButtons() const OVERRIDE;
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

 private:

  // This terminates the nested message-loop.
  void Done();

  const base::string16 window_title_;
  const MessageBoxType type_;
  base::string16 yes_text_;
  base::string16 no_text_;
  MessageBoxResult* result_;
  bool is_system_modal_;
  views::MessageBoxView* message_box_view_;
  base::Closure quit_runloop_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMessageBoxViews);
};

////////////////////////////////////////////////////////////////////////////////
// SimpleMessageBoxViews, public:

SimpleMessageBoxViews::SimpleMessageBoxViews(const base::string16& title,
                                             const base::string16& message,
                                             MessageBoxType type,
                                             const base::string16& yes_text,
                                             const base::string16& no_text,
                                             bool is_system_modal)
    : window_title_(title),
      type_(type),
      yes_text_(yes_text),
      no_text_(no_text),
      result_(NULL),
      is_system_modal_(is_system_modal),
      message_box_view_(new views::MessageBoxView(
          views::MessageBoxView::InitParams(message))) {
  if (yes_text_.empty()) {
    if (type_ == MESSAGE_BOX_TYPE_QUESTION)
      yes_text_ =
          l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL);
    else if (type_ == MESSAGE_BOX_TYPE_OK_CANCEL)
      yes_text_ = l10n_util::GetStringUTF16(IDS_OK);
    else
      yes_text_ = l10n_util::GetStringUTF16(IDS_OK);
  }

  if (no_text_.empty()) {
    if (type_ == MESSAGE_BOX_TYPE_QUESTION)
      no_text_ =
          l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL);
    else if (type_ == MESSAGE_BOX_TYPE_OK_CANCEL)
      no_text_ = l10n_util::GetStringUTF16(IDS_CANCEL);
  }
}

SimpleMessageBoxViews::~SimpleMessageBoxViews() {
}

MessageBoxResult SimpleMessageBoxViews::RunDialogAndGetResult() {
  MessageBoxResult result = MESSAGE_BOX_RESULT_NO;
  result_ = &result;
  // Use the widget's window itself so that the message loop exists when the
  // dialog is closed by some other means than |Cancel| or |Accept|.
  aura::Window* anchor = GetWidget()->GetNativeWindow();
  aura::client::DispatcherRunLoop run_loop(
      aura::client::GetDispatcherClient(anchor->GetRootWindow()), NULL);
  quit_runloop_ = run_loop.QuitClosure();
  run_loop.Run();
  return result;
}

int SimpleMessageBoxViews::GetDialogButtons() const {
  if (type_ == MESSAGE_BOX_TYPE_QUESTION ||
      type_ == MESSAGE_BOX_TYPE_OK_CANCEL) {
    return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  }

  return ui::DIALOG_BUTTON_OK;
}

base::string16 SimpleMessageBoxViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return no_text_;
  return yes_text_;
}

bool SimpleMessageBoxViews::Cancel() {
  *result_ = MESSAGE_BOX_RESULT_NO;
  Done();
  return true;
}

bool SimpleMessageBoxViews::Accept() {
  *result_ = MESSAGE_BOX_RESULT_YES;
  Done();
  return true;
}

base::string16 SimpleMessageBoxViews::GetWindowTitle() const {
  return window_title_;
}

void SimpleMessageBoxViews::DeleteDelegate() {
  delete this;
}

ui::ModalType SimpleMessageBoxViews::GetModalType() const {
  return is_system_modal_ ? ui::MODAL_TYPE_SYSTEM : ui::MODAL_TYPE_WINDOW;
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

////////////////////////////////////////////////////////////////////////////////
// SimpleMessageBoxViews, private:

void SimpleMessageBoxViews::Done() {
  CHECK(!quit_runloop_.is_null());
  quit_runloop_.Run();
}

#if defined(OS_WIN)
UINT GetMessageBoxFlagsFromType(MessageBoxType type) {
  UINT flags = MB_SETFOREGROUND;
  switch (type) {
    case MESSAGE_BOX_TYPE_INFORMATION:
      return flags | MB_OK | MB_ICONINFORMATION;
    case MESSAGE_BOX_TYPE_WARNING:
      return flags | MB_OK | MB_ICONWARNING;
    case MESSAGE_BOX_TYPE_QUESTION:
      return flags | MB_YESNO | MB_ICONQUESTION;
    case MESSAGE_BOX_TYPE_OK_CANCEL:
      return flags | MB_OKCANCEL | MB_ICONWARNING;
  }
  NOTREACHED();
  return flags | MB_OK | MB_ICONWARNING;
}
#endif

MessageBoxResult ShowMessageBoxImpl(gfx::NativeWindow parent,
                                    const base::string16& title,
                                    const base::string16& message,
                                    MessageBoxType type,
                                    const base::string16& yes_text,
                                    const base::string16& no_text) {
  // Views dialogs cannot be shown outside the UI thread message loop or if the
  // ResourceBundle is not initialized yet.
  // Fallback to logging with a default response or a Windows MessageBox.
#if defined(OS_WIN)
  if (!base::MessageLoopForUI::IsCurrent() ||
      !base::MessageLoopForUI::current()->is_running() ||
      !ResourceBundle::HasSharedInstance()) {
    int result = ui::MessageBox(views::HWNDForNativeWindow(parent), message,
                                title, GetMessageBoxFlagsFromType(type));
    return (result == IDYES || result == IDOK) ?
        MESSAGE_BOX_RESULT_YES : MESSAGE_BOX_RESULT_NO;
  }
#else
  if (!base::MessageLoopForUI::IsCurrent() ||
      !ResourceBundle::HasSharedInstance()) {
    LOG(ERROR) << "Unable to show a dialog outside the UI thread message loop: "
               << title << " - " << message;
    return MESSAGE_BOX_RESULT_NO;
  }
#endif

  SimpleMessageBoxViews* dialog =
      new SimpleMessageBoxViews(title,
                                message,
                                type,
                                yes_text,
                                no_text,
                                parent == NULL  // is_system_modal
                                );
  CreateBrowserModalDialogViews(dialog, parent)->Show();

  // NOTE: |dialog| may have been deleted by the time |RunDialogAndGetResult()|
  // returns.
  return dialog->RunDialogAndGetResult();
}

}  // namespace

MessageBoxResult ShowMessageBox(gfx::NativeWindow parent,
                                const base::string16& title,
                                const base::string16& message,
                                MessageBoxType type) {
  return ShowMessageBoxImpl(
      parent, title, message, type, base::string16(), base::string16());
}

MessageBoxResult ShowMessageBoxWithButtonText(gfx::NativeWindow parent,
                                              const base::string16& title,
                                              const base::string16& message,
                                              const base::string16& yes_text,
                                              const base::string16& no_text) {
  return ShowMessageBoxImpl(
      parent, title, message, MESSAGE_BOX_TYPE_QUESTION, yes_text, no_text);
}

}  // namespace chrome
