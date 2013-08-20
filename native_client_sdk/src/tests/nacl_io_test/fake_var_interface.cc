// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_var_interface.h"
#include "gtest/gtest.h"

FakeVarInterface::FakeVarInterface() : next_id_(1) {}

FakeVarInterface::~FakeVarInterface() {
  // The ref counts for all vars should be zero.
  for (VarMap::const_iterator iter = var_map_.begin(); iter != var_map_.end();
       ++iter) {
    const FakeStringVar& string_var = iter->second;
    EXPECT_EQ(0, string_var.ref_count) << "Non-zero refcount on string var "
                                       << iter->first << " with value \""
                                       << string_var.value << "\"";
  }
}

void FakeVarInterface::AddRef(PP_Var var) {
  // From ppb_var.h:
  //   AddRef() adds a reference to the given var. If this is not a refcounted
  //   object, this function will do nothing so you can always call it no matter
  //   what the type.
  if (var.type != PP_VARTYPE_STRING)
    return;

  VarMap::iterator iter = var_map_.find(var.value.as_id);
  if (iter == var_map_.end())
    return;

  FakeStringVar& string_var = iter->second;
  EXPECT_LT(0, string_var.ref_count) << "AddRefing freed string var "
                                     << var.value.as_id << " with value \""
                                     << string_var.value << "\"";
  string_var.ref_count++;
}

void FakeVarInterface::Release(PP_Var var) {
  // From ppb_var.h:
  //   Release() removes a reference to given var, deleting it if the internal
  //   reference count becomes 0. If the given var is not a refcounted object,
  //   this function will do nothing so you can always call it no matter what
  //   the type.
  if (var.type != PP_VARTYPE_STRING)
    return;

  VarMap::iterator iter = var_map_.find(var.value.as_id);
  if (iter == var_map_.end())
    return;

  FakeStringVar& string_var = iter->second;
  EXPECT_LT(0, string_var.ref_count) << "Releasing freed string var "
                                     << var.value.as_id << " with value \""
                                     << string_var.value << "\"";
  string_var.ref_count--;
}

PP_Var FakeVarInterface::VarFromUtf8(const char* data, uint32_t len) {
  Id id = next_id_++;

  FakeStringVar string_var;
  string_var.value.assign(data, len);
  string_var.ref_count = 1;

  var_map_[id] = string_var;

  struct PP_Var result = {PP_VARTYPE_STRING, 0, {PP_FALSE}};
  result.value.as_id = id;
  return result;
}

const char* FakeVarInterface::VarToUtf8(PP_Var var, uint32_t* out_len) {
  if (var.type != PP_VARTYPE_STRING) {
    *out_len = 0;
    return NULL;
  }

  VarMap::const_iterator iter = var_map_.find(var.value.as_id);
  if (iter == var_map_.end()) {
    *out_len = 0;
    return NULL;
  }

  const FakeStringVar& string_var = iter->second;
  EXPECT_LT(0, string_var.ref_count) << "VarToUtf8 on freed string var "
                                     << var.value.as_id << " with value \""
                                     << string_var.value << "\"";

  *out_len = string_var.value.length();
  return string_var.value.c_str();
}
