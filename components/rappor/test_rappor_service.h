// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RAPPOR_TEST_RAPPOR_SERVICE_H_
#define COMPONENTS_RAPPOR_TEST_RAPPOR_SERVICE_H_

#include "base/prefs/testing_pref_service.h"
#include "components/rappor/rappor_service.h"

namespace rappor {

// This class provides a simple instance that can be instantiated by tests
// and examined to check that metrics were recorded.  It assumes the most
// permissive settings so that any metric can be recorded.
class TestRapporService : public RapporService {
 public:
  TestRapporService();

  ~TestRapporService() override;

  // Get the number of reports that would be uploaded by this service.
  // This also clears the internal map of metrics as a biproduct, so if
  // comparing numbers of reports, the comparison should be from the last time
  // GetReportsCount() was called (not from the beginning of the test).
  int GetReportsCount();

 private:
  TestingPrefServiceSimple prefs_;

  DISALLOW_COPY_AND_ASSIGN(TestRapporService);
};

}  // namespace rappor

#endif  // COMPONENTS_RAPPOR_TEST_RAPPOR_SERVICE_H_
