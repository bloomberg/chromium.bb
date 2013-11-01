// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_list/start_page_ui.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/sys_info.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_list/start_page_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

namespace app_list {
namespace {
#if defined(OS_CHROMEOS)
const char* kHotwordFilenames[] = {
  "dnn",
  "ep_acoustic_model",
  "hclg_shotword",
  "hotword_arm.nexe",
  "hotword_classifier",
  "hotword_normalizer",
  "hotword_word_symbols",
  "hotword_x86_32.nexe",
  "hotword_x86_64.nexe",
  "okgoogle_hotword.config",
  "phonelist",
  "phone_state_map",
};

const char kHotwordFilesDir[] = "/opt/google/hotword";

void LoadModelData(const std::string& path,
                   const content::WebUIDataSource::GotDataCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  // Will be owned by |callback|.
  scoped_refptr<base::RefCountedString> data(new base::RefCountedString());
  base::FilePath base_dir(kHotwordFilesDir);
  base::ReadFileToString(base_dir.AppendASCII(path), &(data->data()));
  callback.Run(data.get());
}

bool HandleHotwordFilesResourceFilter(
    const std::string& path,
    const content::WebUIDataSource::GotDataCallback& callback) {
  for (size_t i = 0; i < arraysize(kHotwordFilenames); ++i) {
    if (path == kHotwordFilenames[i]) {
      content::BrowserThread::PostTask(
          content::BrowserThread::FILE,
          FROM_HERE,
          base::Bind(&LoadModelData, path, callback));
      return true;
    }
  }

  return false;
}
#endif  // OS_CHROMEOS
}  // namespace

StartPageUI::StartPageUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(new StartPageHandler);
  InitDataSource();
}

StartPageUI::~StartPageUI() {}

void StartPageUI::InitDataSource() {
  scoped_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIAppListStartPageHost));

  source->SetUseJsonJSFormatV2();
  source->SetJsonPath("strings.js");

  source->AddResourcePath("start_page.css", IDR_APP_LIST_START_PAGE_CSS);
  source->AddResourcePath("start_page.js", IDR_APP_LIST_START_PAGE_JS);
  source->SetDefaultResource(IDR_APP_LIST_START_PAGE_HTML);

#if defined(OS_CHROMEOS)
  source->OverrideContentSecurityPolicyObjectSrc("object-src 'self' data:;");
  if (base::SysInfo::IsRunningOnChromeOS())
    source->SetRequestFilter(base::Bind(&HandleHotwordFilesResourceFilter));
#endif

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui()), source.release());
}

}  // namespace app_list
