// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SPY_SPY_H_
#define MOJO_SPY_SPY_H_

#include <string>

namespace mojo {

class ServiceManager;

class Spy {
 public:
  Spy(mojo::ServiceManager* service_manager, const std::string& options);
  ~Spy();
};

}  // namespace mojo

#endif  // MOJO_SPY_SPY_H_
