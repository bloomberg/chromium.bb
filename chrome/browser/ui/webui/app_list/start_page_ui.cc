// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_list/start_page_ui.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/sys_info.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_list/start_page_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "grit/browser_resources.h"

namespace app_list {
namespace {
#if defined(OS_CHROMEOS)
const char* kHotwordFilePrefixes[] = {
  "hotword_",
  "_platform_specific/",
};

void LoadModelData(const base::FilePath& base_dir,
                   const std::string& path,
                   const content::WebUIDataSource::GotDataCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
  // Will be owned by |callback|.
  scoped_refptr<base::RefCountedString> data(new base::RefCountedString());
  base::ReadFileToString(base_dir.AppendASCII(path), &(data->data()));
  callback.Run(data.get());
}

bool HandleHotwordFilesResourceFilter(
    Profile* profile,
    const std::string& path,
    const content::WebUIDataSource::GotDataCallback& callback) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const extensions::Extension* extension =
      service->GetExtensionById(extension_misc::kHotwordExtensionId, false);
  if (!extension)
    return false;

  for (size_t i = 0; i < arraysize(kHotwordFilePrefixes); ++i) {
    if (path.find(kHotwordFilePrefixes[i]) == 0) {
      content::BrowserThread::PostTask(
          content::BrowserThread::FILE,
          FROM_HERE,
          base::Bind(&LoadModelData, extension->path(), path, callback));
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
  source->AddResourcePath("hotword_nacl.nmf", IDR_APP_LIST_HOTWORD_NACL_NMF);
  source->SetDefaultResource(IDR_APP_LIST_START_PAGE_HTML);

#if defined(OS_CHROMEOS)
  source->OverrideContentSecurityPolicyObjectSrc("object-src 'self' data:;");
  if (base::SysInfo::IsRunningOnChromeOS())
    source->SetRequestFilter(base::Bind(&HandleHotwordFilesResourceFilter,
                                        Profile::FromWebUI(web_ui())));
#endif

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui()), source.release());
}

}  // namespace app_list
