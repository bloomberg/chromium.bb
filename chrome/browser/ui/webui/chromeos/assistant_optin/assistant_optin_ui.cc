// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_arc_home_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/value_prop_screen_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

namespace {

bool is_active = false;

constexpr int kAssistantOptInDialogWidth = 576;
constexpr int kAssistantOptInDialogHeight = 480;

}  // namespace

AssistantOptInUI::AssistantOptInUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui), weak_factory_(this) {
  // Set up the chrome://assistant-optin source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAssistantOptInHost);

  AddScreenHandler(std::make_unique<ValuePropScreenHandler>(
      base::BindOnce(&AssistantOptInUI::OnExit, weak_factory_.GetWeakPtr())));

  base::DictionaryValue localized_strings;
  for (auto* handler : screen_handlers_)
    handler->GetLocalizedStrings(&localized_strings);
  source->AddLocalizedStrings(localized_strings);

  source->SetJsonPath("strings.js");

  source->AddResourcePath("assistant_optin.js", IDR_ASSISTANT_OPTIN_JS);
  source->SetDefaultResource(IDR_ASSISTANT_OPTIN_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

AssistantOptInUI::~AssistantOptInUI() {}

void AssistantOptInUI::AddScreenHandler(
    std::unique_ptr<BaseWebUIHandler> handler) {
  screen_handlers_.push_back(handler.get());
  web_ui()->AddMessageHandler(std::move(handler));
}

void AssistantOptInUI::OnExit(AssistantOptInScreenExitCode exit_code) {
  switch (exit_code) {
    case AssistantOptInScreenExitCode::VALUE_PROP_SKIPPED:
      // TODO(updowndota) Update the action to use the new Assistant service.
      GetVoiceInteractionHomeService()->OnAssistantCanceled();
      CloseDialog(nullptr);
      break;
    case AssistantOptInScreenExitCode::VALUE_PROP_ACCEPTED:
      // TODO(updowndota) Update the action to use the new Assistant service.
      GetVoiceInteractionHomeService()->OnAssistantAppRequested();
      CloseDialog(nullptr);
      break;
    default:
      NOTREACHED();
  }
}

arc::ArcVoiceInteractionArcHomeService*
AssistantOptInUI::GetVoiceInteractionHomeService() {
  Profile* const profile = Profile::FromWebUI(web_ui());
  arc::ArcVoiceInteractionArcHomeService* const home_service =
      arc::ArcVoiceInteractionArcHomeService::GetForBrowserContext(profile);
  DCHECK(home_service);
  return home_service;
}

// AssistantOptInDialog

// static
void AssistantOptInDialog::Show() {
  DCHECK(!is_active);
  AssistantOptInDialog* dialog = new AssistantOptInDialog();
  dialog->ShowSystemDialog();
}

// static
bool AssistantOptInDialog::IsActive() {
  return is_active;
}

AssistantOptInDialog::AssistantOptInDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIAssistantOptInURL),
                              base::string16()) {
  DCHECK(!is_active);
  is_active = true;
}

AssistantOptInDialog::~AssistantOptInDialog() {
  is_active = false;
}

void AssistantOptInDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kAssistantOptInDialogWidth, kAssistantOptInDialogHeight);
}

std::string AssistantOptInDialog::GetDialogArgs() const {
  return std::string();
}

bool AssistantOptInDialog::ShouldShowDialogTitle() const {
  return false;
}

}  // namespace chromeos
