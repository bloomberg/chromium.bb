// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_MOCK_SYSUI_MOCK_SYSUI_H_
#define COMPONENTS_MUS_EXAMPLE_MOCK_SYSUI_MOCK_SYSUI_H_

#include "base/macros.h"
#include "mojo/application/public/cpp/application_delegate.h"

class MockSysUI : public mojo::ApplicationDelegate {
 public:
  MockSysUI();
  ~MockSysUI() override;

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  DISALLOW_COPY_AND_ASSIGN(MockSysUI);
};

#endif  // COMPONENTS_MUS_EXAMPLE_MOCK_SYSUI_MOCK_SYSUI_H_
