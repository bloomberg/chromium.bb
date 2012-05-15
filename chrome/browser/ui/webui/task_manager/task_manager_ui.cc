// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/task_manager/task_manager_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/task_manager/task_manager_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/chromium_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::WebContents;

namespace {

ChromeWebUIDataSource* CreateTaskManagerUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUITaskManagerHost);
  source->set_use_json_js_format_v2();

  source->AddLocalizedString("closeWindow", IDS_CLOSE);
  source->AddLocalizedString("title",IDS_TASK_MANAGER_TITLE);
  source->AddLocalizedString("aboutMemoryLink",
                             IDS_TASK_MANAGER_ABOUT_MEMORY_LINK);
  source->AddLocalizedString("killChromeOS", IDS_TASK_MANAGER_KILL_CHROMEOS);
  source->AddLocalizedString("processIDColumn",
                             IDS_TASK_MANAGER_PROCESS_ID_COLUMN);
  source->AddLocalizedString("taskColumn", IDS_TASK_MANAGER_TASK_COLUMN);
  source->AddLocalizedString("profileNameColumn",
                             IDS_TASK_MANAGER_PROFILE_NAME_COLUMN);
  source->AddLocalizedString("netColumn", IDS_TASK_MANAGER_NET_COLUMN);
  source->AddLocalizedString("cpuColumn", IDS_TASK_MANAGER_CPU_COLUMN);
  source->AddLocalizedString("physicalMemColumn",
                             IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN);
  source->AddLocalizedString("sharedMemColumn",
                             IDS_TASK_MANAGER_SHARED_MEM_COLUMN);
  source->AddLocalizedString("privateMemColumn",
                             IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN);
  source->AddLocalizedString("goatsTeleportedColumn",
                             IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN);
  source->AddLocalizedString("webcoreImageCacheColumn",
                             IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN);
  source->AddLocalizedString("webcoreScriptsCacheColumn",
                             IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN);
  source->AddLocalizedString("webcoreCSSCacheColumn",
                             IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN);
  source->AddLocalizedString("fpsColumn", IDS_TASK_MANAGER_FPS_COLUMN);
  source->AddLocalizedString("sqliteMemoryUsedColumn",
                             IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN);
  source->AddLocalizedString(
      "javascriptMemoryAllocatedColumn",
      IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN);
  source->AddLocalizedString("inspect", IDS_TASK_MANAGER_INSPECT);
  source->AddLocalizedString("activate", IDS_TASK_MANAGER_ACTIVATE);
  source->set_json_path("strings.js");
  source->add_resource_path("main.js", IDR_TASK_MANAGER_JS);
  source->add_resource_path("commands.js", IDR_TASK_MANAGER_COMMANDS_JS);
  source->add_resource_path("defines.js", IDR_TASK_MANAGER_DEFINES_JS);
  source->add_resource_path("includes.js", IDR_TASK_MANAGER_INCLUDES_JS);
  source->add_resource_path("preload.js", IDR_TASK_MANAGER_PRELOAD_JS);
  source->add_resource_path("measure_time.js",
                            IDR_TASK_MANAGER_MEASURE_TIME_JS);
  source->add_resource_path("measure_time_end.js",
                            IDR_TASK_MANAGER_MEASURE_TIME_END_JS);
  source->set_default_resource(IDR_TASK_MANAGER_HTML);

  return source;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// TaskManagerUI
//
///////////////////////////////////////////////////////////////////////////////

TaskManagerUI::TaskManagerUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new TaskManagerHandler(TaskManager::GetInstance()));

  // Set up the chrome://taskmanager/ source.
  ChromeWebUIDataSource* html_source = CreateTaskManagerUIHTMLSource();
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, html_source);
}
