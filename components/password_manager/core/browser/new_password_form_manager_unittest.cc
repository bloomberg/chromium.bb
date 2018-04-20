// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/new_password_form_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FormData;
using autofill::FormFieldData;
using base::ASCIIToUTF16;

namespace password_manager {
class NewPasswordFormManagerTest : public testing::Test {
 public:
  NewPasswordFormManagerTest() {
    form_.name = ASCIIToUTF16("sign-in");

    FormFieldData field;
    field.name = ASCIIToUTF16("firstname");
    field.form_control_type = "text";
    form_.fields.push_back(field);

    field.name = ASCIIToUTF16("username");
    field.form_control_type = "text";
    form_.fields.push_back(field);

    field.name = ASCIIToUTF16("password");
    field.form_control_type = "password";
    form_.fields.push_back(field);
  }

 protected:
  FormData form_;
  StubPasswordManagerClient client_;
};

TEST_F(NewPasswordFormManagerTest, DoesManage) {
  NewPasswordFormManager form_manager(&client_, form_);
  EXPECT_TRUE(form_manager.DoesManage(form_));
  FormData another_form = form_;
  another_form.name += ASCIIToUTF16("1");
  EXPECT_FALSE(form_manager.DoesManage(another_form));

  another_form = form_;
  another_form.fields[0].name += ASCIIToUTF16("1");
  EXPECT_FALSE(form_manager.DoesManage(another_form));
}

}  // namespace  password_manager
