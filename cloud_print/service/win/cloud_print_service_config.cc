// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlapp.h>  // NOLINT

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_dispatcher.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_constants.h"
#include "cloud_print/common/win/cloud_print_utils.h"
#include "cloud_print/resources.h"
#include "cloud_print/service/service_state.h"
#include "cloud_print/service/win/chrome_launcher.h"
#include "cloud_print/service/win/service_controller.h"
#include "cloud_print/service/win/service_utils.h"
#include "cloud_print/service/win/setup_listener.h"

using cloud_print::LoadLocalString;
using cloud_print::GetErrorMessage;

class SetupDialog : public base::RefCounted<SetupDialog>,
                    public ATL::CDialogImpl<SetupDialog> {
 public:
  // Enables accelerators.
  class Dispatcher : public base::MessagePumpDispatcher {
   public:
    explicit Dispatcher(SetupDialog* dialog) : dialog_(dialog) {}
    virtual ~Dispatcher() {};

    // MessagePumpDispatcher:
    virtual uint32_t Dispatch(const MSG& msg) OVERRIDE {
      MSG msg2 = msg;
      uint32_t action = POST_DISPATCH_NONE;
      if (!dialog_->IsDialogMessage(&msg2))
        action = POST_DISPATCH_PERFORM_DEFAULT;
      return action;
    }

   private:
    scoped_refptr<SetupDialog> dialog_;
  };

  typedef ATL::CDialogImpl<SetupDialog> Base;
  enum { IDD = IDD_SETUP_DIALOG };

  BEGIN_MSG_MAP(SetupDialog)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtrColor)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    COMMAND_ID_HANDLER(IDC_START, OnStart)
    COMMAND_ID_HANDLER(IDC_INSTALL, OnInstall)
    COMMAND_ID_HANDLER(IDC_LOGGING, OnLogging)
  END_MSG_MAP()

  SetupDialog();
 private:
  // Window Message Handlers
  LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam,
                       BOOL& handled);
  LRESULT OnCtrColor(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnCancel(UINT, INT nIdentifier, HWND, BOOL& handled);
  LRESULT OnStart(UINT, INT nIdentifier, HWND, BOOL& handled);
  LRESULT OnInstall(UINT, INT nIdentifier, HWND, BOOL& handled);
  LRESULT OnLogging(UINT, INT nIdentifier, HWND, BOOL& handled);
  LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

  void PostUITask(const base::Closure& task);
  void PostIOTask(const base::Closure& task);

  // UI Calls.

  // Disables all controls after users actions.
  void DisableControls();
  // Updates state of controls after when we received service status.
  void SetState(ServiceController::State state, const base::string16& user,
                 bool is_logging_enabled);
  // Show message box with error.
  void ShowErrorMessageBox(const base::string16& error_message);
  // Show use message box instructions how to deal with opened Chrome window.
  void AskToCloseChrome();
  base::string16 GetDlgItemText(int id) const;
  base::string16 GetUser() const;
  base::string16 GetPassword() const;
  bool IsLoggingEnabled() const;
  bool IsInstalled() const {
    return state_ > ServiceController::STATE_NOT_FOUND;
  }

  // IO Calls.
  // Installs service.
  void Install(const base::string16& user, const base::string16& password,
               bool enable_logging);
  // Starts service.
  void Start();
  // Stops service.
  void Stop();
  // Uninstall service.
  void Uninstall();
  // Update service state.
  void UpdateState();
  // Posts task to UI thread to show error using string id.
  void ShowError(int string_id);
  // Posts task to UI thread to show error using string.
  void ShowError(const base::string16& error_message);
  // Posts task to UI thread to show error using error code.
  void ShowError(HRESULT hr);

  ServiceController::State state_;
  base::Thread worker_;

  base::MessageLoop* ui_loop_;
  base::MessageLoop* io_loop_;

  ServiceController controller_;
};

SetupDialog::SetupDialog()
    : state_(ServiceController::STATE_NOT_FOUND),
      worker_("worker") {
  ui_loop_ = base::MessageLoop::current();
  DCHECK(base::MessageLoopForUI::IsCurrent());

  worker_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  io_loop_ = worker_.message_loop();
  DCHECK(io_loop_->IsType(base::MessageLoop::TYPE_IO));
}

