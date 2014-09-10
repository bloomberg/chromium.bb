// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_ENV_PUBLIC_ATHENA_ENV_H_
#define ATHENA_ENV_PUBLIC_ATHENA_ENV_H_

#include "athena/athena_export.h"
#include "base/callback_forward.h"

namespace aura {
class WindowTreeHost;
}

namespace gfx {
class Insets;
}

namespace athena {

// AthenaEnv creates/shuts down the environment necessary to
// start Athena shell.
class ATHENA_EXPORT AthenaEnv {
 public:
  static void Create();
  static AthenaEnv* Get();
  static void Shutdown();

  virtual ~AthenaEnv() {}

  // Returns the single WindowTreeHost for the primary display.
  virtual aura::WindowTreeHost* GetHost() = 0;

  // Sets the insets for the primary displays's work area.
  virtual void SetDisplayWorkAreaInsets(const gfx::Insets& insets) = 0;

  // Adds the callback called when the athena is about to exit.
  virtual void AddTerminatingCallback(const base::Closure& closure) = 0;
  virtual void RemoveTerminatingCallback(const base::Closure& closure) = 0;

  // Called when the athena is about to exist.
  virtual void OnTerminating() = 0;
};

}  // namespace athena

#endif  // ATHENA_ENV_PUBLIC_ATHENA_ENV_H_
