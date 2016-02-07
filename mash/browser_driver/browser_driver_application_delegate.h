// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_BROWSER_DRIVER_BROWSER_DRIVER_APPLICATION_DELEGATE_H_
#define MASH_BROWSER_DRIVER_BROWSER_DRIVER_APPLICATION_DELEGATE_H_

#include <stdint.h>

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/accelerator_registrar.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace mash {
namespace browser_driver {

class BrowserDriverApplicationDelegate : public mojo::ShellClient,
                                         public mus::mojom::AcceleratorHandler {
 public:
  BrowserDriverApplicationDelegate();
  ~BrowserDriverApplicationDelegate() override;

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  // mus::mojom::AcceleratorHandler:
  void OnAccelerator(uint32_t id, mus::mojom::EventPtr event) override;

  void AddAccelerators();

  mojo::Shell* shell_;
  mojo::Binding<mus::mojom::AcceleratorHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(BrowserDriverApplicationDelegate);
};

}  // namespace browser_driver
}  // namespace mash

#endif  // MASH_BROWSER_DRIVER_BROWSER_DRIVER_APPLICATION_DELEGATE_H_