void SetupDialog::PostUITask(const base::Closure& task) {
  ui_loop_->PostTask(FROM_HERE, task);
}

void SetupDialog::PostIOTask(const base::Closure& task) {
  io_loop_->PostTask(FROM_HERE, task);
}

void SetupDialog::ShowErrorMessageBox(const base::string16& error_message) {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  MessageBox(error_message.c_str(),
             LoadLocalString(IDS_OPERATION_FAILED_TITLE).c_str(),
             MB_ICONERROR | MB_OK);
}

void SetupDialog::AskToCloseChrome() {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  MessageBox(LoadLocalString(IDS_ADD_PRINTERS_USING_CHROME).c_str(),
             LoadLocalString(IDS_CONTINUE_IN_CHROME_TITLE).c_str(),
             MB_OK);
}

void SetupDialog::SetState(ServiceController::State status,
                           const base::string16& user,
                           bool is_logging_enabled) {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  state_ = status;

  DWORD status_string = 0;
  switch(status) {
  case ServiceController::STATE_NOT_FOUND:
    status_string = IDS_SERVICE_NOT_FOUND;
    break;
  case ServiceController::STATE_STOPPED:
    status_string = IDS_SERVICE_STOPPED;
    break;
  case ServiceController::STATE_RUNNING:
    status_string = IDS_SERVICE_RUNNING;
    break;
  }
  SetDlgItemText(IDC_STATUS,
                 status_string ? LoadLocalString(status_string).c_str() : L"");
  if (IsInstalled()) {
    SetDlgItemText(IDC_USER, user.c_str());
    CheckDlgButton(IDC_LOGGING,
                   is_logging_enabled ? BST_CHECKED : BST_UNCHECKED);
  }

  ATL::CWindow start_button = GetDlgItem(IDC_START);
  DWORD start_string = (status == ServiceController::STATE_STOPPED) ?
                       IDS_SERVICE_START : IDS_SERVICE_STOP;
  start_button.SetWindowText(LoadLocalString(start_string).c_str());
  start_button.ShowWindow(IsInstalled() ? SW_SHOW : SW_HIDE);
  start_button.EnableWindow(TRUE);

  ATL::CWindow install_button = GetDlgItem(IDC_INSTALL);
  DWORD install_string = IsInstalled() ? IDS_SERVICE_UNINSTALL :
                                         IDS_SERVICE_INSTALL;
  install_button.SetWindowText(LoadLocalString(install_string).c_str());
  install_button.ShowWindow(SW_SHOW);
  install_button.EnableWindow(TRUE);

  if (!IsInstalled()) {
    GetDlgItem(IDC_USER).EnableWindow(TRUE);
    GetDlgItem(IDC_PASSWORD).EnableWindow(TRUE);
    GetDlgItem(IDC_LOGGING).EnableWindow(TRUE);
  }
}

LRESULT SetupDialog::OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam,
                                  BOOL& handled) {
  ATLVERIFY(CenterWindow());

  WTL::CIcon icon;
  if (icon.LoadIcon(MAKEINTRESOURCE(IDI_ICON))) {
    SetIcon(icon);
  }

  SetWindowText(LoadLocalString(IDS_SETUP_PROGRAM_NAME).c_str());
  SetDlgItemText(IDC_STATE_LABEL, LoadLocalString(IDS_STATE_LABEL).c_str());
  SetDlgItemText(IDC_USER_LABEL, LoadLocalString(IDS_USER_LABEL).c_str());
  SetDlgItemText(IDC_PASSWORD_LABEL,
                 LoadLocalString(IDS_PASSWORD_LABEL).c_str());
  SetDlgItemText(IDC_LOGGING, LoadLocalString(IDS_LOGGING_LABEL).c_str());
  SetDlgItemText(IDCANCEL, LoadLocalString(IDS_CLOSE).c_str());

  SetState(ServiceController::STATE_UNKNOWN, L"", false);
  DisableControls();

  SetDlgItemText(IDC_USER, GetCurrentUserName().c_str());

  PostIOTask(base::Bind(&SetupDialog::UpdateState, this));

  return 0;
}

