// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_image_loader_client.h"

namespace chromeos {

// ImageLoaderClient override:
void FakeImageLoaderClient::RegisterComponent(
    const std::string& name,
    const std::string& version,
    const std::string& component_folder_abs_path,
    const BoolDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_FAILURE, false);
}

}  // namespace chromeos
