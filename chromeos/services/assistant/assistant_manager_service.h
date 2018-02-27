// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_

#include <memory>
#include <string>

namespace chromeos {
namespace assistant {

// Interface class that defines all assistant functionalities.
class AssistantManagerService {
 public:
  virtual ~AssistantManagerService() = default;

  // Start the assistant in the background with |token|.
  virtual void Start(const std::string& access_token) = 0;

  // Set access token for assistant.
  virtual void SetAccessToken(const std::string& access_token) = 0;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_
