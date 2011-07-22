// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/task_manager_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
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

///////////////////////////////////////////////////////////////////////////////
//
// TaskManagerUIHTMLSource
//
///////////////////////////////////////////////////////////////////////////////

class TaskManagerUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  TaskManagerUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~TaskManagerUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(TaskManagerUIHTMLSource);
};

TaskManagerUIHTMLSource::TaskManagerUIHTMLSource()
    : DataSource(chrome::kChromeUITaskManagerHost, MessageLoop::current()) {
}

void TaskManagerUIHTMLSource::StartDataRequest(const std::string& path,
                                             bool is_incognito,
                                             int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString("CLOSE_WINDOW",
      l10n_util::GetStringUTF16(IDS_CLOSE));

#define SET_LOCALIZED_STRING(STRINGS, ID) \
    (STRINGS.SetString(#ID, l10n_util::GetStringUTF16(IDS_TASK_MANAGER_##ID)))

  SET_LOCALIZED_STRING(localized_strings, TITLE);
  SET_LOCALIZED_STRING(localized_strings, ABOUT_MEMORY_LINK);
  SET_LOCALIZED_STRING(localized_strings, KILL);
  SET_LOCALIZED_STRING(localized_strings, PROCESS_ID_COLUMN);
  SET_LOCALIZED_STRING(localized_strings, PAGE_COLUMN);
  SET_LOCALIZED_STRING(localized_strings, NET_COLUMN);
  SET_LOCALIZED_STRING(localized_strings, CPU_COLUMN);
  SET_LOCALIZED_STRING(localized_strings, PHYSICAL_MEM_COLUMN);
  SET_LOCALIZED_STRING(localized_strings, SHARED_MEM_COLUMN);
  SET_LOCALIZED_STRING(localized_strings, PRIVATE_MEM_COLUMN);
  SET_LOCALIZED_STRING(localized_strings, GOATS_TELEPORTED_COLUMN);
  SET_LOCALIZED_STRING(localized_strings, WEBCORE_IMAGE_CACHE_COLUMN);
  SET_LOCALIZED_STRING(localized_strings, WEBCORE_SCRIPTS_CACHE_COLUMN);
  SET_LOCALIZED_STRING(localized_strings, WEBCORE_CSS_CACHE_COLUMN);
  SET_LOCALIZED_STRING(localized_strings, FPS_COLUMN);
  SET_LOCALIZED_STRING(localized_strings, SQLITE_MEMORY_USED_COLUMN);
  SET_LOCALIZED_STRING(localized_strings, JAVASCRIPT_MEMORY_ALLOCATED_COLUMN);

#undef SET_LOCALIZED_STRING

  SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece task_manager_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_TASK_MANAGER_HTML));
  std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      task_manager_html, &localized_strings);

  SendResponse(request_id, base::RefCountedString::TakeString(&full_html));
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

  TaskManagerUIHTMLSource* html_source = new TaskManagerUIHTMLSource();

  // Set up the chrome://taskmanager/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

