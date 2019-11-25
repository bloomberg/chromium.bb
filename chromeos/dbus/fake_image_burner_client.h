// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_IMAGE_BURNER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_IMAGE_BURNER_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "chromeos/dbus/image_burner_client.h"

namespace chromeos {

// A fake implemetation of ImageBurnerClient. This class does nothing.
class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeImageBurnerClient
    : public ImageBurnerClient {
 public:
  FakeImageBurnerClient();
  ~FakeImageBurnerClient() override;

  // ImageBurnerClient overrides
  void Init(dbus::Bus* bus) override;
  void BurnImage(const std::string& from_path,
                 const std::string& to_path,
                 ErrorCallback error_callback) override;
  void SetEventHandlers(
      const BurnFinishedHandler& burn_finished_handler,
      const BurnProgressUpdateHandler& burn_progress_update_handler) override;
  void ResetEventHandlers() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeImageBurnerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_IMAGE_BURNER_CLIENT_H_
