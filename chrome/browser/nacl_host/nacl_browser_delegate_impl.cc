// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"

#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/pnacl/pnacl_component_installer.h"
#include "chrome/browser/nacl_host/nacl_infobar_delegate.h"
#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "content/public/browser/browser_thread.h"
#include "ppapi/c/private/ppb_nacl_private.h"

void NaClBrowserDelegateImpl::ShowNaClInfobar(int render_process_id,
                                              int render_view_id,
                                              int error_id) {
  DCHECK_EQ(PP_NACL_MANIFEST_MISSING_ARCH, error_id);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&NaClInfoBarDelegate::Create, render_process_id,
                 render_view_id));
}

bool NaClBrowserDelegateImpl::DialogsAreSuppressed() {
  return logging::DialogsAreSuppressed();
}

bool NaClBrowserDelegateImpl::GetCacheDirectory(base::FilePath* cache_dir) {
  base::FilePath user_data_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return false;
  chrome::GetUserCacheDirectory(user_data_dir, cache_dir);
  return true;
}

bool NaClBrowserDelegateImpl::GetPluginDirectory(base::FilePath* plugin_dir) {
  return PathService::Get(chrome::DIR_INTERNAL_PLUGINS, plugin_dir);
}

bool NaClBrowserDelegateImpl::GetPnaclDirectory(base::FilePath* pnacl_dir) {
  return PathService::Get(chrome::DIR_PNACL_COMPONENT, pnacl_dir);
}

bool NaClBrowserDelegateImpl::GetUserDirectory(base::FilePath* user_dir) {
  return PathService::Get(chrome::DIR_USER_DATA, user_dir);
}

std::string NaClBrowserDelegateImpl::GetVersionString() const {
  return chrome::VersionInfo().CreateVersionString();
}

ppapi::host::HostFactory* NaClBrowserDelegateImpl::CreatePpapiHostFactory(
    content::BrowserPpapiHost* ppapi_host) {
  return new chrome::ChromeBrowserPepperHostFactory(ppapi_host);
}

void NaClBrowserDelegateImpl::TryInstallPnacl(
    const base::Callback<void(bool)>& installed) {
  PnaclComponentInstaller* pci =
      g_browser_process->pnacl_component_installer();
  if (pci)
    pci->RequestFirstInstall(installed);
  else
    installed.Run(false);
}
