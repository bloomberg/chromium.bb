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
  // Start the assistant in the background.  In particular the assistant would
  // start listening for hotword and respond.
  virtual void Start() = 0;

 protected:
  virtual ~AssistantManagerService() = default;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_
