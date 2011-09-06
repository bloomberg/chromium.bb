// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of DeleteChromeHistory
#include "chrome_frame/delete_chrome_history.h"

#include "chrome/browser/browsing_data_remover.h"

#include "base/win/windows_version.h"
#include "chrome_frame/chrome_frame_activex.h"
#include "chrome_frame/utils.h"

// Below other header to avoid symbol pollution.
#define INITGUID
#include <deletebrowsinghistory.h>

DeleteChromeHistory::DeleteChromeHistory()
  : remove_mask_(0) {
  DVLOG(1) << __FUNCTION__;
}

DeleteChromeHistory::~DeleteChromeHistory() {
}


HRESULT DeleteChromeHistory::FinalConstruct() {
  DVLOG(1) << __FUNCTION__;
  Initialize();
  return S_OK;
}

void DeleteChromeHistory::OnAutomationServerReady() {
  DVLOG(1) << __FUNCTION__;
  automation_client_->RemoveBrowsingData(remove_mask_);
  loop_.Quit();
}

void DeleteChromeHistory::OnAutomationServerLaunchFailed(
      AutomationLaunchResult reason, const std::string& server_version) {
  DLOG(WARNING) << __FUNCTION__;
  loop_.Quit();
}

void DeleteChromeHistory::GetProfilePath(const std::wstring& profile_name,
                                         FilePath* profile_path) {
  ChromeFramePlugin::GetProfilePath(kIexploreProfileName, profile_path);
}

STDMETHODIMP DeleteChromeHistory::DeleteBrowsingHistory(DWORD flags) {
  DVLOG(1) << __FUNCTION__;
  // Usually called inside a quick startup/tear-down routine by RunDLL32. You
  // can simulate the process by calling:
  //    RunDll32.exe InetCpl.cpl,ClearMyTracksByProcess 255
  // Since automation setup isn't synchronous, we can be tearing down while
  // being only partially set-up, causing even synchronous IPCs to be dropped.
  // Since the *Chrome* startup/tear-down occurs synchronously from the
  // perspective of automation, we can add a flag to the chrome.exe invocation
  // in lieu of sending an IPC when it seems appropriate. Since we assume this
  // happens in one-off fashion, don't attempt to pack REMOVE_* arguments.
  // Instead, have the browser process clobber all history.
  //
  // IE8 on Vista launches us twice when the user asks to delete browsing data -
  // once in low integrity and once in medium integrity. The low integrity
  // instance will fail to connect to the automation server and restart it in an
  // effort to connect. Thus, we detect if we are in that circumstance and exit
  // silently.
  base::IntegrityLevel integrity_level;
  if (base::win::GetVersion() >= base::win::VERSION_VISTA &&
      !base::GetProcessIntegrityLevel(base::GetCurrentProcessHandle(),
                                      &integrity_level)) {
    return E_UNEXPECTED;
  }
  if (integrity_level == base::LOW_INTEGRITY) {
    return S_OK;
  }
  if (!InitializeAutomation(GetHostProcessName(false), L"", false, false,
                            GURL(), GURL(), true)) {
    return E_UNEXPECTED;
  }

  if (flags & DELETE_BROWSING_HISTORY_COOKIES)
    remove_mask_ |= BrowsingDataRemover::REMOVE_SITE_DATA;
  if (flags & DELETE_BROWSING_HISTORY_TIF)
    remove_mask_ |= BrowsingDataRemover::REMOVE_CACHE;
  if (flags & DELETE_BROWSING_HISTORY_FORMDATA)
    remove_mask_ |= BrowsingDataRemover::REMOVE_FORM_DATA;
  if (flags & DELETE_BROWSING_HISTORY_PASSWORDS)
    remove_mask_ |= BrowsingDataRemover::REMOVE_PASSWORDS;
  if (flags & DELETE_BROWSING_HISTORY_HISTORY)
    remove_mask_ |= BrowsingDataRemover::REMOVE_HISTORY;

  loop_.PostDelayedTask(FROM_HERE,
      new MessageLoop::QuitTask, 1000 * 600);
  loop_.MessageLoop::Run();

  return S_OK;
}


