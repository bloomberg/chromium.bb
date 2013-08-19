// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_TEST_FAKE_VAR_INTERFACE_H_
#define LIBRARIES_NACL_IO_TEST_FAKE_VAR_INTERFACE_H_

#include <map>
#include <string>

#include <ppapi/c/pp_var.h>

#include "nacl_io/pepper_interface.h"
#include "sdk_util/macros.h"

class FakeVarInterface : public nacl_io::VarInterface {
 public:
  FakeVarInterface();
  ~FakeVarInterface();

  virtual void AddRef(PP_Var var);
  virtual void Release(PP_Var var);
  virtual PP_Var VarFromUtf8(const char* data, uint32_t len);
  virtual const char* VarToUtf8(PP_Var var, uint32_t* out_len);

 private:
  typedef uint64_t Id;

  struct FakeStringVar {
    std::string value;
    int32_t ref_count;
  };

  typedef std::map<Id, FakeStringVar> VarMap;
  Id next_id_;
  VarMap var_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeVarInterface);
};

#endif  // LIBRARIES_NACL_IO_TEST_FAKE_VAR_INTERFACE_H_
