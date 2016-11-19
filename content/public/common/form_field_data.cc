// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/form_field_data.h"

namespace content {

FormFieldData::FormFieldData() : text_input_type(ui::TEXT_INPUT_TYPE_NONE) {}

FormFieldData::FormFieldData(const FormFieldData& other) = default;

FormFieldData::~FormFieldData() = default;

}  // namespace content
