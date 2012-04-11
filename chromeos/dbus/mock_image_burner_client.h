// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_IMAGE_BURNER_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_IMAGE_BURNER_CLIENT_H_
#pragma once

#include <string>

#include "chromeos/dbus/image_burner_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockImageBurnerClient : public ImageBurnerClient {
 public:
  MockImageBurnerClient();
  virtual ~MockImageBurnerClient();

  MOCK_METHOD3(BurnImage, void(const std::string&,
                               const std::string&,
                               const ErrorCallback&));
  MOCK_METHOD2(SetEventHandlers, void(const BurnFinishedHandler&,
                                      const BurnProgressUpdateHandler&));
  MOCK_METHOD0(ResetEventHandlers, void());

};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_IMAGE_BURNER_CLIENT_H_
