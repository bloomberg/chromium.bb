// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_TYPE_CONVERTERS_H_
#define COMPONENTS_VIEW_MANAGER_TYPE_CONVERTERS_H_

#include "ui/mojo/ime/text_input_state.mojom.h"
#include "ui/platform_window/text_input_state.h"

namespace mojo {

template <>
struct TypeConverter<ui::TextInputType, TextInputType> {
  static ui::TextInputType Convert(const TextInputType& input);
};

template <>
struct TypeConverter<ui::TextInputState, TextInputStatePtr> {
  static ui::TextInputState Convert(const TextInputStatePtr& input);
};

}  // namespace mojo

#endif  // COMPONENTS_VIEW_MANAGER_TYPE_CONVERTERS_H_
