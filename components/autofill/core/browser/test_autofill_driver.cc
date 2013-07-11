// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_driver.h"

namespace autofill {

TestAutofillDriver::TestAutofillDriver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

TestAutofillDriver::~TestAutofillDriver() {}

content::WebContents* TestAutofillDriver::GetWebContents() {
  return web_contents();
}

bool TestAutofillDriver::RendererIsAvailable() {
  return true;
}

void TestAutofillDriver::SetRendererActionOnFormDataReception(
    RendererFormDataAction action) {
}

void TestAutofillDriver::SendFormDataToRenderer(int query_id,
                                                const FormData& form_data) {
}

void TestAutofillDriver::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
}

void TestAutofillDriver::RendererShouldClearFilledForm() {
}

void TestAutofillDriver::RendererShouldClearPreviewedForm() {
}

}  // namespace autofill
