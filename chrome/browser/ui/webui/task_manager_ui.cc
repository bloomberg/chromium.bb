// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/task_manager_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/task_manager_handler.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/chromium_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Convenience macro for AddLocalizedString() method.
#define SET_LOCALIZED_STRING(ID) \
  source->AddLocalizedString(#ID, IDS_TASK_MANAGER_##ID)

ChromeWebUIDataSource* CreateTaskManagerUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUITaskManagerHost);

  source->AddLocalizedString("CLOSE_WINDOW", IDS_CLOSE);
  SET_LOCALIZED_STRING(TITLE);
  SET_LOCALIZED_STRING(ABOUT_MEMORY_LINK);
  SET_LOCALIZED_STRING(KILL_CHROMEOS);
  SET_LOCALIZED_STRING(PROCESS_ID_COLUMN);
  SET_LOCALIZED_STRING(PAGE_COLUMN);
  SET_LOCALIZED_STRING(PROFILE_NAME_COLUMN);
  SET_LOCALIZED_STRING(NET_COLUMN);
  SET_LOCALIZED_STRING(CPU_COLUMN);
  SET_LOCALIZED_STRING(PHYSICAL_MEM_COLUMN);
  SET_LOCALIZED_STRING(SHARED_MEM_COLUMN);
  SET_LOCALIZED_STRING(PRIVATE_MEM_COLUMN);
  SET_LOCALIZED_STRING(GOATS_TELEPORTED_COLUMN);
  SET_LOCALIZED_STRING(WEBCORE_IMAGE_CACHE_COLUMN);
  SET_LOCALIZED_STRING(WEBCORE_SCRIPTS_CACHE_COLUMN);
  SET_LOCALIZED_STRING(WEBCORE_CSS_CACHE_COLUMN);
  SET_LOCALIZED_STRING(FPS_COLUMN);
  SET_LOCALIZED_STRING(SQLITE_MEMORY_USED_COLUMN);
  SET_LOCALIZED_STRING(JAVASCRIPT_MEMORY_ALLOCATED_COLUMN);
  source->set_json_path("strings.js");
  source->add_resource_path("main.js", IDR_TASK_MANAGER_JS);
  source->add_resource_path("includes.js", IDR_TASK_MANAGER_INCLUDES_JS);
  source->set_default_resource(IDR_TASK_MANAGER_HTML);

  return source;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// TaskManagerUI
//
///////////////////////////////////////////////////////////////////////////////

TaskManagerUI::TaskManagerUI(TabContents* contents) : ChromeWebUI(contents) {
  TaskManagerHandler* handler =
      new TaskManagerHandler(TaskManager::GetInstance());

  handler->Attach(this);
  handler->Init();
  AddMessageHandler(handler);

  // Set up the chrome://taskmanager/ source.
  ChromeWebUIDataSource* html_source = CreateTaskManagerUIHTMLSource();
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(html_source);
}

