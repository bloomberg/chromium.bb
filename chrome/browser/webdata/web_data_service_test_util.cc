// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service_test_util.h"

MockWebDataServiceWrapperBase::MockWebDataServiceWrapperBase() {
}

MockWebDataServiceWrapperBase::~MockWebDataServiceWrapperBase() {
}

void MockWebDataServiceWrapperBase::Shutdown() {
}

// TODO(caitkp): This won't scale well. As we get more WebData subclasses, we
// will probably need a better way to create these mocks rather than passing
// all the webdatas in.
MockWebDataServiceWrapper::MockWebDataServiceWrapper(
    scoped_refptr<WebDataService> fake_service,
    scoped_refptr<AutofillWebDataService> fake_autofill)
    : fake_autofill_web_data_(fake_autofill),
      fake_web_data_(fake_service) {
}

MockWebDataServiceWrapper::~MockWebDataServiceWrapper() {
}

scoped_refptr<AutofillWebDataService>
    MockWebDataServiceWrapper::GetAutofillWebData() {
  return fake_autofill_web_data_;
}

scoped_refptr<WebDataService> MockWebDataServiceWrapper::GetWebData() {
  return fake_web_data_;
}
