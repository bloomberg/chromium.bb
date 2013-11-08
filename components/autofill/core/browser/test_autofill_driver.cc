// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_driver.h"

#include "base/threading/sequenced_worker_pool.h"

namespace autofill {

TestAutofillDriver::TestAutofillDriver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      blocking_pool_(new base::SequencedWorkerPool(4, "TestAutofillDriver")) {}

TestAutofillDriver::~TestAutofillDriver() {
  blocking_pool_->Shutdown();
}

bool TestAutofillDriver::IsOffTheRecord() const {
  return false;
}

content::WebContents* TestAutofillDriver::GetWebContents() {
  return web_contents();
}

base::SequencedWorkerPool* TestAutofillDriver::GetBlockingPool() {
  return blocking_pool_;
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

void TestAutofillDriver::RendererShouldAcceptDataListSuggestion(
      const base::string16& value) {
}

void TestAutofillDriver::RendererShouldClearFilledForm() {
}

void TestAutofillDriver::RendererShouldClearPreviewedForm() {
}

}  // namespace autofill
