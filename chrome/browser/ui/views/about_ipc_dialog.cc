// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Need to include this before any other file because it defines
// IPC_MESSAGE_LOG_ENABLED. We need to use it to define
// IPC_MESSAGE_MACROS_LOG_ENABLED so that all_messages.h will generate the
// ViewMsgLog et al. functions.
#include "ipc/ipc_message.h"

#ifdef IPC_MESSAGE_LOG_ENABLED
#define IPC_MESSAGE_MACROS_LOG_ENABLED

// We need to do this real early to be sure IPC_MESSAGE_MACROS_LOG_ENABLED
// doesn't get undefined.
#include "chrome/common/all_messages.h"

#include "chrome/browser/ui/views/about_ipc_dialog.h"

#include <set>

#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/content_ipc_logging.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "views/controls/button/text_button.h"
#include "views/controls/native/native_view_host.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/widget/widget.h"

namespace {

// We don't localize this UI since this is a developer-only feature.
const wchar_t kStartTrackingLabel[] = L"Start tracking";
const wchar_t kStopTrackingLabel[] = L"Stop tracking";
const wchar_t kClearLabel[] = L"Clear";
const wchar_t kFilterLabel[] = L"Filter...";

enum {
  kTimeColumn = 0,
  kChannelColumn,
  kMessageColumn,
  kFlagsColumn,
  kDispatchColumn,
  kProcessColumn,
  kParamsColumn,
};

// This class registers the browser IPC logger functions with IPC::Logging.
class RegisterLoggerFuncs {
 public:
  RegisterLoggerFuncs() {
    IPC::Logging::set_log_function_map(&g_log_function_mapping);
  }
};

RegisterLoggerFuncs g_register_logger_funcs;

// The singleton dialog box. This is non-NULL when a dialog is active so we
// know not to create a new one.
AboutIPCDialog* g_active_dialog = NULL;

std::set<int> disabled_messages;

// Settings dialog -------------------------------------------------------------

bool init_done = false;
HWND settings_dialog = NULL;
// Settings.
CListViewCtrl* messages = NULL;

void OnCheck(int id, bool checked) {
  if (!init_done)
    return;

  if (checked)
    disabled_messages.erase(id);
  else
    disabled_messages.insert(id);
}

void InitDialog(HWND hwnd) {
  messages = new CListViewCtrl(::GetDlgItem(hwnd, IDC_Messages));

  messages->SetViewType(LVS_REPORT);
  messages->SetExtendedListViewStyle(LVS_EX_CHECKBOXES);
  messages->ModifyStyle(0, LVS_SORTASCENDING | LVS_NOCOLUMNHEADER);
  messages->InsertColumn(0, L"id", LVCFMT_LEFT, 230);

  LogFunctionMap* log_functions = IPC::Logging::log_function_map();
  for (LogFunctionMap::iterator i = log_functions->begin();
       i != log_functions->end(); ++i) {
    std::string name;
    (*i->second)(&name, NULL, NULL);
    if (name.empty())
      continue;  // Will happen if the message file isn't included above.
    std::wstring wname = UTF8ToWide(name);

    int index = messages->InsertItem(
        LVIF_TEXT | LVIF_PARAM, 0, wname.c_str(), 0, 0, 0, i->first);

    messages->SetItemText(index, 0, wname.c_str());

    if (disabled_messages.find(i->first) == disabled_messages.end())
      messages->SetCheckState(index, TRUE);
  }

  init_done = true;
}

void CloseDialog() {
  delete messages;
  messages = NULL;

  init_done = false;

  ::DestroyWindow(settings_dialog);
  settings_dialog = NULL;

  /* The old version of this code stored the last settings in the preferences.
     But with this dialog, there currently isn't an easy way to get the profile
     to save in the preferences.
  Profile* current_profile = profile();
  if (!current_profile)
    return;
  PrefService* prefs = current_profile->GetPrefs();
  if (!prefs->FindPreference(prefs::kIpcDisabledMessages))
    return;
  ListValue* list = prefs->GetMutableList(prefs::kIpcDisabledMessages);
  list->Clear();
  for (std::set<int>::const_iterator itr = disabled_messages_.begin();
       itr != disabled_messages_.end();
       ++itr) {
    list->Append(Value::CreateIntegerValue(*itr));
  }
  */
}

void OnButtonClick(int id) {
  int count = messages->GetItemCount();
  for (int i = 0; i < count; ++i)
    messages->SetCheckState(i, id == IDC_MessagesAll);
}

INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_INITDIALOG:
      InitDialog(hwnd);
      return FALSE;  // Don't set keyboard focus.
    case WM_SYSCOMMAND:
      if (wparam == SC_CLOSE) {
        CloseDialog();
        return FALSE;
      }
      break;
    case WM_NOTIFY: {
      NMLISTVIEW* info = reinterpret_cast<NM_LISTVIEW*>(lparam);
      if (wparam == IDC_Messages && info->hdr.code == LVN_ITEMCHANGED) {
        if (info->uChanged & LVIF_STATE) {
          bool checked = (info->uNewState >> 12) == 2;
          OnCheck(static_cast<int>(info->lParam), checked);
        }
        return FALSE;
      }
      break;
    }
    case WM_COMMAND:
      if (HIWORD(wparam) == BN_CLICKED)
        OnButtonClick(LOWORD(wparam));
      break;
  }
  return FALSE;
}

