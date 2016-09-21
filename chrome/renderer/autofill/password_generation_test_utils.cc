// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill/password_generation_test_utils.h"

#include <vector>

#include "base/strings/stringprintf.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/test_password_generation_agent.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/signatures_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormElement.h"

namespace autofill {

void SetNotBlacklistedMessage(TestPasswordGenerationAgent* generation_agent,
                              const char* form_str) {
  autofill::PasswordForm form;
  form.origin = form_util::StripAuthAndParams(
      GURL(base::StringPrintf("data:text/html;charset=utf-8,%s", form_str)));
  generation_agent->FormNotBlacklisted(form);
}

// Sends a message that the |form_index| form on the page is valid for
// account creation.
void SetAccountCreationFormsDetectedMessage(
    TestPasswordGenerationAgent* generation_agent,
    blink::WebDocument document,
    int form_index,
    int field_index) {
  blink::WebVector<blink::WebFormElement> web_forms;
  document.forms(web_forms);

  autofill::FormData form_data;
  WebFormElementToFormData(
      web_forms[form_index], blink::WebFormControlElement(), nullptr,
      form_util::EXTRACT_NONE, &form_data, nullptr /* FormFieldData */);

  std::vector<autofill::PasswordFormGenerationData> forms;
  forms.push_back(autofill::PasswordFormGenerationData{
      CalculateFormSignature(form_data),
      CalculateFieldSignatureForField(form_data.fields[field_index])});
  generation_agent->FoundFormsEligibleForGeneration(forms);
}

}  // namespace autofill
