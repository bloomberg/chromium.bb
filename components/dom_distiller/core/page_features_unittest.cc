// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/page_features.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

// This test uses input data of core features and the output of the training
// pipeline's derived feature extraction to ensure that the extraction that is
// done in Chromium matches that in the training pipeline.
TEST(DomDistillerPageFeaturesTest, TestCalculateDerivedFeatures) {
  base::FilePath dir_source_root;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &dir_source_root));
  std::string input_data;
  ASSERT_TRUE(base::ReadFileToString(
      dir_source_root.AppendASCII(
          "components/test/data/dom_distiller/core_features.json"),
      &input_data));
  std::string expected_output_data;
  // This file contains the output from the calculation of derived features in
  // the training pipeline.
  ASSERT_TRUE(base::ReadFileToString(
      dir_source_root.AppendASCII(
          "components/test/data/dom_distiller/derived_features.json"),
      &expected_output_data));

  scoped_ptr<base::Value> input_json(base::JSONReader::Read(input_data));
  ASSERT_TRUE(input_json);

  scoped_ptr<base::Value> expected_output_json(
      base::JSONReader::Read(expected_output_data));
  ASSERT_TRUE(expected_output_json);

  base::ListValue* input_entries;
  ASSERT_TRUE(input_json->GetAsList(&input_entries));
  ASSERT_GT(input_entries->GetSize(), 0u);

  base::ListValue* expected_output_entries;
  ASSERT_TRUE(expected_output_json->GetAsList(&expected_output_entries));
  ASSERT_EQ(expected_output_entries->GetSize(), input_entries->GetSize());

  // In the output, the features list is a sequence of labels followed by values
  // (so labels at even indices, values at odd indices).
  base::DictionaryValue* entry;
  base::ListValue* derived_features;
  ASSERT_TRUE(expected_output_entries->GetDictionary(0, &entry));
  ASSERT_TRUE(entry->GetList("features", &derived_features));
  std::vector<std::string> labels;
  for (size_t i = 0; i < derived_features->GetSize(); i += 2) {
    std::string label;
    ASSERT_TRUE(derived_features->GetString(i, &label));
    labels.push_back(label);
  }

  for (size_t i = 0; i < input_entries->GetSize(); ++i) {
    base::DictionaryValue* core_features;
    ASSERT_TRUE(input_entries->GetDictionary(i, &entry));
    ASSERT_TRUE(entry->GetDictionary("features", &core_features));
    // CalculateDerivedFeaturesFromJSON expects a base::Value of the stringified
    // JSON (and not a base::Value of the JSON itself)
    std::string stringified_json;
    ASSERT_TRUE(base::JSONWriter::Write(*core_features, &stringified_json));
    scoped_ptr<base::Value> stringified_value(
        new base::StringValue(stringified_json));
    std::vector<double> derived(
        CalculateDerivedFeaturesFromJSON(stringified_value.get()));

    ASSERT_EQ(labels.size(), derived.size());
    ASSERT_TRUE(expected_output_entries->GetDictionary(i, &entry));
    ASSERT_TRUE(entry->GetList("features", &derived_features));
    std::string entry_url;
    ASSERT_TRUE(entry->GetString("url", &entry_url));
    for (size_t j = 0, value_index = 1; j < derived.size();
         ++j, value_index += 2) {
      double expected_value;
      if (!derived_features->GetDouble(value_index, &expected_value)) {
        bool bool_value;
        ASSERT_TRUE(derived_features->GetBoolean(value_index, &bool_value));
        expected_value = bool_value ? 1.0 : 0.0;
      }
      EXPECT_DOUBLE_EQ(derived[j], expected_value)
          << "incorrect value for entry with url " << entry_url
          << " for derived feature " << labels[j];
    }
  }
}
}
