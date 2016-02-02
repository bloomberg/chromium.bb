// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SCREENLOCK_SCREENLOCK_H_
#define MASH_SCREENLOCK_SCREENLOCK_H_

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mash/screenlock/public/interfaces/screenlock.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_delegate.h"

namespace views {
class AuraInit;
}

namespace mash {
namespace screenlock {

class Screenlock
    : public mojo::ApplicationDelegate,
      public mash::screenlock::mojom::Screenlock,
      public mojo::InterfaceFactory<mash::screenlock::mojom::Screenlock> {
 public:
  Screenlock();
  ~Screenlock() override;

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // mash::screenlock::mojom::Screenlock:
  void Quit() override;

  // mojo::InterfaceFactory<mash::screenlock::mojom::Screenlock>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mash::screenlock::mojom::Screenlock> r) override;

  mojo::ApplicationImpl* app_;
  mojo::TracingImpl tracing_;
  scoped_ptr<views::AuraInit> aura_init_;
  mojo::WeakBindingSet<mash::screenlock::mojom::Screenlock> bindings_;

  DISALLOW_COPY_AND_ASSIGN(Screenlock);
};

}  // namespace screenlock
}  // namespace mash

#endif  // MASH_SCREENLOCK_SCREENLOCK_H_
