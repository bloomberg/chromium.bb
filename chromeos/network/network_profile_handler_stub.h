// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_PROFILE_HANDLER_STUB_H_
#define CHROMEOS_NETWORK_NETWORK_PROFILE_HANDLER_STUB_H_

#include "chromeos/chromeos_export.h"
#include "chromeos/network/network_profile_handler.h"

namespace chromeos {

class CHROMEOS_EXPORT NetworkProfileHandlerStub
    : public NetworkProfileHandler {
 public:
  using NetworkProfileHandler::AddProfile;
  using NetworkProfileHandler::RemoveProfile;
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_PROFILE_HANDLER_STUB_H_
