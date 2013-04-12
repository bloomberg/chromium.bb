// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/common/form_field_data_predictions.h"

namespace autofill {

FormFieldDataPredictions::FormFieldDataPredictions() {
}

FormFieldDataPredictions::FormFieldDataPredictions(
    const FormFieldDataPredictions& other)
    : field(other.field),
      signature(other.signature),
      heuristic_type(other.heuristic_type),
      server_type(other.server_type),
      overall_type(other.overall_type) {
}

FormFieldDataPredictions::~FormFieldDataPredictions() {
}

}  // namespace autofill