LRESULT SetupDialog::OnCtrColor(UINT message, WPARAM wparam, LPARAM lparam,
                                BOOL& handled) {
  HWND window = reinterpret_cast<HWND>(lparam);
  if (GetDlgItem(IDC_LOGO).m_hWnd == window) {
    return reinterpret_cast<LRESULT>(::GetStockObject(WHITE_BRUSH));
  }
  return 0;
}

LRESULT SetupDialog::OnStart(UINT, INT nIdentifier, HWND, BOOL& handled) {
  DisableControls();
  DCHECK(IsInstalled());
  if (state_ == ServiceController::STATE_RUNNING)
    PostIOTask(base::Bind(&SetupDialog::Stop, this));
  else
    PostIOTask(base::Bind(&SetupDialog::Start, this));
  return 0;
}

LRESULT SetupDialog::OnInstall(UINT, INT nIdentifier, HWND, BOOL& handled) {
  DisableControls();
  if (IsInstalled()) {
    PostIOTask(base::Bind(&SetupDialog::Uninstall, this));
  } else {
    PostIOTask(base::Bind(&SetupDialog::Install, this, GetUser(),
                          GetPassword(), IsLoggingEnabled()));
  }
  return 0;
}

LRESULT SetupDialog::OnLogging(UINT, INT nIdentifier, HWND, BOOL& handled) {
  CheckDlgButton(IDC_LOGGING, IsLoggingEnabled()? BST_UNCHECKED : BST_CHECKED);
  return 0;
}

LRESULT SetupDialog::OnCancel(UINT, INT nIdentifier, HWND, BOOL& handled) {
  DestroyWindow();
  return 0;
}

LRESULT SetupDialog::OnDestroy(UINT message, WPARAM wparam, LPARAM lparam,
                               BOOL& handled) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
  return 1;
}

void SetupDialog::DisableControls() {
  GetDlgItem(IDC_START).EnableWindow(FALSE);
  GetDlgItem(IDC_INSTALL).EnableWindow(FALSE);
  GetDlgItem(IDC_USER).EnableWindow(FALSE);
  GetDlgItem(IDC_PASSWORD).EnableWindow(FALSE);
  GetDlgItem(IDC_LOGGING).EnableWindow(FALSE);
}

base::string16 SetupDialog::GetDlgItemText(int id) const {
  const ATL::CWindow& item = GetDlgItem(id);
  size_t length = item.GetWindowTextLength();
  base::string16 result(length + 1, L'\0');
  result.resize(item.GetWindowText(&result[0], result.size()));
  return result;
}

base::string16 SetupDialog::GetUser() const {
  return GetDlgItemText(IDC_USER);
}

base::string16 SetupDialog::GetPassword() const {
  return GetDlgItemText(IDC_PASSWORD);
}

bool SetupDialog::IsLoggingEnabled() const{
  return IsDlgButtonChecked(IDC_LOGGING) == BST_CHECKED;
}

void SetupDialog::UpdateState() {
  DCHECK(base::MessageLoopForIO::IsCurrent());
  controller_.UpdateState();
  PostUITask(base::Bind(&SetupDialog::SetState, this, controller_.state(),
                        controller_.user(), controller_.is_logging_enabled()));
}

void SetupDialog::ShowError(const base::string16& error_message) {
  DCHECK(base::MessageLoopForIO::IsCurrent());
  PostUITask(base::Bind(&SetupDialog::SetState,
                        this,
                        ServiceController::STATE_UNKNOWN,
                        L"",
                        false));
  PostUITask(base::Bind(&SetupDialog::ShowErrorMessageBox, this,
                        error_message));
  LOG(ERROR) << error_message;
}

void SetupDialog::ShowError(int string_id) {
  ShowError(cloud_print::LoadLocalString(string_id));
}

void SetupDialog::ShowError(HRESULT hr) {
  ShowError(GetErrorMessage(hr));
}

