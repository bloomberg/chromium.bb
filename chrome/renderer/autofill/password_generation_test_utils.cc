// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill/password_generation_test_utils.h"

#include <vector>

#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/test_password_generation_agent.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormElement.h"

namespace autofill {

void SetNotBlacklistedMessage(TestPasswordGenerationAgent* generation_agent,
                              const char* form_str) {
  autofill::PasswordForm form;
  form.origin =
      GURL(base::StringPrintf("data:text/html;charset=utf-8,%s", form_str));
  AutofillMsg_FormNotBlacklisted msg(0, form);
  generation_agent->OnMessageReceived(msg);
}

// Sends a message that the |form_index| form on the page is valid for
// account creation.
void SetAccountCreationFormsDetectedMessage(
    TestPasswordGenerationAgent* generation_agent,
    blink::WebDocument document,
    int form_index) {
  blink::WebVector<blink::WebFormElement> web_forms;
  document.forms(web_forms);

  autofill::FormData form_data;
  WebFormElementToFormData(web_forms[form_index],
                           blink::WebFormControlElement(),
                           EXTRACT_NONE,
                           &form_data,
                           nullptr /* FormFieldData */);

  std::vector<autofill::FormData> forms;
  forms.push_back(form_data);
  AutofillMsg_AccountCreationFormsDetected msg(0, forms);
  generation_agent->OnMessageReceived(msg);
}

void ExpectPasswordGenerationAvailable(
    TestPasswordGenerationAgent* password_generation,
    bool available) {
  if (available) {
    ASSERT_EQ(1u, password_generation->messages().size());
    EXPECT_EQ(AutofillHostMsg_ShowPasswordGenerationPopup::ID,
              password_generation->messages()[0]->type());
  } else {
    EXPECT_TRUE(password_generation->messages().empty());
  }
  password_generation->clear_messages();
}

}  // namespace autofill
