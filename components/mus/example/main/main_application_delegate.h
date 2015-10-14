// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_MAIN_MAIN_APPLICATION_DELEGATE_H_
#define COMPONENTS_MUS_EXAMPLE_MAIN_MAIN_APPLICATION_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"

namespace mojo {
class ApplicationConnection;
}

class MainApplicationDelegate : public mojo::ApplicationDelegate {
 public:
  MainApplicationDelegate();
  ~MainApplicationDelegate() override;

 private:
  // ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  ScopedVector<mojo::ApplicationConnection> connections_;

  DISALLOW_COPY_AND_ASSIGN(MainApplicationDelegate);
};

#endif  // COMPONENTS_MUS_EXAMPLE_MAIN_MAIN_APPLICATION_DELEGATE_H_
