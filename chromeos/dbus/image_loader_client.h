// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IMAGE_LOADER_CLIENT_H_
#define CHROMEOS_DBUS_IMAGE_LOADER_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

// ImageLoaderClient is used to communicate with the ImageLoader service, which
// registers and loads component updates on Chrome OS.
class CHROMEOS_EXPORT ImageLoaderClient : public DBusClient {
 public:
  ~ImageLoaderClient() override;

  // Registers a component by copying from |component_folder_abs_path| into its
  // internal storage, if and only if, the component passes verification.
  virtual void RegisterComponent(const std::string& name,
                                 const std::string& version,
                                 const std::string& component_folder_abs_path,
                                 const BoolDBusMethodCallback& callback) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static ImageLoaderClient* Create();

 protected:
  // Create() should be used instead.
  ImageLoaderClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageLoaderClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IMAGE_LOADER_CLIENT_H_
