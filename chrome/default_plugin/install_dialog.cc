// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/default_plugin/install_dialog.h"

#include "base/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/default_plugin/plugin_impl.h"
#include "grit/webkit_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/webkit_glue.h"

typedef base::hash_map<const std::wstring, PluginInstallDialog*> DialogMap;
base::LazyInstance<DialogMap> s_dialogs = LAZY_INSTANCE_INITIALIZER;

PluginInstallDialog* PluginInstallDialog::AddInstaller(
    PluginInstallerImpl* plugin_impl, const std::wstring& plugin_name) {
  PluginInstallDialog* dialog;
  if (s_dialogs.Get().count(plugin_name)) {
    dialog = s_dialogs.Get()[plugin_name];
  } else {
    dialog = new PluginInstallDialog(plugin_name);
  }

  dialog->installers_.push_back(plugin_impl);
  return dialog;
}

PluginInstallDialog::PluginInstallDialog(const std::wstring& plugin_name)
    : plugin_name_(plugin_name) {
  s_dialogs.Get()[plugin_name] = this;
}

PluginInstallDialog::~PluginInstallDialog() {
  s_dialogs.Get().erase(plugin_name_);
  if (IsWindow())
    DestroyWindow();
}

void PluginInstallDialog::RemoveInstaller(PluginInstallerImpl* installer) {
  for (size_t i = 0; i < installers_.size(); ++i) {
    if (installers_[i] == installer) {
      installers_.erase(installers_.begin() + i);
      if (installers_.empty())
        delete this;
      return;
    }
  }
  NOTREACHED();
}

void PluginInstallDialog::ShowInstallDialog(HWND parent) {
  if (IsWindow())
    return;

  Create(parent, NULL);
  ShowWindow(SW_SHOW);
}

HWND PluginInstallDialog::Create(HWND parent_window, LPARAM init_param) {
  // Most of the code here is based on CDialogImpl<T>::Create.
  DCHECK(m_hWnd == NULL);

  // Allocate the thunk structure here, where we can fail
  // gracefully.
  BOOL thunk_inited = m_thunk.Init(NULL, NULL);
  if (thunk_inited == FALSE) {
    SetLastError(ERROR_OUTOFMEMORY);
    return NULL;
  }

  _AtlWinModule.AddCreateWndData(&m_thunk.cd, this);

#ifndef NDEBUG
  m_bModal = false;
#endif // NDEBUG

  HINSTANCE instance_handle = _AtlBaseModule.GetResourceInstance();

  HRSRC dialog_resource = FindResource(instance_handle,
                                       MAKEINTRESOURCE(IDD), RT_DIALOG);
  if (!dialog_resource) {
    NOTREACHED();
    return NULL;
  }

  HGLOBAL dialog_template = LoadResource(instance_handle,
                                         dialog_resource);
  if (!dialog_template) {
    NOTREACHED();
    return NULL;
  }

  _DialogSplitHelper::DLGTEMPLATEEX* dialog_template_struct =
      reinterpret_cast<_DialogSplitHelper::DLGTEMPLATEEX *>(
          ::LockResource(dialog_template));
  DCHECK(dialog_template_struct != NULL);

  unsigned long dialog_template_size =
      SizeofResource(instance_handle, dialog_resource);

  HGLOBAL rtl_layout_dialog_template = NULL;

  if (PluginInstallerImpl::IsRTLLayout()) {
    rtl_layout_dialog_template = GlobalAlloc(GPTR, dialog_template_size);
    DCHECK(rtl_layout_dialog_template != NULL);

    _DialogSplitHelper::DLGTEMPLATEEX* rtl_layout_dialog_template_struct =
        reinterpret_cast<_DialogSplitHelper::DLGTEMPLATEEX*>(
            ::GlobalLock(rtl_layout_dialog_template));
    DCHECK(rtl_layout_dialog_template_struct != NULL);

    memcpy(rtl_layout_dialog_template_struct, dialog_template_struct,
           dialog_template_size);

    rtl_layout_dialog_template_struct->exStyle |=
        (WS_EX_LAYOUTRTL | WS_EX_RTLREADING);

    dialog_template_struct = rtl_layout_dialog_template_struct;
  }

  HWND dialog_window =
      CreateDialogIndirectParam(
          instance_handle,
          reinterpret_cast<DLGTEMPLATE*>(dialog_template_struct),
          parent_window, StartDialogProc, init_param);

  DCHECK(m_hWnd == dialog_window);

  if (rtl_layout_dialog_template) {
    GlobalUnlock(rtl_layout_dialog_template);
    GlobalFree(rtl_layout_dialog_template);
  }

  return dialog_window;
}


LRESULT PluginInstallDialog::OnInitDialog(UINT message, WPARAM wparam,
                                          LPARAM lparam, BOOL& handled) {
  std::wstring dialog_title =
      PluginInstallerImpl::ReplaceStringForPossibleEmptyReplacement(
          IDS_DEFAULT_PLUGIN_CONFIRMATION_DIALOG_TITLE,
          IDS_DEFAULT_PLUGIN_CONFIRMATION_DIALOG_TITLE_NO_PLUGIN_NAME,
          plugin_name_);
  AdjustTextDirectionality(&dialog_title);
  SetWindowText(dialog_title.c_str());

  std::wstring get_the_plugin_btn_msg =
      l10n_util::GetStringUTF16(
          IDS_DEFAULT_PLUGIN_GET_THE_PLUGIN_BTN_MSG);
  AdjustTextDirectionality(&get_the_plugin_btn_msg);
  SetDlgItemText(IDB_GET_THE_PLUGIN, get_the_plugin_btn_msg.c_str());

  std::wstring cancel_plugin_download_msg =
      l10n_util::GetStringUTF16(
          IDS_DEFAULT_PLUGIN_CANCEL_PLUGIN_DOWNLOAD_MSG);
  AdjustTextDirectionality(&cancel_plugin_download_msg);
  SetDlgItemText(IDCANCEL, cancel_plugin_download_msg.c_str());

  std::wstring plugin_user_action_msg =
      PluginInstallerImpl::ReplaceStringForPossibleEmptyReplacement(
          IDS_DEFAULT_PLUGIN_USER_OPTION_MSG,
          IDS_DEFAULT_PLUGIN_USER_OPTION_MSG_NO_PLUGIN_NAME,
          plugin_name_);
  AdjustTextDirectionality(&plugin_user_action_msg);
  SetDlgItemText(IDC_PLUGIN_INSTALL_CONFIRMATION_LABEL,
                 plugin_user_action_msg.c_str());
  return 0;
}


LRESULT PluginInstallDialog::OnGetPlugin(WORD notify_code, WORD id,
                                         HWND wnd_ctl, BOOL &handled) {
  DestroyWindow();
  if (!installers_.empty())
    installers_[0]->DownloadPlugin();

  return 0;
}

LRESULT PluginInstallDialog::OnCancel(WORD notify_code, WORD id, HWND wnd_ctl,
                                      BOOL &handled) {
  DestroyWindow();
  if (!installers_.empty())
    installers_[0]->DownloadCancelled();
  return 0;
}

// TODO(idana) bug# 1246452: use the library l10n_util once it is moved from
// the Chrome module into the Base module. For now, we simply copy/paste the
// same code.
void PluginInstallDialog::AdjustTextDirectionality(std::wstring* text) const {
  if (PluginInstallerImpl::IsRTLLayout()) {
    // Inserting an RLE (Right-To-Left Embedding) mark as the first character.
    text->insert(0, L"\x202B");

    // Inserting a PDF (Pop Directional Formatting) mark as the last character.
    text->append(L"\x202C");
  }
}
