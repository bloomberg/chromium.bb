// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/environment/environment.h"

#include "mojo/common/environment_data.h"

namespace mojo {

class Environment::Data {
 public:
  Data();
  ~Data();

 private:
  common::EnvironmentData data_;

  DISALLOW_COPY_AND_ASSIGN(Data);
};

Environment::Data::Data() {
}

Environment::Data::~Data() {
}

Environment::Environment() : data_(new Environment::Data) {
}

Environment::~Environment() {
  delete data_;
}

}  // namespace mojo
