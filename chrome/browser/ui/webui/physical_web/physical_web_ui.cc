// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/physical_web/physical_web_ui.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/grit/components_resources.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "components/physical_web/webui/physical_web_base_message_handler.h"
#include "components/physical_web/webui/physical_web_ui_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

content::WebUIDataSource* CreatePhysicalWebHTMLSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIPhysicalWebHost);

  source->AddLocalizedString(physical_web_ui::kTitle,
                             IDS_PHYSICAL_WEB_UI_TITLE);
  source->SetJsonPath("strings.js");
  source->AddResourcePath(physical_web_ui::kPhysicalWebJS,
                          IDR_PHYSICAL_WEB_UI_JS);
  source->AddResourcePath(physical_web_ui::kPhysicalWebCSS,
                          IDR_PHYSICAL_WEB_UI_CSS);
  source->SetDefaultResource(IDR_PHYSICAL_WEB_UI_HTML);
  return source;
}

// Implements all MessageHandler core functionality.  This is extends the
// PhysicalWebBaseMessageHandler and implements functions to manipulate an
// Android-specific WebUI object.
class MessageHandlerImpl
    : public physical_web_ui::PhysicalWebBaseMessageHandler {
 public:
  explicit MessageHandlerImpl(content::WebUI* web_ui) : web_ui_(web_ui) {}
  ~MessageHandlerImpl() override {}

 private:
  void RegisterMessageCallback(
      const std::string& message,
      const physical_web_ui::MessageCallback& callback) override {
    web_ui_->RegisterMessageCallback(message, callback);
  }
  void CallJavaScriptFunction(const std::string& function,
                              const base::Value& arg) override {
    web_ui_->CallJavascriptFunctionUnsafe(function, arg);
  }
  physical_web::PhysicalWebDataSource* GetPhysicalWebDataSource() override {
    return g_browser_process->GetPhysicalWebDataSource();
  }

  content::WebUI* const web_ui_;
  DISALLOW_COPY_AND_ASSIGN(MessageHandlerImpl);
};

class PhysicalWebMessageHandler : public content::WebUIMessageHandler {
 public:
  PhysicalWebMessageHandler() {}
  ~PhysicalWebMessageHandler() override {}

  void RegisterMessages() override {
    impl_.reset(new MessageHandlerImpl(web_ui()));
    impl_->RegisterMessages();
  }

 private:
  std::unique_ptr<MessageHandlerImpl> impl_;
  DISALLOW_COPY_AND_ASSIGN(PhysicalWebMessageHandler);
};

}  // namespace

PhysicalWebUI::PhysicalWebUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreatePhysicalWebHTMLSource());
  web_ui->AddMessageHandler(base::MakeUnique<PhysicalWebMessageHandler>());
  base::RecordAction(base::UserMetricsAction("PhysicalWeb.WebUI.Open"));
}

PhysicalWebUI::~PhysicalWebUI() = default;
