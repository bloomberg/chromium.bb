// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_WINDOW_TYPE_LAUNCHER_WINDOW_TYPE_LAUNCHER_H_
#define COMPONENTS_MUS_EXAMPLE_WINDOW_TYPE_LAUNCHER_WINDOW_TYPE_LAUNCHER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/application/public/cpp/application_delegate.h"

class WindowTypeLauncher : public mojo::ApplicationDelegate {
 public:
  WindowTypeLauncher();
  ~WindowTypeLauncher() override;

 private:
  // ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeLauncher);
};

#endif  // COMPONENTS_MUS_EXAMPLE_WINDOW_TYPE_LAUNCHER_WINDOW_TYPE_LAUNCHER_H_
