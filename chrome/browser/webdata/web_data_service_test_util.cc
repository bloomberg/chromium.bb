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

MockWebDataServiceWrapper::MockWebDataServiceWrapper(
    scoped_refptr<WebDataService> fake_service)
    : fake_web_data_service_(fake_service) {
}

MockWebDataServiceWrapper::~MockWebDataServiceWrapper() {
}

scoped_refptr<WebDataService> MockWebDataServiceWrapper::GetWebData() {
  return fake_web_data_service_;
}
