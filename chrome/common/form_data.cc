// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/form_data.h"

#include "base/string_util.h"

FormData::FormData()
    : user_submitted(false) {
}

FormData::FormData(const FormData& data)
    : name(data.name),
      method(data.method),
      origin(data.origin),
      action(data.action),
      user_submitted(data.user_submitted),
      fields(data.fields),
      ssl_status(data.ssl_status) {
}

FormData::~FormData() {
}

bool FormData::operator==(const FormData& form) const {
  return (name == form.name &&
          StringToLowerASCII(method) == StringToLowerASCII(form.method) &&
          origin == form.origin &&
          action == form.action &&
          user_submitted == form.user_submitted &&
          fields == form.fields &&
          ssl_status.Equals(form.ssl_status));
}

bool FormData::operator!=(const FormData& form) const {
  return !operator==(form);
}
