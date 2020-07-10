// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_CORE_INTERFACE_H_
#define TESTS_NACL_IO_TEST_FAKE_CORE_INTERFACE_H_

#include "fake_ppapi/fake_resource_manager.h"
#include "nacl_io/pepper_interface.h"
#include "sdk_util/macros.h"

class FakeCoreInterface : public nacl_io::CoreInterface {
 public:
  explicit FakeCoreInterface(FakeResourceManager* manager);

  virtual void AddRefResource(PP_Resource handle);
  virtual void ReleaseResource(PP_Resource handle);
  virtual PP_Bool IsMainThread() { return PP_FALSE; }

  FakeResourceManager* resource_manager() { return resource_manager_; }

 private:
  FakeResourceManager* resource_manager_;  // Weak reference

  DISALLOW_COPY_AND_ASSIGN(FakeCoreInterface);
};

#endif  // TESTS_NACL_IO_TEST_FAKE_CORE_INTERFACE_H_
