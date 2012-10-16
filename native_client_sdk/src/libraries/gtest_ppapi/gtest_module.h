// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef GTEST_PPAPI_GTEST_MODULE_H_
#define GTEST_PPAPI_GTEST_MODULE_H_
#include "ppapi/cpp/module.h"

// GTestModule is a NaCl module dedicated to running gtest-based unit tests.
// It creates an NaCl instance based on GTestInstance.
class GTestModule : public pp::Module {
 public:
  GTestModule() : pp::Module() {}
  ~GTestModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance);
};

#endif  // GTEST_PPAPI_GTEST_MODULE_H_
