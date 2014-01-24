// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_pepper_interface.h"

using namespace nacl_io;

FakePepperInterface::FakePepperInterface()
   : messaging_interface_(&var_manager_, &var_interface_),
     var_array_interface_(&var_manager_),
     var_array_buffer_interface_(&var_manager_),
     var_interface_(&var_manager_) {}

CoreInterface* FakePepperInterface::GetCoreInterface() {
  return &core_interface_;
}

VarArrayInterface* FakePepperInterface::GetVarArrayInterface() {
  return &var_array_interface_;
}

VarArrayBufferInterface* FakePepperInterface::GetVarArrayBufferInterface() {
  return &var_array_buffer_interface_;
}

VarInterface* FakePepperInterface::GetVarInterface() {
  return &var_interface_;
}

MessagingInterface* FakePepperInterface::GetMessagingInterface() {
  return &messaging_interface_;
}
