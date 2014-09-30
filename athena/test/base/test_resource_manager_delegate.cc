// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/resource_manager/public/resource_manager_delegate.h"

#include <string>

#include "base/macros.h"

namespace athena {

namespace test {

namespace {
// This is the minimum amount of time in milliseconds between checks for
// low memory. Since we report no memory use (and the timeout is extremely long)
// the memory pressure event will never go off.
const int kMemoryPressureIntervalMs = 60000;
}  // namespace

class ResourceManagerDelegateImpl : public ResourceManagerDelegate {
 public:
  ResourceManagerDelegateImpl() {}
  virtual ~ResourceManagerDelegateImpl() {}

 private:
  virtual int GetUsedMemoryInPercent() OVERRIDE {
    return 0;
  }

  virtual int MemoryPressureIntervalInMS() OVERRIDE {
    return kMemoryPressureIntervalMs;
  }
  DISALLOW_COPY_AND_ASSIGN(ResourceManagerDelegateImpl);
};

}  // namespace test

// static
ResourceManagerDelegate*
ResourceManagerDelegate::CreateResourceManagerDelegate() {
  return new test::ResourceManagerDelegateImpl;
}

}  // namespace athena
