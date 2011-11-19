// Copyright 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "c_salt/test/gtest_instance.h"
#include "ppapi/cpp/module.h"

namespace c_salt {

// GTestModule is a NaCl module dedicated to running gtest-based unit tests.
// It creates an NaCl instance based on GTestInstance.
class GTestModule : public pp::Module {
 public:
  GTestModule() : pp::Module() {}
  ~GTestModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new c_salt::GTestInstance(instance);
  }
};

}  // namespace c_salt

namespace pp {
Module* CreateModule() {
  return new c_salt::GTestModule();
}
}  // namespace pp

