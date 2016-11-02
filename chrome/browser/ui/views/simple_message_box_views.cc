// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/simple_message_box_internal.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

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
                        const base::string16& checkbox_text,
                        bool is_system_modal);
  ~SimpleMessageBoxViews() override;

  MessageBoxResult RunDialogAndGetResult();

  // Overridden from views::DialogDelegate:
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool Cancel() override;
  bool Accept() override;

  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  void DeleteDelegate() override;
  ui::ModalType GetModalType() const override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

 private:
  // This terminates the nested message-loop.
  void Done();

  const base::string16 window_title_;
  const MessageBoxType type_;
  base::string16 yes_text_;
  base::string16 no_text_;
  MessageBoxResult* result_;
  views::MessageBoxView* message_box_view_;
  base::Closure quit_runloop_;
  bool is_system_modal_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMessageBoxViews);
};

// The currently showing message box, if there is one. Used for tests.
SimpleMessageBoxViews* g_current_message_box = nullptr;

////////////////////////////////////////////////////////////////////////////////
// SimpleMessageBoxViews, public:

SimpleMessageBoxViews::SimpleMessageBoxViews(
    const base::string16& title,
    const base::string16& message,
    MessageBoxType type,
    const base::string16& yes_text,
    const base::string16& no_text,
    const base::string16& checkbox_text,
    bool is_system_modal)
    : window_title_(title),
      type_(type),
      yes_text_(yes_text),
      no_text_(no_text),
      result_(NULL),
      message_box_view_(new views::MessageBoxView(
          views::MessageBoxView::InitParams(message))),
      is_system_modal_(is_system_modal) {
  if (yes_text_.empty()) {
    yes_text_ =
        type_ == MESSAGE_BOX_TYPE_QUESTION
            ? l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL)
            : l10n_util::GetStringUTF16(IDS_OK);
  }

  if (no_text_.empty() && type_ == MESSAGE_BOX_TYPE_QUESTION)
    no_text_ = l10n_util::GetStringUTF16(IDS_CANCEL);

  if (!checkbox_text.empty()) {
    message_box_view_->SetCheckBoxLabel(checkbox_text);
    message_box_view_->SetCheckBoxSelected(true);
  }
}

SimpleMessageBoxViews::~SimpleMessageBoxViews() {
}

MessageBoxResult SimpleMessageBoxViews::RunDialogAndGetResult() {
  g_current_message_box = this;
  MessageBoxResult result = MESSAGE_BOX_RESULT_NO;
  result_ = &result;
  // TODO(pkotwicz): Exit message loop when the dialog is closed by some other
  // means than |Cancel| or |Accept|. crbug.com/404385
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);
  base::RunLoop run_loop;
  quit_runloop_ = run_loop.QuitClosure();
  run_loop.Run();
  g_current_message_box = nullptr;
  return result;
}

int SimpleMessageBoxViews::GetDialogButtons() const {
  if (type_ == MESSAGE_BOX_TYPE_QUESTION)
    return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;

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
  if (!message_box_view_->HasCheckBox() ||
      message_box_view_->IsCheckBoxSelected()) {
    *result_ = MESSAGE_BOX_RESULT_YES;
  } else {
    *result_ = MESSAGE_BOX_RESULT_NO;
  }

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
    case MESSAGE_BOX_TYPE_WARNING:
      return flags | MB_OK | MB_ICONWARNING;
    case MESSAGE_BOX_TYPE_QUESTION:
      return flags | MB_YESNO | MB_ICONQUESTION;
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
                                    const base::string16& no_text,
                                    const base::string16& checkbox_text) {
  startup_metric_utils::SetNonBrowserUIDisplayed();
  if (internal::g_should_skip_message_box_for_test)
    return MESSAGE_BOX_RESULT_YES;

  // Views dialogs cannot be shown outside the UI thread message loop or if the
  // ResourceBundle is not initialized yet.
  // Fallback to logging with a default response or a Windows MessageBox.
#if defined(OS_WIN)
  if (!base::MessageLoopForUI::IsCurrent() ||
      !base::MessageLoopForUI::current()->is_running() ||
      !ResourceBundle::HasSharedInstance()) {
    LOG_IF(ERROR, !checkbox_text.empty()) << "Dialog checkbox won't be shown";
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
      new SimpleMessageBoxViews(title, message, type, yes_text, no_text,
                                checkbox_text, !parent /* is_system_modal */);
  constrained_window::CreateBrowserModalDialogViews(dialog, parent)->Show();

  // NOTE: |dialog| may have been deleted by the time |RunDialogAndGetResult()|
  // returns.
  return dialog->RunDialogAndGetResult();
}

}  // namespace

bool CloseMessageBoxForTest(bool accept) {
  if (!g_current_message_box)
    return false;

  if (accept)
    g_current_message_box->Accept();
  else
    g_current_message_box->Cancel();
  return true;
}

void ShowWarningMessageBox(gfx::NativeWindow parent,
                           const base::string16& title,
                           const base::string16& message) {
  ShowMessageBoxImpl(parent, title, message, MESSAGE_BOX_TYPE_WARNING,
                     base::string16(), base::string16(), base::string16());
}

bool ShowWarningMessageBoxWithCheckbox(gfx::NativeWindow parent,
                                       const base::string16& title,
                                       const base::string16& message,
                                       const base::string16& checkbox_text) {
  return ShowMessageBoxImpl(parent, title, message, MESSAGE_BOX_TYPE_WARNING,
                            base::string16(), base::string16(),
                            checkbox_text) == MESSAGE_BOX_RESULT_YES;
}

MessageBoxResult ShowQuestionMessageBox(gfx::NativeWindow parent,
                                        const base::string16& title,
                                        const base::string16& message) {
  return ShowMessageBoxImpl(parent, title, message, MESSAGE_BOX_TYPE_QUESTION,
                            base::string16(), base::string16(),
                            base::string16());
}

MessageBoxResult ShowMessageBoxWithButtonText(gfx::NativeWindow parent,
                                              const base::string16& title,
                                              const base::string16& message,
                                              const base::string16& yes_text,
                                              const base::string16& no_text) {
  return ShowMessageBoxImpl(parent, title, message, MESSAGE_BOX_TYPE_QUESTION,
                            yes_text, no_text, base::string16());
}

}  // namespace chrome
