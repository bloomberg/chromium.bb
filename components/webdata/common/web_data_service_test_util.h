// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_TEST_UTIL_H__
#define COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_TEST_UTIL_H__

#include "base/basictypes.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "components/signin/core/browser/webdata/token_web_data.h"

// Base class for mocks of WebDataService, that does nothing in
// Shutdown().
class MockWebDataServiceWrapperBase : public WebDataServiceWrapper {
 public:
  MockWebDataServiceWrapperBase();
  virtual ~MockWebDataServiceWrapperBase();

  virtual void Shutdown() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebDataServiceWrapperBase);
};

// Pass your fake WebDataService in the constructor and this will
// serve it up via GetWebData().
class MockWebDataServiceWrapper : public MockWebDataServiceWrapperBase {
 public:
  MockWebDataServiceWrapper(
      scoped_refptr<autofill::AutofillWebDataService> fake_autofill,
      scoped_refptr<TokenWebData> fake_token);

  virtual ~MockWebDataServiceWrapper();

  virtual scoped_refptr<autofill::AutofillWebDataService>
      GetAutofillWebData() OVERRIDE;

  virtual scoped_refptr<TokenWebData> GetTokenWebData() OVERRIDE;

 protected:
  scoped_refptr<autofill::AutofillWebDataService> fake_autofill_web_data_;
  scoped_refptr<TokenWebData> fake_token_web_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebDataServiceWrapper);
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_TEST_UTIL_H__
