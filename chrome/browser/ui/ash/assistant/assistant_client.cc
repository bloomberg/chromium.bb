// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_client.h"

#include <utility>

#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "chrome/browser/chromeos/arc/voice_interaction/voice_interaction_controller_client.h"
#include "chrome/browser/chromeos/assistant/assistant_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/assistant/assistant_context_util.h"
#include "chrome/browser/ui/ash/assistant/assistant_image_downloader.h"
#include "chrome/browser/ui/ash/assistant/assistant_setup.h"
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

// Owned by ChromeBrowserMainChromeOS:
AssistantClient* g_instance = nullptr;

bool IsAssistantAllowed(Profile* profile) {
  return assistant::IsAssistantAllowedForProfile(profile) ==
         ash::mojom::AssistantAllowedState::ALLOWED;
}

}  // namespace

// static
AssistantClient* AssistantClient::Get() {
  DCHECK(g_instance);
  return g_instance;
}

AssistantClient::AssistantClient() : client_binding_(this) {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AssistantClient::~AssistantClient() {
  DCHECK(g_instance);
  g_instance = nullptr;

  if (identity_manager_)
    identity_manager_->RemoveObserver(this);
}

void AssistantClient::MaybeInit(Profile* profile) {
  if (!IsAssistantAllowed(profile))
    return;

  if (!profile_) {
    profile_ = profile;
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
    DCHECK(identity_manager_);
    identity_manager_->AddObserver(this);
  }
  DCHECK_EQ(profile_, profile);

  if (initialized_)
    return;

  initialized_ = true;
  auto* connector = content::BrowserContext::GetConnectorFor(profile);
  connector->BindInterface(chromeos::assistant::mojom::kServiceName,
                           &assistant_connection_);

  chromeos::assistant::mojom::ClientPtr client_ptr;
  client_binding_.Bind(mojo::MakeRequest(&client_ptr));

  bool is_test = base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kBrowserTest);
  assistant_connection_->Init(std::move(client_ptr),
                              device_actions_.AddBinding(), is_test);

  assistant_image_downloader_ = std::make_unique<AssistantImageDownloader>();
  assistant_setup_ = std::make_unique<AssistantSetup>(connector);
}

void AssistantClient::MaybeStartAssistantOptInFlow() {
  // We need to check if the Assistant is allowed here again, since the result
  // might have changed from the last time we checked. This usually happens if
  // any post-OOBE screen changes any property that determines Assistant
  // availability.
  if (!initialized_ || !IsAssistantAllowed(profile_))
    return;

  assistant_setup_->MaybeStartAssistantOptInFlow();
}

void AssistantClient::OnAssistantStatusChanged(bool running) {
  // |running| means assistant mojom service is running. This maps to
  // |STOPPED| and |NOT_READY|. |RUNNING| maps to UI is shown and an assistant
  // session is running.
  arc::VoiceInteractionControllerClient::Get()->NotifyStatusChanged(
      running ? ash::mojom::VoiceInteractionState::STOPPED
              : ash::mojom::VoiceInteractionState::NOT_READY);
}

void AssistantClient::RequestAssistantStructure(
    RequestAssistantStructureCallback callback) {
  RequestAssistantStructureForActiveBrowserWindow(std::move(callback));
}

void AssistantClient::OnExtendedAccountInfoUpdated(const AccountInfo& info) {
  if (initialized_)
    return;

  MaybeInit(profile_);
}