void RunSettingsDialog(HWND parent) {
  if (settings_dialog)
    return;
  HINSTANCE module_handle = GetModuleHandle(chrome::kBrowserResourcesDll);
  settings_dialog = CreateDialog(module_handle,
                                 MAKEINTRESOURCE(IDD_IPC_SETTINGS),
                                 NULL,
                                 &DialogProc);
  ::ShowWindow(settings_dialog, SW_SHOW);
}

}  // namespace

// AboutIPCDialog --------------------------------------------------------------

AboutIPCDialog::AboutIPCDialog()
    : track_toggle_(NULL),
      clear_button_(NULL),
      filter_button_(NULL),
      table_(NULL),
      tracking_(false) {
  SetupControls();
  IPC::Logging::GetInstance()->SetConsumer(this);
}

AboutIPCDialog::~AboutIPCDialog() {
  g_active_dialog = NULL;
  IPC::Logging::GetInstance()->SetConsumer(NULL);
}

// static
void AboutIPCDialog::RunDialog() {
  if (!g_active_dialog) {
    g_active_dialog = new AboutIPCDialog;
    views::Widget::CreateWindow(g_active_dialog)->Show();
  } else {
    // TODO(brettw) it would be nice to focus the existing window.
  }
}

void AboutIPCDialog::SetupControls() {
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  track_toggle_ = new views::TextButton(this, kStartTrackingLabel);
  clear_button_ = new views::TextButton(this, kClearLabel);
  filter_button_ = new views::TextButton(this, kFilterLabel);

  table_ = new views::NativeViewHost;

  static const int first_column_set = 1;
  views::ColumnSet* column_set = layout->AddColumnSet(first_column_set);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                        33.33f, views::GridLayout::FIXED, 0, 0);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                        33.33f, views::GridLayout::FIXED, 0, 0);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                        33.33f, views::GridLayout::FIXED, 0, 0);

  static const int table_column_set = 2;
  column_set = layout->AddColumnSet(table_column_set);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        100.0f, views::GridLayout::FIXED, 0, 0);

  layout->StartRow(0, first_column_set);
  layout->AddView(track_toggle_);
  layout->AddView(clear_button_);
  layout->AddView(filter_button_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  layout->StartRow(1.0f, table_column_set);
  layout->AddView(table_);
}

gfx::Size AboutIPCDialog::GetPreferredSize() {
  return gfx::Size(800, 400);
}

views::View* AboutIPCDialog::GetContentsView() {
  return this;
}

int AboutIPCDialog::GetDialogButtons() const {
  // Don't want OK or Cancel.
  return 0;
}

string16 AboutIPCDialog::GetWindowTitle() const {
  return ASCIIToUTF16("about:ipc");
}

