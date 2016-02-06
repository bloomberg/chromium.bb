// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_EXAMPLE_WINDOW_TYPE_LAUNCHER_WINDOW_TYPE_LAUNCHER_H_
#define MASH_EXAMPLE_WINDOW_TYPE_LAUNCHER_WINDOW_TYPE_LAUNCHER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/shell/public/cpp/application_delegate.h"

namespace views {
class AuraInit;
}

class WindowTypeLauncher : public mojo::ApplicationDelegate {
 public:
  WindowTypeLauncher();
  ~WindowTypeLauncher() override;

 private:
  // ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool AcceptConnection(
      mojo::ApplicationConnection* connection) override;

  scoped_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeLauncher);
};

#endif  // MASH_EXAMPLE_WINDOW_TYPE_LAUNCHER_WINDOW_TYPE_LAUNCHER_H_
