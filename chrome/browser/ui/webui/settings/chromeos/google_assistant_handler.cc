// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/google_assistant_handler.h"

#include <utility>

#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/arc_service_manager.h"
#include "content/public/browser/browser_context.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {
namespace settings {

GoogleAssistantHandler::GoogleAssistantHandler(Profile* profile)
    : profile_(profile) {}

GoogleAssistantHandler::~GoogleAssistantHandler() {}

void GoogleAssistantHandler::OnJavascriptAllowed() {}
void GoogleAssistantHandler::OnJavascriptDisallowed() {}

void GoogleAssistantHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showGoogleAssistantSettings",
      base::BindRepeating(
          &GoogleAssistantHandler::HandleShowGoogleAssistantSettings,
          base::Unretained(this)));
}

void GoogleAssistantHandler::HandleShowGoogleAssistantSettings(
    const base::ListValue* args) {
  // Opens Google Assistant settings.
  service_manager::Connector* connector =
      content::BrowserContext::GetConnectorFor(profile_);
  ash::mojom::AssistantControllerPtr assistant_controller;
  connector->BindInterface(ash::mojom::kServiceName, &assistant_controller);
  assistant_controller->OpenAssistantSettings();
}

}  // namespace settings
}  // namespace chromeos
