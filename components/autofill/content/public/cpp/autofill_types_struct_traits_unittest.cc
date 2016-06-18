// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/autofill/content/public/interfaces/test_autofill_types.mojom.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

const std::vector<const char*> kOptions = {"Option1", "Option2", "Option3",
                                           "Option4"};
namespace {

void CreateTestFieldDataPredictions(const std::string& signature,
                                    FormFieldDataPredictions* field_predict) {
  test::CreateTestSelectField("TestLabel", "TestName", "TestValue", kOptions,
                              kOptions, 4, &field_predict->field);
  field_predict->signature = signature;
  field_predict->heuristic_type = "TestSignature";
  field_predict->server_type = "TestServerType";
  field_predict->overall_type = "TestOverallType";
  field_predict->parseable_name = "TestParseableName";
}

}  // namespace

class AutofillTypeTraitsTestImpl : public testing::Test,
                                   public mojom::TypeTraitsTest {
 public:
  AutofillTypeTraitsTestImpl() {}

  mojom::TypeTraitsTestPtr GetTypeTraitsTestProxy() {
    return bindings_.CreateInterfacePtrAndBind(this);
  }

  // mojom::TypeTraitsTest:
  void PassFormData(const FormData& s,
                    const PassFormDataCallback& callback) override {
    callback.Run(s);
  }

  void PassFormFieldData(const FormFieldData& s,
                         const PassFormFieldDataCallback& callback) override {
    callback.Run(s);
  }

  void PassFormDataPredictions(
      const FormDataPredictions& s,
      const PassFormDataPredictionsCallback& callback) override {
    callback.Run(s);
  }

  void PassFormFieldDataPredictions(
      const FormFieldDataPredictions& s,
      const PassFormFieldDataPredictionsCallback& callback) override {
    callback.Run(s);
  }

 private:
  base::MessageLoop loop_;

  mojo::BindingSet<TypeTraitsTest> bindings_;
};

void ExpectFormFieldData(const FormFieldData& expected,
                         const base::Closure& closure,
                         const FormFieldData& passed) {
  EXPECT_TRUE(expected.SameFieldAs(passed));
  EXPECT_EQ(expected.option_values, passed.option_values);
  EXPECT_EQ(expected.option_contents, passed.option_contents);
  closure.Run();
}

void ExpectFormData(const FormData& expected,
                    const base::Closure& closure,
                    const FormData& passed) {
  EXPECT_TRUE(expected.SameFormAs(passed));
  closure.Run();
}

void ExpectFormFieldDataPredictions(const FormFieldDataPredictions& expected,
                                    const base::Closure& closure,
                                    const FormFieldDataPredictions& passed) {
  EXPECT_EQ(expected, passed);
  closure.Run();
}

void ExpectFormDataPredictions(const FormDataPredictions& expected,
                               const base::Closure& closure,
                               const FormDataPredictions& passed) {
  EXPECT_EQ(expected, passed);
  closure.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormFieldData) {
  FormFieldData input;
  test::CreateTestSelectField("TestLabel", "TestName", "TestValue", kOptions,
                              kOptions, 4, &input);

  base::RunLoop loop;
  mojom::TypeTraitsTestPtr proxy = GetTypeTraitsTestProxy();
  proxy->PassFormFieldData(
      input, base::Bind(&ExpectFormFieldData, input, loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormData) {
  FormData input;
  test::CreateTestAddressFormData(&input);

  base::RunLoop loop;
  mojom::TypeTraitsTestPtr proxy = GetTypeTraitsTestProxy();
  proxy->PassFormData(
      input, base::Bind(&ExpectFormData, input, loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormFieldDataPredictions) {
  FormFieldDataPredictions input;
  CreateTestFieldDataPredictions("TestSignature", &input);

  base::RunLoop loop;
  mojom::TypeTraitsTestPtr proxy = GetTypeTraitsTestProxy();
  proxy->PassFormFieldDataPredictions(
      input,
      base::Bind(&ExpectFormFieldDataPredictions, input, loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormDataPredictions) {
  FormDataPredictions input;
  test::CreateTestAddressFormData(&input.data);
  input.signature = "TestSignature";

  FormFieldDataPredictions field_predict;
  CreateTestFieldDataPredictions("Tom", &field_predict);
  input.fields.push_back(field_predict);
  CreateTestFieldDataPredictions("Jerry", &field_predict);
  input.fields.push_back(field_predict);
  CreateTestFieldDataPredictions("NoOne", &field_predict);
  input.fields.push_back(field_predict);

  base::RunLoop loop;
  mojom::TypeTraitsTestPtr proxy = GetTypeTraitsTestProxy();
  proxy->PassFormDataPredictions(
      input,
      base::Bind(&ExpectFormDataPredictions, input, loop.QuitClosure()));
  loop.Run();
}

}  // namespace autofill
