// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/cld_data_harness.h"

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "chrome/browser/translate/component_cld_data_harness.h"
#include "chrome/browser/translate/standalone_cld_data_harness.h"
#include "chrome/common/chrome_paths.h"
#include "components/translate/content/common/cld_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// This constant yields the version of the CRX that has been extracted into
// the test data directory, and must be kept in sync with what is there.
// A reciprocal comment has been placed in
// chrome/test/data/cld2_component/README.chromium; don't update one without
// updating the other.
const base::FilePath::CharType kCrxVersion[] = FILE_PATH_LITERAL("160");

class StaticCldDataHarness : public test::CldDataHarness {
 public:
  StaticCldDataHarness() {}
  ~StaticCldDataHarness() override {};
  void Init() override {
    translate::CldDataSource::Set(
        translate::CldDataSource::GetStaticDataSource());
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(StaticCldDataHarness);
};

}  // namespace

namespace test {

const base::FilePath::CharType* CldDataHarness::GetTestDataSourceCrxVersion() {
  return kCrxVersion;
}

void CldDataHarness::GetTestDataSourceDirectory(base::FilePath* out_path) {
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, out_path));
  *out_path =
      out_path->Append(FILE_PATH_LITERAL("cld2_component")).Append(kCrxVersion);
}

// static
std::unique_ptr<CldDataHarness> CldDataHarness::CreateStaticDataHarness() {
  return std::unique_ptr<CldDataHarness>(new StaticCldDataHarness());
}

// static
std::unique_ptr<CldDataHarness> CldDataHarness::CreateStandaloneDataHarness() {
  return std::unique_ptr<CldDataHarness>(new StandaloneCldDataHarness());
}

// static
std::unique_ptr<CldDataHarness> CldDataHarness::CreateComponentDataHarness() {
  return std::unique_ptr<CldDataHarness>(new ComponentCldDataHarness());
}

}  // namespace test
