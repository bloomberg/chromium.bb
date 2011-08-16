// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_util.h"

#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/values.h"

namespace webdriver {

SkipParsing* kSkipParsing = NULL;

std::string GenerateRandomID() {
  uint64 msb = base::RandUint64();
  uint64 lsb = base::RandUint64();
  return base::StringPrintf("%016" PRIx64 "%016" PRIx64, msb, lsb);
}

std::string JsonStringify(const Value* value) {
  std::string json;
  base::JSONWriter::Write(value, false, &json);
  return json;
}

ValueParser::ValueParser() { }

ValueParser::~ValueParser() { }

}  // namespace webdriver

bool ValueConversionTraits<webdriver::SkipParsing>::SetFromValue(
    const base::Value* value, const webdriver::SkipParsing* t) {
  return true;
}

bool ValueConversionTraits<webdriver::SkipParsing>::CanConvert(
    const base::Value* value) {
  return true;
}