void SetupDialog::Install(const base::string16& user,
                          const base::string16& password,
                          bool enable_logging) {
  // Don't forget to update state on exit.
  base::ScopedClosureRunner scoped_update_status(
        base::Bind(&SetupDialog::UpdateState, this));

  DCHECK(base::MessageLoopForIO::IsCurrent());

  SetupListener setup(GetUser());
  HRESULT hr = controller_.InstallCheckService(user, password,
                                               base::FilePath());
  if (FAILED(hr))
    return ShowError(hr);

  {
    // Always uninstall service after requirements check.
    base::ScopedClosureRunner scoped_uninstall(
        base::Bind(base::IgnoreResult(&ServiceController::UninstallService),
                   base::Unretained(&controller_)));

    hr = controller_.StartService();
    if (FAILED(hr))
      return ShowError(hr);

    if (!setup.WaitResponce(base::TimeDelta::FromSeconds(30)))
      return ShowError(IDS_ERROR_FAILED_START_SERVICE);
  }

  if (setup.user_data_dir().empty())
    return ShowError(IDS_ERROR_NO_DATA_DIR);

  if (setup.chrome_path().empty())
    return ShowError(IDS_ERROR_NO_CHROME);

  if (!setup.is_xps_available())
    return ShowError(IDS_ERROR_NO_XPS);

  base::FilePath file = setup.user_data_dir();
  file = file.Append(chrome::kServiceStateFileName);

  std::string proxy_id;
  std::string contents;

  if (base::ReadFileToString(file, &contents)) {
    ServiceState service_state;
    if (service_state.FromString(contents))
      proxy_id = service_state.proxy_id();
  }
  PostUITask(base::Bind(&SetupDialog::AskToCloseChrome, this));
  contents = ChromeLauncher::CreateServiceStateFile(proxy_id, setup.printers());

  if (contents.empty())
    return ShowError(IDS_ERROR_FAILED_CREATE_CONFIG);

  size_t written = base::WriteFile(file, contents.c_str(),
                                        contents.size());
  if (written != contents.size()) {
    DWORD last_error = GetLastError();
    if (!last_error)
      return ShowError(IDS_ERROR_FAILED_CREATE_CONFIG);
    return ShowError(HRESULT_FROM_WIN32(last_error));
  }

  hr = controller_.InstallConnectorService(user, password, base::FilePath(),
                                           enable_logging);
  if (FAILED(hr))
    return ShowError(hr);

  hr = controller_.StartService();
  if (FAILED(hr))
    return ShowError(hr);
}

void SetupDialog::Start() {
  DCHECK(base::MessageLoopForIO::IsCurrent());
  HRESULT hr = controller_.StartService();
  if (FAILED(hr))
    ShowError(hr);
  UpdateState();
}

void SetupDialog::Stop() {
  DCHECK(base::MessageLoopForIO::IsCurrent());
  HRESULT hr = controller_.StopService();
  if (FAILED(hr))
    ShowError(hr);
  UpdateState();
}

void SetupDialog::Uninstall() {
  DCHECK(base::MessageLoopForIO::IsCurrent());
  HRESULT hr = controller_.UninstallService();
  if (FAILED(hr))
    ShowError(hr);
  UpdateState();
}

class CloudPrintServiceConfigModule
    : public ATL::CAtlExeModuleT<CloudPrintServiceConfigModule> {
};

CloudPrintServiceConfigModule _AtlModule;

int WINAPI WinMain(__in  HINSTANCE hInstance,
                   __in  HINSTANCE hPrevInstance,
                   __in  LPSTR lpCmdLine,
                   __in  int nCmdShow) {
  base::AtExitManager at_exit;
  CommandLine::Init(0, NULL);

  base::MessageLoopForUI loop;
  scoped_refptr<SetupDialog> dialog(new SetupDialog());
  dialog->Create(NULL);
  dialog->ShowWindow(SW_SHOW);
  SetupDialog::Dispatcher dispatcher(dialog);
  base::RunLoop run_loop(&dispatcher);
  run_loop.Run();
  return 0;
}
