// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOCHECKOUT_PAGE_META_DATA_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOCHECKOUT_PAGE_META_DATA_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "components/autofill/content/browser/autocheckout_steps.h"
#include "components/autofill/core/common/web_element_descriptor.h"

namespace autofill {

// Container for multipage Autocheckout data.
struct AutocheckoutPageMetaData {
  AutocheckoutPageMetaData();
  ~AutocheckoutPageMetaData();

  // Returns true if the Autofill server says that the current page is start of
  // a multipage Autofill flow.
  bool IsStartOfAutofillableFlow() const;

  // Returns true if the Autofill server says that the current page is in a
  // multipage Autofill flow.
  bool IsInAutofillableFlow() const;

  // Returns true if the Autofill server says that the current page is the end
  // of a multipage Autofill flow.
  bool IsEndOfAutofillableFlow() const;

  // Page number of the multipage Autofill flow this form belongs to
  // (zero-indexed). If this form doesn't belong to any autofill flow, it is set
  // to -1.
  int current_page_number;

  // Total number of pages in the multipage Autofill flow. If this form doesn't
  // belong to any autofill flow, it is set to -1.
  int total_pages;

  // Whether ajaxy form changes that occur on this page during an Autocheckout
  // flow should be ignored.
  bool ignore_ajax;

  // A list of elements to click before filling form fields. Elements have to be
  // clicked in order.
  std::vector<WebElementDescriptor> click_elements_before_form_fill;

  // A list of elements to click after filling form fields, and before clicking
  // page_advance_button. Elements have to be clicked in order.
  std::vector<WebElementDescriptor> click_elements_after_form_fill;

  // The proceed element of the multipage Autofill flow. It can be empty
  // if current page is the last page of a flow or isn't a member of a flow.
  //
  // We do expect page navigation when click on |proceed_element_descriptor|,
  // and report an error if it doesn't. Oppositely, we do not expect page
  // navigation when click elements in |click_elements_before_form_fill| and
  // |click_elements_after_form_fill|. Because of this behavior difference and
  // |proceed_element_descriptor| is optional, we separate it from
  // |click_elements_after_form_fill|.
  WebElementDescriptor proceed_element_descriptor;

  // Mapping of page numbers to the types of Autocheckout actions that will be
  // performed on the given page of a multipage Autofill flow.
  // If this form doesn't belong to such a flow, the map will be empty.
  std::map<int, std::vector<AutocheckoutStepType> > page_types;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutocheckoutPageMetaData);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOCHECKOUT_PAGE_META_DATA_H_
