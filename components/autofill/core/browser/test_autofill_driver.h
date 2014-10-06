// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "components/autofill/core/browser/autofill_driver.h"

namespace base {
class SequencedWorkerPoolOwner;
}

namespace autofill {

// This class is only for easier writing of tests.
class TestAutofillDriver : public AutofillDriver {
 public:
  TestAutofillDriver();
  virtual ~TestAutofillDriver();

  // AutofillDriver implementation.
  virtual bool IsOffTheRecord() const override;
  // Returns the value passed in to the last call to |SetURLRequestContext()|
  // or NULL if that method has never been called.
  virtual net::URLRequestContextGetter* GetURLRequestContext() override;
  virtual base::SequencedWorkerPool* GetBlockingPool() override;
  virtual bool RendererIsAvailable() override;
  virtual void SendFormDataToRenderer(int query_id,
                                      RendererFormDataAction action,
                                      const FormData& data) override;
  virtual void PingRenderer() override;
  virtual void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) override;
  virtual void RendererShouldAcceptDataListSuggestion(
      const base::string16& value) override;
  virtual void RendererShouldClearFilledForm() override;
  virtual void RendererShouldClearPreviewedForm() override;
  virtual void RendererShouldFillFieldWithValue(
      const base::string16& value) override;
  virtual void RendererShouldPreviewFieldWithValue(
      const base::string16& value) override;

  // Methods that tests can use to specialize functionality.

  // Sets the URL request context for this instance. |url_request_context|
  // should outlive this instance.
  void SetURLRequestContext(net::URLRequestContextGetter* url_request_context);

 private:
  scoped_ptr<base::SequencedWorkerPoolOwner> blocking_pool_owner_;
  net::URLRequestContextGetter* url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDriver);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
