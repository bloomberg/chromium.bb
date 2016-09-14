// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_TEST_COMPOSITOR_MOCK_COMPOSITOR_DEPENDENCIES_H_
#define BLIMP_CLIENT_TEST_COMPOSITOR_MOCK_COMPOSITOR_DEPENDENCIES_H_

#include "blimp/client/support/compositor/compositor_dependencies_impl.h"

namespace blimp {
namespace client {

class MockCompositorDependencies : public CompositorDependenciesImpl {
 public:
  MockCompositorDependencies() = default;
  ~MockCompositorDependencies() override = default;

  // CompositorDependenciesImpl implementation.
  void GetContextProviders(const ContextProviderCallback& callback) override;
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_TEST_COMPOSITOR_MOCK_COMPOSITOR_DEPENDENCIES_H_
