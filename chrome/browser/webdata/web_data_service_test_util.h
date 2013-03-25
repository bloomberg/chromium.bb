// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_TEST_UTIL_H__
#define CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_TEST_UTIL_H__

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "content/public/browser/browser_thread.h"

template <class T>
class AutofillWebDataServiceConsumer: public WebDataServiceConsumer {
 public:
  AutofillWebDataServiceConsumer() : handle_(0) {}
  virtual ~AutofillWebDataServiceConsumer() {}

  virtual void OnWebDataServiceRequestDone(WebDataService::Handle handle,
                                           const WDTypedResult* result) {
    using content::BrowserThread;
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    handle_ = handle;
    const WDResult<T>* wrapped_result =
        static_cast<const WDResult<T>*>(result);
    result_ = wrapped_result->GetValue();

    MessageLoop::current()->Quit();
  }

  WebDataService::Handle handle() { return handle_; }
  T& result() { return result_; }

 private:
  WebDataService::Handle handle_;
  T result_;
  DISALLOW_COPY_AND_ASSIGN(AutofillWebDataServiceConsumer);
};

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
      scoped_refptr<WebDataService> fake_service,
      scoped_refptr<AutofillWebDataService> fake_autofill);

  virtual ~MockWebDataServiceWrapper();

  virtual scoped_refptr<AutofillWebDataService> GetAutofillWebData() OVERRIDE;

  virtual scoped_refptr<WebDataService> GetWebData() OVERRIDE;

 protected:
  scoped_refptr<AutofillWebDataService> fake_autofill_web_data_;
  scoped_refptr<WebDataService> fake_web_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebDataServiceWrapper);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_TEST_UTIL_H__
