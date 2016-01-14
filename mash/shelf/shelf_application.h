// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SHELF_SHELF_APPLICATION_H_
#define MASH_SHELF_SHELF_APPLICATION_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_delegate.h"

namespace views {
class AuraInit;
}

namespace mash {
namespace shelf {

class ShelfApplication : public mojo::ApplicationDelegate {
 public:
  ShelfApplication();
  ~ShelfApplication() override;

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  mojo::TracingImpl tracing_;

  scoped_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(ShelfApplication);
};

}  // namespace shelf
}  // namespace mash

#endif  // MASH_SHELF_SHELF_APPLICATION_H_
