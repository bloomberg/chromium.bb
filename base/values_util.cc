// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values_util.h"

RefCountedList::RefCountedList(ListValue* list) {
  list_ = list;
}

RefCountedList::~RefCountedList() {
  if (list_)
    delete list_;
}
