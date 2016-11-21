// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_FORM_FIELD_DATA_H_
#define CONTENT_PUBLIC_COMMON_FORM_FIELD_DATA_H_

#include <string>

#include "content/common/content_export.h"
#include "ui/base/ime/text_input_type.h"

namespace content {

// Contains information about a form input field.
struct CONTENT_EXPORT FormFieldData {
  FormFieldData();
  FormFieldData(const FormFieldData& other);
  ~FormFieldData();

  std::string text;
  std::string placeholder;
  ui::TextInputType text_input_type;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_FORM_FIELD_DATA_H_
