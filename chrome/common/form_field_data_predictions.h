// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_FORM_FIELD_DATA_PREDICTIONS_H_
#define CHROME_COMMON_FORM_FIELD_DATA_PREDICTIONS_H_

#include <string>
#include <vector>

#include "chrome/common/form_field_data.h"

// Stores information about a field in a form.
struct FormFieldDataPredictions {
  FormFieldDataPredictions();
  FormFieldDataPredictions(const FormFieldDataPredictions& other);
  ~FormFieldDataPredictions();

  FormFieldData field;
  std::string signature;
  std::string heuristic_type;
  std::string server_type;
  std::string overall_type;
};

#endif  // CHROME_COMMON_FORM_FIELD_DATA_PREDICTIONS_H_
