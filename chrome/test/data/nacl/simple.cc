// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

class SimpleInstance : public pp::Instance {
 public:
  explicit SimpleInstance(PP_Instance instance) : pp::Instance(instance) {
  }
};

class SimpleModule : public pp::Module {
 public:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new SimpleInstance(instance);
  }
};

namespace pp {

Module* CreateModule() {
  return new SimpleModule();
}

}  // namespace pp
