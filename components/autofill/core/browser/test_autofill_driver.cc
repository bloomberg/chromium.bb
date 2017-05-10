// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_driver.h"

#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

TestAutofillDriver::TestAutofillDriver() : url_request_context_(NULL) {}

TestAutofillDriver::~TestAutofillDriver() {}

bool TestAutofillDriver::IsIncognito() const {
  return false;
}

net::URLRequestContextGetter* TestAutofillDriver::GetURLRequestContext() {
  return url_request_context_;
}

bool TestAutofillDriver::RendererIsAvailable() {
  return true;
}

void TestAutofillDriver::SendFormDataToRenderer(int query_id,
                                                RendererFormDataAction action,
                                                const FormData& form_data) {
}

void TestAutofillDriver::PropagateAutofillPredictions(
    const std::vector<FormStructure*>& forms) {
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

void TestAutofillDriver::SetURLRequestContext(
    net::URLRequestContextGetter* url_request_context) {
  url_request_context_ = url_request_context;
}

void TestAutofillDriver::RendererShouldFillFieldWithValue(
    const base::string16& value) {
}

void TestAutofillDriver::RendererShouldPreviewFieldWithValue(
    const base::string16& value) {
}

void TestAutofillDriver::PopupHidden() {
}

gfx::RectF TestAutofillDriver::TransformBoundingBoxToViewportCoordinates(
    const gfx::RectF& bounding_box) {
  return bounding_box;
}

void TestAutofillDriver::DidInteractWithCreditCardForm() {}

}  // namespace autofill
