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
  localized_strings.SetString("title",
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_TITLE));
  localized_strings.SetString("about_memory_link",
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_ABOUT_MEMORY_LINK));
  localized_strings.SetString("close_window",
      l10n_util::GetStringUTF16(IDS_CLOSE));
  localized_strings.SetString("kill_process",
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_KILL));

  SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece task_manager_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_TASK_MANAGER_HTML));
  const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      task_manager_html, &localized_strings);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
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

