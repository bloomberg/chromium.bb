// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_PREFERENCES_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_PREFERENCES_SYNC_TEST_H_
#pragma once

#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/live_sync/live_sync_test.h"

class LivePreferencesSyncTest : public LiveSyncTest {
 public:
  explicit LivePreferencesSyncTest(TestType test_type)
      : LiveSyncTest(test_type) {}
  virtual ~LivePreferencesSyncTest() {}

  // Used to access the preferences within a particular sync profile.
  PrefService* GetPrefs(int index) {
    return GetProfile(index)->GetPrefs();
  }

  // Used to access the preferences within the verifier sync profile.
  PrefService* GetVerifierPrefs() {
    return verifier()->GetPrefs();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LivePreferencesSyncTest);
};

class SingleClientLivePreferencesSyncTest : public LivePreferencesSyncTest {
 public:
  SingleClientLivePreferencesSyncTest()
      : LivePreferencesSyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientLivePreferencesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientLivePreferencesSyncTest);
};

class TwoClientLivePreferencesSyncTest : public LivePreferencesSyncTest {
 public:
  TwoClientLivePreferencesSyncTest() : LivePreferencesSyncTest(TWO_CLIENT) {}
  virtual ~TwoClientLivePreferencesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientLivePreferencesSyncTest);
};

class MultipleClientLivePreferencesSyncTest : public LivePreferencesSyncTest {
 public:
  MultipleClientLivePreferencesSyncTest()
      : LivePreferencesSyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientLivePreferencesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientLivePreferencesSyncTest);
};

class ManyClientLivePreferencesSyncTest : public LivePreferencesSyncTest {
 public:
  ManyClientLivePreferencesSyncTest() : LivePreferencesSyncTest(MANY_CLIENT) {}
  virtual ~ManyClientLivePreferencesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ManyClientLivePreferencesSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_PREFERENCES_SYNC_TEST_H_
