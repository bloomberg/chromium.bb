// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_

#include <memory>
#include <string>

// TODO(xiaohuic): replace with "base/macros.h" once we remove
// libassistant/contrib dependency.
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/platform_api_impl.h"
#include "libassistant/contrib/core/macros.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

// Implementation of AssistantManagerService based on libassistant.
class AssistantManagerServiceImpl : public AssistantManagerService {
 public:
  class AuthTokenProvider {
   public:
    virtual const std::string GetAccessToken() = 0;

   protected:
    virtual ~AuthTokenProvider() = default;
  };

  explicit AssistantManagerServiceImpl(AuthTokenProvider* auth_token_provider);
  ~AssistantManagerServiceImpl() override;

  // assistant::AssistantManagerService overrides
  void Start() override;

 private:
  AuthTokenProvider* const auth_token_provider_;
  PlatformApiImpl platform_api_;
  std::unique_ptr<assistant_client::AssistantManager> assistant_manager_;
  assistant_client::AssistantManagerInternal* const assistant_manager_internal_;

  DISALLOW_COPY_AND_ASSIGN(AssistantManagerServiceImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_
