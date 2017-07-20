// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/memory_internals_ui.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

// Generates one row of the returned process info.
base::Value MakeProcessInfo(int pid, const std::string& description) {
  base::Value result(base::Value::Type::LIST);
  result.GetList().push_back(base::Value(pid));
  result.GetList().push_back(base::Value(description));
  return result;
}

content::WebUIDataSource* CreateMemoryInternalsUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIMemoryInternalsHost);
  source->SetDefaultResource(IDR_MEMORY_INTERNALS_HTML);
  source->AddResourcePath("memory_internals.js", IDR_MEMORY_INTERNALS_JS);
  return source;
}

class MemoryInternalsDOMHandler : public content::WebUIMessageHandler {
 public:
  MemoryInternalsDOMHandler();
  ~MemoryInternalsDOMHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "requestProcessList" message.
  void HandleRequestProcessList(const base::ListValue* args);

  // Callback for the "dumpProcess" message.
  void HandleDumpProcess(const base::ListValue* args);

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryInternalsDOMHandler);
};

MemoryInternalsDOMHandler::MemoryInternalsDOMHandler() {}

MemoryInternalsDOMHandler::~MemoryInternalsDOMHandler() {}

void MemoryInternalsDOMHandler::RegisterMessages() {
  // Unretained should be OK here since this class is bound to the lifetime of
  // the WebUI.
  web_ui()->RegisterMessageCallback(
      "requestProcessList",
      base::Bind(&MemoryInternalsDOMHandler::HandleRequestProcessList,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "dumpProcess", base::Bind(&MemoryInternalsDOMHandler::HandleDumpProcess,
                                base::Unretained(this)));
}

void MemoryInternalsDOMHandler::HandleRequestProcessList(
    const base::ListValue* args) {
  base::Value result(base::Value::Type::LIST);
  // Return some canned data for now.
  // TODO(brettW) hook this up.
  result.GetList().push_back(MakeProcessInfo(11234, "Process 11234 [Browser]"));
  result.GetList().push_back(MakeProcessInfo(11235, "Process 11235 [GPU]"));
  result.GetList().push_back(MakeProcessInfo(11236, "Process 11236 [Rend]"));

  AllowJavascript();
  CallJavascriptFunction("returnProcessList", result);
  DisallowJavascript();
}

void MemoryInternalsDOMHandler::HandleDumpProcess(const base::ListValue* args) {
  // TODO(brettW) hook this up.
}

}  // namespace

MemoryInternalsUI::MemoryInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<MemoryInternalsDOMHandler>());

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateMemoryInternalsUIHTMLSource());
}

MemoryInternalsUI::~MemoryInternalsUI() {}
