// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_PAGE_META_DATA_H_
#define COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_PAGE_META_DATA_H_

#include "base/memory/scoped_ptr.h"
#include "components/autofill/common/web_element_descriptor.h"

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

  // The proceed element of the multipage Autofill flow. Can be null if the
  // current page is the last page of a flow or isn't a member of a flow.
  scoped_ptr<WebElementDescriptor> proceed_element_descriptor;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutocheckoutPageMetaData);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_PAGE_META_DATA_H_
