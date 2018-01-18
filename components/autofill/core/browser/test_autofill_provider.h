// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROVIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROVIDER_H_

#include "components/autofill/core/browser/autofill_provider.h"

namespace autofill {

class TestAutofillProvider : public AutofillProvider {
 public:
  ~TestAutofillProvider() override{};

  // AutofillProvider:
  void OnQueryFormFieldAutofill(AutofillHandlerProxy* handler,
                                int32_t id,
                                const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box) override;
  void OnTextFieldDidChange(AutofillHandlerProxy* handler,
                            const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box,
                            const base::TimeTicks timestamp) override;
  void OnTextFieldDidScroll(AutofillHandlerProxy* handler,
                            const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box) override;
  bool OnFormSubmitted(AutofillHandlerProxy* handler,
                       const FormData& form,
                       bool known_success,
                       SubmissionSource source,
                       base::TimeTicks timestamp) override;
  void OnFocusNoLongerOnForm(AutofillHandlerProxy* handler) override;
  void OnFocusOnFormField(AutofillHandlerProxy* handler,
                          const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box) override;
  void OnDidFillAutofillFormData(AutofillHandlerProxy* handler,
                                 const FormData& form,
                                 base::TimeTicks timestamp) override;
  void OnFormsSeen(AutofillHandlerProxy* handler,
                   const std::vector<FormData>& forms,
                   const base::TimeTicks timestamp) override;
  void Reset(AutofillHandlerProxy* handler) override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROVIDER_H_
