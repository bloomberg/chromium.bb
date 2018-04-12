// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "chromeos/services/assistant/assistant_settings_manager.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"

namespace chromeos {
namespace assistant {

// Interface class that defines all assistant functionalities.
class AssistantManagerService : public mojom::Assistant {
 public:
  ~AssistantManagerService() override = default;

  // Start the assistant in the background with |token|.
  virtual void Start(const std::string& access_token) = 0;

  // Returns whether assistant is running.
  virtual bool IsRunning() const = 0;

  // Set access token for assistant.
  virtual void SetAccessToken(const std::string& access_token) = 0;

  // Turn on / off hotword listening.
  virtual void EnableListening(bool enable) = 0;

  // Returns a pointer of AssistantSettingsManager.
  virtual AssistantSettingsManager* GetAssistantSettingsManager() = 0;

  using GetSettingsUiResponseCallback =
      base::RepeatingCallback<void(const std::string&)>;
  // Send request for getting settings ui.
  virtual void SendGetSettingsUiRequest(
      const std::string& selector,
      GetSettingsUiResponseCallback callback) = 0;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_
