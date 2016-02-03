// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SCREENLOCK_SCREENLOCK_H_
#define MASH_SCREENLOCK_SCREENLOCK_H_

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mash/shell/public/interfaces/shell.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_delegate.h"

namespace views {
class AuraInit;
}

namespace mash {
namespace screenlock {

class Screenlock : public mojo::ApplicationDelegate,
                   public shell::mojom::ScreenlockStateListener {
 public:
  Screenlock();
  ~Screenlock() override;

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;

  // mash::shell::mojom::ScreenlockStateListener:
  void ScreenlockStateChanged(bool locked) override;

  mojo::ApplicationImpl* app_;
  mojo::TracingImpl tracing_;
  scoped_ptr<views::AuraInit> aura_init_;
  mojo::WeakBindingSet<mash::shell::mojom::ScreenlockStateListener> bindings_;

  DISALLOW_COPY_AND_ASSIGN(Screenlock);
};

}  // namespace screenlock
}  // namespace mash

#endif  // MASH_SCREENLOCK_SCREENLOCK_H_
