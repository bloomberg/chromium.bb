// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"

#include <stack>
#include <vector>

#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/browser/pref_service.h"

CloudPrintProxyService::CloudPrintProxyService(Profile* profile) {
}

CloudPrintProxyService::~CloudPrintProxyService() {
  Shutdown();
}

void CloudPrintProxyService::Initialize() {
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);
  prefs->RegisterStringPref(prefs::kCloudPrintProxyId,  L"");
  prefs->RegisterStringPref(prefs::kCloudPrintProxyName,  L"");
  prefs->RegisterStringPref(prefs::kCloudPrintAuthToken,  L"");
}


void CloudPrintProxyService::EnableForUser(const std::string& auth_token) {
  if (backend_.get())
    return;

  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);
  std::string proxy_id =
      WideToUTF8(prefs->GetString(prefs::kCloudPrintProxyId));
  if (proxy_id.empty()) {
    // TODO(sanjeevr): Determine whether the proxy id should be server generated
    proxy_id = cloud_print::GenerateProxyId();
    prefs->SetString(prefs::kCloudPrintProxyId, UTF8ToWide(proxy_id));
  }
  std::string token_to_use = auth_token;
  if (token_to_use.empty()) {
    token_to_use = WideToUTF8(prefs->GetString(prefs::kCloudPrintAuthToken));
  } else {
    prefs->SetString(prefs::kCloudPrintAuthToken, UTF8ToWide(token_to_use));
  }

  backend_.reset(new CloudPrintProxyBackend(this));
  backend_->Initialize(token_to_use, proxy_id);
}

void CloudPrintProxyService::DisableForUser() {
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);
  prefs->ClearPref(prefs::kCloudPrintAuthToken);
  Shutdown();
}

void CloudPrintProxyService::HandlePrinterNotification(
    const std::string& printer_id) {
  if (backend_.get())
    backend_->HandlePrinterNotification(printer_id);
}

void CloudPrintProxyService::Shutdown() {
  if (backend_.get())
    backend_->Shutdown();
  backend_.reset();
}

// Notification methods from the backend. Called on UI thread.
void CloudPrintProxyService::OnPrinterListAvailable(
    const cloud_print::PrinterList& printer_list) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  // Here we will trim the list to eliminate printers already registered.
  // If there are any more printers left in the list after trimming, we will
  // show the print selection UI. Any printers left in the list after the user
  // selection process will then be registered.
  backend_->RegisterPrinters(printer_list);
}

// Called when authentication is done. Called on UI thread.
// Note that sid can be empty. This is a temp function to steal the sid
// from the Bookmarks Sync code. When the common GAIA signin code is done,
// The CloudPrintProxyService will simply get a notification when authentication
// done with an lsid and a sid.
void CloudPrintProxyService::OnAuthenticated(const std::string& sid) {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  profile->GetCloudPrintProxyService()->EnableForUser(sid);
}

