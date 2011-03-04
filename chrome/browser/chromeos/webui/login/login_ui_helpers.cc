// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// #include <algorithm>

#include <algorithm>

#include "base/file_path.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/webui/login/login_ui_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/jstemplate_builder.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
//
// ProfileOperationsInterface
//
////////////////////////////////////////////////////////////////////////////////
Profile* ProfileOperationsInterface::GetDefaultProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetDefaultProfile();
}

Profile* ProfileOperationsInterface::GetDefaultProfileByPath() {
  return GetDefaultProfile(GetUserDataPath());
}

Profile* ProfileOperationsInterface::GetDefaultProfile(FilePath user_data_dir) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetDefaultProfile(user_data_dir);
}

FilePath ProfileOperationsInterface::GetUserDataPath() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir;
}

////////////////////////////////////////////////////////////////////////////////
//
// BrowserOperationsInterface
//
////////////////////////////////////////////////////////////////////////////////
Browser* BrowserOperationsInterface::CreateBrowser(Profile* profile) {
  return Browser::Create(profile);
}

Browser* BrowserOperationsInterface::GetLoginBrowser(Profile* profile) {
  return BrowserList::FindBrowserWithType(profile,
                                          Browser::TYPE_ANY,
                                          true);
}

////////////////////////////////////////////////////////////////////////////////
//
// HTMLOperationsInterface
//
////////////////////////////////////////////////////////////////////////////////
base::StringPiece HTMLOperationsInterface::GetLoginHTML() {
  base::StringPiece login_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_LOGIN_HTML));
  return login_html;
}

base::StringPiece HTMLOperationsInterface::GetLoginContainerHTML() {
  base::StringPiece login_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_LOGIN_CONTAINER_HTML));
  return login_html;
}

std::string HTMLOperationsInterface::GetFullHTML(
    base::StringPiece login_html,
    DictionaryValue* localized_strings) {
  return jstemplate_builder::GetI18nTemplateHtml(
      login_html,
      localized_strings);
}

RefCountedBytes* HTMLOperationsInterface::CreateHTMLBytes(
    std::string full_html) {
  RefCountedBytes* html_bytes = new RefCountedBytes();
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(),
            full_html.end(),
            html_bytes->data.begin());
  return html_bytes;
}

}  // namespace chromeos
