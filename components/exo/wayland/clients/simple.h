// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_SIMPLE_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_SIMPLE_H_

#include "components/exo/wayland/clients/client_base.h"

namespace exo {
namespace wayland {
namespace clients {

class Simple : public wayland::clients::ClientBase {
 public:
  Simple();

  void Run(int frames);

 private:
  DISALLOW_COPY_AND_ASSIGN(Simple);
};

}  // namespace clients
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_SIMPLE_H_
