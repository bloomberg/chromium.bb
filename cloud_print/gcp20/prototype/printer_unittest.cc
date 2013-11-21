// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/printer.h"

#include "base/basictypes.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(Printer, ValidateCapabilities) {
  int error_code;
  std::string error_msg;
  Printer printer;
  scoped_ptr<base::Value> value(
      base::JSONReader::ReadAndReturnError(printer.GetRawCdd(), 0,
                                           &error_code, &error_msg));
  ASSERT_TRUE(!!value) << error_msg;

  base::DictionaryValue* dictionary_value = NULL;
  EXPECT_TRUE(value->GetAsDictionary(&dictionary_value)) << "Not a dictionary";
}

