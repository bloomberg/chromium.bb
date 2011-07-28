// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_SYNC_DATATYPE_HELPER_H_
#define CHROME_TEST_LIVE_SYNC_SYNC_DATATYPE_HELPER_H_
#pragma once

#include "base/basictypes.h"

class LiveSyncTest;

class SyncDatatypeHelper {
 public:
  // Associates an instance of LiveSyncTest with a SyncDatatypeHelper. Must be
  // called before any of the methods in the helper subclasses can be used.
  static void AssociateWithTest(LiveSyncTest* test);

  // Returns |test_| after making sure it is a valid pointer.
  static LiveSyncTest* test();

 protected:
  SyncDatatypeHelper();
  virtual ~SyncDatatypeHelper();

 private:
  // The LiveSyncTest instance associated with this helper object.
  static LiveSyncTest* test_;

  DISALLOW_COPY_AND_ASSIGN(SyncDatatypeHelper);
};


#endif  // CHROME_TEST_LIVE_SYNC_SYNC_DATATYPE_HELPER_H_
