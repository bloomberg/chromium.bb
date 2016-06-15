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

 private:
  base::MessageLoop loop_;

  mojo::BindingSet<TypeTraitsTest> bindings_;
};

TEST_F(AutofillTypeTraitsTestImpl, PassFormFieldData) {
  FormFieldData input;
  const std::vector<const char*> kOptions = {"Option1", "Option2", "Option3",
                                             "Option4"};
  test::CreateTestSelectField("TestLabel", "TestName", "TestValue", kOptions,
                              kOptions, 4, &input);

  base::RunLoop loop;
  mojom::TypeTraitsTestPtr proxy = GetTypeTraitsTestProxy();
  proxy->PassFormFieldData(input, [&input, &loop](const FormFieldData& passed) {
    EXPECT_TRUE(input.SameFieldAs(passed));
    EXPECT_EQ(input.option_values, passed.option_values);
    EXPECT_EQ(input.option_contents, passed.option_contents);
    loop.Quit();
  });
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormData) {
  FormData input;
  test::CreateTestAddressFormData(&input);

  base::RunLoop loop;
  mojom::TypeTraitsTestPtr proxy = GetTypeTraitsTestProxy();
  proxy->PassFormData(input, [&input, &loop](const FormData& passed) {
    EXPECT_TRUE(input.SameFormAs(passed));
    loop.Quit();
  });
  loop.Run();
}

}  // namespace autofill
