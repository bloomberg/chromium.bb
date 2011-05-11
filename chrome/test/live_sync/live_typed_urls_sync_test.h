// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_TYPED_URLS_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_TYPED_URLS_SYNC_TEST_H_
#pragma once

#include <vector>

#include "chrome/browser/history/history_types.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "content/browser/cancelable_request.h"

namespace base {
class Time;
}

class HistoryService;

class LiveTypedUrlsSyncTest : public LiveSyncTest {
 public:
  explicit LiveTypedUrlsSyncTest(TestType test_type);
  virtual ~LiveTypedUrlsSyncTest();

  // Gets the typed URLs from a specific sync profile.
  std::vector<history::URLRow> GetTypedUrlsFromClient(int index);

  // Adds a URL to the history DB for a specific sync profile (just registers a
  // new visit if the URL already exists).
  void AddUrlToHistory(int index, const GURL& url);

  // Deletes a URL from the history DB for a specific sync profile.
  void DeleteUrlFromHistory(int index, const GURL& url);

  // Returns true if all clients match the verifier profile.
  void AssertAllProfilesHaveSameURLsAsVerifier();

  // Checks that the two vectors contain the same set of URLRows (possibly in
  // a different order).
  void AssertURLRowVectorsAreEqual(const std::vector<history::URLRow>& left,
                                   const std::vector<history::URLRow>& right);

  // Checks that the passed URLRows are equivalent.
  void AssertURLRowsAreEqual(const history::URLRow& left,
                             const history::URLRow& right);

 private:
  // Waits for the history DB thread to finish executing its current set of
  // tasks.
  void WaitForHistoryDBThread(int index);

  // Creates a URLRow in the specified HistoryService.
  void AddToHistory(HistoryService* service,
                    const GURL& url,
                    const base::Time& timestamp);

  std::vector<history::URLRow> GetTypedUrlsFromHistoryService(
      HistoryService* service);


  // Helper object to make sure we don't leave tasks running on the history
  // thread.
  CancelableRequestConsumerT<int, 0> cancelable_consumer_;
  DISALLOW_COPY_AND_ASSIGN(LiveTypedUrlsSyncTest);
};

class SingleClientLiveTypedUrlsSyncTest : public LiveTypedUrlsSyncTest {
 public:
  SingleClientLiveTypedUrlsSyncTest()
      : LiveTypedUrlsSyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientLiveTypedUrlsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientLiveTypedUrlsSyncTest);
};

class TwoClientLiveTypedUrlsSyncTest : public LiveTypedUrlsSyncTest {
 public:
  TwoClientLiveTypedUrlsSyncTest() : LiveTypedUrlsSyncTest(TWO_CLIENT) {}
  virtual ~TwoClientLiveTypedUrlsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientLiveTypedUrlsSyncTest);
};

class MultipleClientLiveTypedUrlsSyncTest : public LiveTypedUrlsSyncTest {
 public:
  MultipleClientLiveTypedUrlsSyncTest()
      : LiveTypedUrlsSyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientLiveTypedUrlsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientLiveTypedUrlsSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_TYPED_URLS_SYNC_TEST_H_
