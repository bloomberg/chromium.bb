// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_data_model.h"

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// Provides concrete implementations for pure virtual methods.
class TestAutofillDataModel : public AutofillDataModel {
 public:
  TestAutofillDataModel(const std::string& guid, const std::string& origin)
      : AutofillDataModel(guid, origin) {}
  virtual ~TestAutofillDataModel() {}

 private:
  virtual base::string16 GetRawInfo(ServerFieldType type) const OVERRIDE {
    return base::string16();
  }
  virtual void SetRawInfo(ServerFieldType type,
                          const base::string16& value) OVERRIDE {}
  virtual void GetSupportedTypes(
      ServerFieldTypeSet* supported_types) const OVERRIDE {}

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDataModel);
};

}  // namespace

TEST(AutofillDataModelTest, IsVerified) {
  TestAutofillDataModel model("guid", std::string());
  EXPECT_FALSE(model.IsVerified());

  model.set_origin("http://www.example.com");
  EXPECT_FALSE(model.IsVerified());

  model.set_origin("https://www.example.com");
  EXPECT_FALSE(model.IsVerified());

  model.set_origin("file:///tmp/example.txt");
  EXPECT_FALSE(model.IsVerified());

  model.set_origin("data:text/plain;charset=utf-8;base64,ZXhhbXBsZQ==");
  EXPECT_FALSE(model.IsVerified());

  model.set_origin("chrome://settings/autofill");
  EXPECT_FALSE(model.IsVerified());

  model.set_origin("Chrome Settings");
  EXPECT_TRUE(model.IsVerified());

  model.set_origin("Some gibberish string");
  EXPECT_TRUE(model.IsVerified());

  model.set_origin(std::string());
  EXPECT_FALSE(model.IsVerified());
}

}  // namespace autofill
