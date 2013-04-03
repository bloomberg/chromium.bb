// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_AUTOFILL_AUTOFILL_WEBDATA_SERVICE_OBSERVER_H_
#define COMPONENTS_WEBDATA_AUTOFILL_AUTOFILL_WEBDATA_SERVICE_OBSERVER_H_

#include "components/webdata/autofill/autofill_change.h"

class AutofillWebDataServiceObserverOnDBThread {
 public:
  // Called on DB thread whenever Autofill entries are changed.
  virtual void AutofillEntriesChanged(const AutofillChangeList& changes) {}

  // Called on DB thread when an AutofillProfile has been added/removed/updated
  // in the WebDatabase.
  virtual void AutofillProfileChanged(const AutofillProfileChange& change) {}

 protected:
  virtual ~AutofillWebDataServiceObserverOnDBThread() {}
};

class AutofillWebDataServiceObserverOnUIThread {
 public:
  // Called on UI thread whenever the web database service has finished loading
  // the web database.
  virtual void WebDatabaseLoaded() {}

  // Called on UI thread when multiple Autofill entries have been modified by
  // Sync.
  virtual void AutofillMultipleChanged() {}

 protected:
  virtual ~AutofillWebDataServiceObserverOnUIThread() {}
};

#endif  // COMPONENTS_WEBDATA_AUTOFILL_AUTOFILL_WEBDATA_SERVICE_OBSERVER_H_
