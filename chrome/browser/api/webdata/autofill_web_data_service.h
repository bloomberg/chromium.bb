// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_API_WEBDATA_AUTOFILL_WEB_DATA_SERVICE_H_
#define CHROME_BROWSER_API_WEBDATA_AUTOFILL_WEB_DATA_SERVICE_H_

#include <vector>

#include "chrome/browser/api/webdata/autofill_web_data.h"

class AutofillChange;
class WebDatabaseService;

namespace content {
class BrowserContext;
}

typedef std::vector<AutofillChange> AutofillChangeList;

// API for Autofill web data.
class AutofillWebDataService
    : public AutofillWebData,
      public WebDataServiceBase {
 public:
  AutofillWebDataService();

  AutofillWebDataService(scoped_refptr<WebDatabaseService> wdbs,
                         const ProfileErrorCallback& callback);

  // Retrieve an AutofillWebDataService for the given context.
  //
  // Can return NULL in some contexts.
  static scoped_refptr<AutofillWebDataService> FromBrowserContext(
      content::BrowserContext* context);

  // Notifies listeners on the UI thread that multiple changes have been made to
  // to Autofill records of the database.
  // NOTE: This method is intended to be called from the DB thread.  It
  // it asynchronously notifies listeners on the UI thread.
  // |web_data_service| may be NULL for testing purposes.
  static void NotifyOfMultipleAutofillChanges(
      AutofillWebDataService* web_data_service);

 protected:
  virtual ~AutofillWebDataService() {}
};

#endif  // CHROME_BROWSER_API_WEBDATA_AUTOFILL_WEB_DATA_SERVICE_H_
