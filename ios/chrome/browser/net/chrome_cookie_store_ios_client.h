// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_CHROME_COOKIE_STORE_IOS_CLIENT_H_
#define IOS_CHROME_BROWSER_NET_CHROME_COOKIE_STORE_IOS_CLIENT_H_

#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "ios/net/cookies/cookie_store_ios_client.h"

@protocol BrowsingDataChangeListening;

// Chrome implementation of net::CookieStoreIOSClient. This class lives on the
// IOThread.
class ChromeCookieStoreIOSClient : public net::CookieStoreIOSClient {
 public:
  // Creates a CookieStoreIOSClient with a BrowsingDataChangeListening.
  // |browsing_data_change_listener| cannot be nil.
  explicit ChromeCookieStoreIOSClient(
      id<BrowsingDataChangeListening> browsing_data_change_listener);

  // CookieStoreIOSClient implementation.
  void DidChangeCookieStorage() const override;
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const override;

 private:
  base::ThreadChecker thread_checker_;
  // The listener that is informed of change in browsing data.
  id<BrowsingDataChangeListening> browsing_data_change_listener_;  // Weak.
  DISALLOW_COPY_AND_ASSIGN(ChromeCookieStoreIOSClient);
};

#endif  // IOS_CHROME_BROWSER_NET_CHROME_COOKIE_STORE_IOS_CLIENT_H_
