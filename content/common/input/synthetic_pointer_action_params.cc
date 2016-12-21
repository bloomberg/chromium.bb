// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_pointer_action_params.h"

namespace content {

SyntheticPointerActionParams::SyntheticPointerActionParams()
    : pointer_action_type_(PointerActionType::NOT_INITIALIZED), index_(0) {}

SyntheticPointerActionParams::SyntheticPointerActionParams(
    PointerActionType action_type)
    : pointer_action_type_(action_type), index_(0) {}

SyntheticPointerActionParams::~SyntheticPointerActionParams() {}

}  // namespace content