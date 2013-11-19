// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_MOCK_FACTORY_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_MOCK_FACTORY_H_

#include "chrome/browser/prefs/pref_service_syncable_factory.h"

// A helper that allows convenient building of custom PrefServices in tests.
class PrefServiceMockFactory : public PrefServiceSyncableFactory {
 public:
  PrefServiceMockFactory();
  virtual ~PrefServiceMockFactory();

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefServiceMockFactory);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_MOCK_FACTORY_H_
