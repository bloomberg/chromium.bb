// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/interface.h"

namespace mojo {

bool NoInterfaceStub::Accept(Message* message) {
  return false;
}

bool NoInterfaceStub::AcceptWithResponder(Message* message,
                                          MessageReceiver* responder) {
  return false;
}

}  // namespace mojo

