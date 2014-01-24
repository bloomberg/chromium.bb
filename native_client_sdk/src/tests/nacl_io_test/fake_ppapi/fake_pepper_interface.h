// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PEPPER_INTERFACE_H_
#define TESTS_NACL_IO_TEST_FAKE_PEPPER_INTERFACE_H_

#include "fake_ppapi/fake_core_interface.h"
#include "fake_ppapi/fake_messaging_interface.h"
#include "fake_ppapi/fake_var_array_buffer_interface.h"
#include "fake_ppapi/fake_var_array_interface.h"
#include "fake_ppapi/fake_var_interface.h"
#include "fake_ppapi/fake_var_manager.h"
#include "nacl_io/pepper_interface_dummy.h"

class FakePepperInterface : public nacl_io::PepperInterfaceDummy {
 public:
  FakePepperInterface();
  virtual ~FakePepperInterface() {}

  virtual nacl_io::CoreInterface* GetCoreInterface();
  virtual nacl_io::MessagingInterface* GetMessagingInterface();
  virtual nacl_io::VarArrayInterface* GetVarArrayInterface();
  virtual nacl_io::VarArrayBufferInterface* GetVarArrayBufferInterface();
  virtual nacl_io::VarInterface* GetVarInterface();

 private:
  FakeVarManager var_manager_;
  FakeCoreInterface core_interface_;
  FakeMessagingInterface messaging_interface_;
  FakeVarArrayInterface var_array_interface_;
  FakeVarArrayBufferInterface var_array_buffer_interface_;
  FakeVarInterface var_interface_;

  DISALLOW_COPY_AND_ASSIGN(FakePepperInterface);
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PEPPER_INTERFACE_H_
