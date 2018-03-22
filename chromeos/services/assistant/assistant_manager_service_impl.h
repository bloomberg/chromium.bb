// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_

#include <memory>
#include <string>

// TODO(xiaohuic): replace with "base/macros.h" once we remove
// libassistant/contrib dependency.
#include "chromeos/assistant/internal/action/cros_action_module.h"
#include "chromeos/assistant/internal/cros_display_connection.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/platform_api_impl.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "libassistant/contrib/core/macros.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

// Implementation of AssistantManagerService based on libassistant.
class AssistantManagerServiceImpl
    : public AssistantManagerService,
      public ::chromeos::assistant::action::AssistantActionObserver,
      public AssistantEventObserver {
 public:
  explicit AssistantManagerServiceImpl(mojom::AudioInputPtr audio_input);
  ~AssistantManagerServiceImpl() override;

  // assistant::AssistantManagerService overrides
  void Start(const std::string& access_token) override;
  bool IsRunning() const override;
  void SetAccessToken(const std::string& access_token) override;
  void EnableListening(bool enable) override;

  // mojom::Assistant overrides:
  void SendTextQuery(const std::string& query) override;
  void AddAssistantEventSubscriber(
      mojom::AssistantEventSubscriberPtr subscriber) override;

  // AssistantActionObserver overrides:
  void OnShowHtml(const std::string& html) override;
  void OnShowText(const std::string& text) override;
  void OnOpenUrl(const std::string& url) override;

  // AssistantEventObserver overrides:
  void onSpeechLevelUpdated(float speech_level) override;

 private:
  bool running_ = false;
  PlatformApiImpl platform_api_;
  std::unique_ptr<action::CrosActionModule> action_module_;
  std::unique_ptr<assistant_client::AssistantManager> assistant_manager_;
  assistant_client::AssistantManagerInternal* const assistant_manager_internal_;
  std::unique_ptr<CrosDisplayConnection> display_connection_;
  mojo::InterfacePtrSet<mojom::AssistantEventSubscriber> subscribers_;

  void StartAssistantInternal(const std::string& access_token,
                              const std::string& arc_version);
  std::string BuildUserAgent(const std::string& arc_version) const;

  DISALLOW_COPY_AND_ASSIGN(AssistantManagerServiceImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_
