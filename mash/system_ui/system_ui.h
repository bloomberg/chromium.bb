// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SYSTEM_UI_SYSTEM_UI_H_
#define MASH_SYSTEM_UI_SYSTEM_UI_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"

namespace views {
class AuraInit;
}

namespace mash {
namespace system_ui {

class SystemUI : public mojo::ApplicationDelegate {
 public:
  SystemUI();
  ~SystemUI() override;

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  mojo::TracingImpl tracing_;

  scoped_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(SystemUI);
};

}  // namespace system_ui
}  // namespace mash

#endif  // MASH_SYSTEM_UI_SYSTEM_UI_H_