void AboutIPCDialog::Layout() {
  if (!message_list_.m_hWnd) {
    HWND parent_window = GetWidget()->GetNativeView();

    RECT rect = {0, 0, 10, 10};
    HWND list_hwnd = message_list_.Create(parent_window,
        rect, NULL, WS_CHILD | WS_VISIBLE | LVS_SORTASCENDING);
    message_list_.SetViewType(LVS_REPORT);
    message_list_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);

    int column_index = 0;
    message_list_.InsertColumn(kTimeColumn, L"time", LVCFMT_LEFT, 80);
    message_list_.InsertColumn(kChannelColumn, L"channel", LVCFMT_LEFT, 110);
    message_list_.InsertColumn(kMessageColumn, L"message", LVCFMT_LEFT, 240);
    message_list_.InsertColumn(kFlagsColumn, L"flags", LVCFMT_LEFT, 50);
    message_list_.InsertColumn(kDispatchColumn, L"dispatch (ms)", LVCFMT_RIGHT,
                               80);
    message_list_.InsertColumn(kProcessColumn, L"process (ms)", LVCFMT_RIGHT,
                               80);
    message_list_.InsertColumn(kParamsColumn, L"parameters", LVCFMT_LEFT, 500);

    table_->Attach(list_hwnd);
  }

  View::Layout();
}

void AboutIPCDialog::Log(const IPC::LogData& data) {
  if (disabled_messages.find(data.type) != disabled_messages.end())
    return;  // Message type is filtered out.

  base::Time sent = base::Time::FromInternalValue(data.sent);
  base::Time::Exploded exploded;
  sent.LocalExplode(&exploded);
  if (exploded.hour > 12)
    exploded.hour -= 12;

  std::wstring sent_str = StringPrintf(L"%02d:%02d:%02d.%03d",
      exploded.hour, exploded.minute, exploded.second, exploded.millisecond);

  int count = message_list_.GetItemCount();
  int index = message_list_.InsertItem(count, sent_str.c_str());

  message_list_.SetItemText(index, kTimeColumn, sent_str.c_str());
  message_list_.SetItemText(index, kChannelColumn,
                            ASCIIToWide(data.channel).c_str());

  std::string message_name;
  IPC::Logging::GetMessageText(data.type, &message_name, NULL, NULL);
  message_list_.SetItemText(index, kMessageColumn,
                            UTF8ToWide(message_name).c_str());
  message_list_.SetItemText(index, kFlagsColumn,
                            UTF8ToWide(data.flags).c_str());

  int64 time_to_send = (base::Time::FromInternalValue(data.receive) -
      sent).InMilliseconds();
  // time can go backwards by a few ms (see Time), don't show that.
  time_to_send = std::max(static_cast<int>(time_to_send), 0);
  std::wstring temp = StringPrintf(L"%d", time_to_send);
  message_list_.SetItemText(index, kDispatchColumn, temp.c_str());

  int64 time_to_process = (base::Time::FromInternalValue(data.dispatch) -
      base::Time::FromInternalValue(data.receive)).InMilliseconds();
  time_to_process = std::max(static_cast<int>(time_to_process), 0);
  temp = StringPrintf(L"%d", time_to_process);
  message_list_.SetItemText(index, kProcessColumn, temp.c_str());

  message_list_.SetItemText(index, kParamsColumn,
                            UTF8ToWide(data.params).c_str());
  message_list_.EnsureVisible(index, FALSE);
}

bool AboutIPCDialog::CanResize() const {
  return true;
}

void AboutIPCDialog::ButtonPressed(
    views::Button* button, const views::Event& event) {
  if (button == track_toggle_) {
    if (tracking_) {
      track_toggle_->SetText(kStartTrackingLabel);
      tracking_ = false;
      content::EnableIPCLogging(false);
    } else {
      track_toggle_->SetText(kStopTrackingLabel);
      tracking_ = true;
      content::EnableIPCLogging(true);
    }
    track_toggle_->SchedulePaint();
  } else if (button == clear_button_) {
    message_list_.DeleteAllItems();
  } else if (button == filter_button_) {
    RunSettingsDialog(GetWidget()->GetNativeView());
  }
}

namespace browser {

void ShowAboutIPCDialog() {
  AboutIPCDialog::RunDialog();
}

} // namespace browser

#endif  // IPC_MESSAGE_LOG_ENABLED
