// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_HANDLER_PROXY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_HANDLER_PROXY_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_handler.h"

namespace autofill {

class AutofillProvider;

// This class forwards AutofillHandler calls to AutofillProvider.
class AutofillHandlerProxy : public AutofillHandler {
 public:
  AutofillHandlerProxy(AutofillDriver* driver, AutofillProvider* provider);
  ~AutofillHandlerProxy() override;

  void OnFocusNoLongerOnForm() override;

  void OnDidFillAutofillFormData(const FormData& form,
                                 const base::TimeTicks timestamp) override;

  void OnDidPreviewAutofillFormData() override;

  void OnFormsSeen(const std::vector<FormData>& forms,
                   const base::TimeTicks timestamp) override;

  bool OnFormSubmitted(const FormData& form) override;

  void OnDidEndTextFieldEditing() override;
  void OnHidePopup() override;
  void OnSetDataList(const std::vector<base::string16>& values,
                     const std::vector<base::string16>& labels) override;

  void Reset() override;

  base::WeakPtr<AutofillHandlerProxy> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  bool OnWillSubmitFormImpl(const FormData& form,
                            const base::TimeTicks timestamp) override;

  void OnTextFieldDidChangeImpl(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box,
                                const base::TimeTicks timestamp) override;

  void OnQueryFormFieldAutofillImpl(int query_id,
                                    const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box) override;

  void OnFocusOnFormFieldImpl(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override;

 private:
  AutofillProvider* provider_;
  base::WeakPtrFactory<AutofillHandlerProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillHandlerProxy);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_HANDLER_PROXY_H_
