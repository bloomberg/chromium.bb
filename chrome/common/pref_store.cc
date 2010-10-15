// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_store.h"
#include "base/values.h"

Value* PrefStore::CreateUseDefaultSentinelValue() {
  return Value::CreateNullValue();
}

bool PrefStore::IsUseDefaultSentinelValue(Value* value) {
  return value->IsType(Value::TYPE_NULL);
}
