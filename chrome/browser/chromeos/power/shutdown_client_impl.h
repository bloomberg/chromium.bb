// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_SHUTDOWN_CLIENT_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_SHUTDOWN_CLIENT_IMPL_H_

#include "ash/public/interfaces/shutdown.mojom.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace chromeos {

class ShutdownClientImpl : public ash::mojom::ShutdownClient {
 public:
  ShutdownClientImpl();
  ~ShutdownClientImpl() override;

 private:
  // ash::mojom::ShutdownClient implementation.
  void RequestShutdown() override;

  base::WeakPtrFactory<ShutdownClientImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownClientImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_SHUTDOWN_CLIENT_IMPL_H_
