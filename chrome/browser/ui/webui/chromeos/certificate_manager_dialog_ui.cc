// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/certificate_manager_dialog_ui.h"

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/options/certificate_manager_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_constants.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

const char kLocalizedStringsFile[] = "strings.js";

class CertificateManagerDialogHTMLSource : public content::URLDataSource {
 public:
  explicit CertificateManagerDialogHTMLSource(
      base::DictionaryValue* localized_strings);

  // content::URLDataSource implementation.
  virtual std::string GetSource() const OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string&) const OVERRIDE {
    return "text/html";
  }
  virtual bool ShouldAddContentSecurityPolicy() const OVERRIDE {
    return false;
  }

 protected:
  virtual ~CertificateManagerDialogHTMLSource() {}

 private:
  scoped_ptr<base::DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(CertificateManagerDialogHTMLSource);
};

CertificateManagerDialogHTMLSource::CertificateManagerDialogHTMLSource(
    base::DictionaryValue* localized_strings)
    : localized_strings_(localized_strings) {
}

std::string CertificateManagerDialogHTMLSource::GetSource() const {
  return chrome::kChromeUICertificateManagerHost;
}

void CertificateManagerDialogHTMLSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  scoped_refptr<base::RefCountedMemory> response_bytes;
  webui::SetFontAndTextDirection(localized_strings_.get());

  if (path == kLocalizedStringsFile) {
    // Return dynamically-generated strings from memory.
    webui::UseVersion2 version;
    std::string strings_js;
    webui::AppendJsonJS(localized_strings_.get(), &strings_js);
    response_bytes = base::RefCountedString::TakeString(&strings_js);
  } else {
    // Return (and cache) the main options html page as the default.
    response_bytes = ui::ResourceBundle::GetSharedInstance().
        LoadDataResourceBytes(IDR_CERT_MANAGER_DIALOG_HTML);
  }

  callback.Run(response_bytes.get());
}

}  // namespace

namespace chromeos {

CertificateManagerDialogUI::CertificateManagerDialogUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui),
      initialized_handlers_(false),
      cert_handler_(new ::options::CertificateManagerHandler(true)),
      core_handler_(new options::CoreChromeOSOptionsHandler()) {
  // |localized_strings| will be owned by CertificateManagerDialogHTMLSource.
  base::DictionaryValue* localized_strings = new base::DictionaryValue();

  web_ui->AddMessageHandler(core_handler_);
  core_handler_->set_handlers_host(this);
  core_handler_->GetLocalizedValues(localized_strings);

  web_ui->AddMessageHandler(cert_handler_);
  cert_handler_->GetLocalizedValues(localized_strings);

  bool keyboard_driven_oobe =
      system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation();
  localized_strings->SetString("highlightStrength",
                               keyboard_driven_oobe ? "strong" : "normal");

  CertificateManagerDialogHTMLSource* source =
      new CertificateManagerDialogHTMLSource(localized_strings);
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, source);
}

CertificateManagerDialogUI::~CertificateManagerDialogUI() {
  // Uninitialize all registered handlers. The base class owns them and it will
  // eventually delete them.
  core_handler_->Uninitialize();
  cert_handler_->Uninitialize();
}

void CertificateManagerDialogUI::InitializeHandlers() {
  // A new web page DOM has been brought up in an existing renderer, causing
  // this method to be called twice. In that case, don't initialize the handlers
  // again. Compare with options_ui.cc.
  if (!initialized_handlers_) {
    core_handler_->InitializeHandler();
    cert_handler_->InitializeHandler();
    initialized_handlers_ = true;
  }
  core_handler_->InitializePage();
  cert_handler_->InitializePage();
}

}  // namespace chromeos
