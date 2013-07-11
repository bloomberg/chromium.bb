// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "content/public/browser/web_contents_observer.h"

namespace autofill {

// This class is only for easier writing of tests. All pure virtual functions
// have been given empty methods.
// TODO(blundell): Eliminate this class being a WebContentsObserver once
// autofill shared code no longer needs knowledge of WebContents.
class TestAutofillDriver : public AutofillDriver,
                           public content::WebContentsObserver {
 public:
  explicit TestAutofillDriver(content::WebContents* web_contents);
  virtual ~TestAutofillDriver();

  // AutofillDriver implementation.
  virtual content::WebContents* GetWebContents() OVERRIDE;
  virtual bool RendererIsAvailable() OVERRIDE;
  virtual void SetRendererActionOnFormDataReception(
      RendererFormDataAction action) OVERRIDE;
  virtual void SendFormDataToRenderer(int query_id,
                                      const FormData& data) OVERRIDE;
  virtual void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) OVERRIDE;
  virtual void RendererShouldClearFilledForm() OVERRIDE;
  virtual void RendererShouldClearPreviewedForm() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAutofillDriver);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
