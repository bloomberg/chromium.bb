// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/associated_interface_provider.h"

#include <stdint.h>

#include "base/macros.h"
#include "content/common/associated_interfaces.mojom.h"
#include "mojo/public/cpp/bindings/associated_group.h"

namespace content {

class AssociatedInterfaceProviderImpl : public AssociatedInterfaceProvider {
 public:
  // Binds this to a remote mojom::AssociatedInterfaceProvider.
  explicit AssociatedInterfaceProviderImpl(
      mojom::AssociatedInterfaceProviderAssociatedPtr proxy);
  ~AssociatedInterfaceProviderImpl() override;

  // AssociatedInterfaceProvider:
  void GetInterface(const std::string& name,
                    mojo::ScopedInterfaceEndpointHandle handle) override;
  mojo::AssociatedGroup* GetAssociatedGroup() override;

 private:
  mojom::AssociatedInterfaceProviderAssociatedPtr proxy_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedInterfaceProviderImpl);
};

}  // namespace content
