// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_UNIT_CONVERSION_RESULT_PARSER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_UNIT_CONVERSION_RESULT_PARSER_H_

#include "chromeos/components/quick_answers/search_result_parsers/result_parser.h"

namespace base {
class Value;
}  // namespace base

namespace chromeos {
namespace quick_answers {

class UnitConversionResultParser : public ResultParser {
 public:
  // ResultParser:
  bool Parse(const base::Value* result, QuickAnswer* quick_answer) override;
};

}  // namespace quick_answers
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_UNIT_CONVERSION_RESULT_PARSER_H_
