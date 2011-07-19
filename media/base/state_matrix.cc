// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/state_matrix.h"

#include "base/stl_util.h"

namespace media {

StateMatrix::StateMatrix() {
}

StateMatrix::~StateMatrix() {
  STLDeleteValues(&states_);
}

bool StateMatrix::IsStateDefined(int state) {
  return states_.find(state) != states_.end();
}

int StateMatrix::ExecuteHandler(void* instance, int state) {
  return states_.find(state)->second->ExecuteHandler(instance);
}

}  // namespace media
