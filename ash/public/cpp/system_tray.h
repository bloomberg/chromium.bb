// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TRAY_H_
#define ASH_PUBLIC_CPP_SYSTEM_TRAY_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

class SystemTrayClient;

// Public interface to control the system tray bubble in ash.
class ASH_PUBLIC_EXPORT SystemTray {
 public:
  static SystemTray* Get();

  // Sets the client interface in the browser.
  virtual void SetClient(SystemTrayClient* client) = 0;

  // TODO(jamescook): Migrate ash::mojom::SystemTray here.

 protected:
  SystemTray();
  virtual ~SystemTray();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TRAY_H_
