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
#include "mojo/shell/public/cpp/application_delegate.h"

namespace mojo {
class ApplicationConnection;
}

namespace mash {
namespace browser_driver {

class BrowserDriverApplicationDelegate : public mojo::ApplicationDelegate,
                                         public mus::mojom::AcceleratorHandler {
 public:
  BrowserDriverApplicationDelegate();
  ~BrowserDriverApplicationDelegate() override;

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // mus::mojom::AcceleratorHandler:
  void OnAccelerator(uint32_t id, mus::mojom::EventPtr event) override;

  void AddAccelerators();

  mojo::ApplicationImpl* app_;
  mojo::Binding<mus::mojom::AcceleratorHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(BrowserDriverApplicationDelegate);
};

}  // namespace browser_driver
}  // namespace mash

#endif  // MASH_BROWSER_DRIVER_BROWSER_DRIVER_APPLICATION_DELEGATE_H_
