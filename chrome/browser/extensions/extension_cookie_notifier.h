// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIE_NOTIFIER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIE_NOTIFIER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "net/cookies/cookie_store.h"

class Profile;

// Sends cookie-change notifications on the UI thread via
// chrome::NOTIFICATION_COOKIE_CHANGED_FOR_EXTENSIONS.
// This class must be used (AddStore() and OnCookieChanged() called) on a
// single thread, but it may be constructed on a different thread.
class ExtensionCookieNotifier {
 public:
  explicit ExtensionCookieNotifier(Profile* profile);
  ~ExtensionCookieNotifier();

  // Add a CookieStore for which cookie notifications will be transmitted.
  // This store will be monitored until this object is destructed; i.e.
  // |*store| must outlive this object.
  // Must be called on the IO thread.
  void AddStore(net::CookieStore* store);

 private:
  // net::CookieStore::CookieChangedCallback implementation.
  void OnCookieChanged(const net::CanonicalCookie& cookie,
                       net::CookieStore::ChangeCause cause);

  Profile* profile_;
  std::vector<std::unique_ptr<net::CookieStore::CookieChangedSubscription>>
      subscriptions_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ExtensionCookieNotifier);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIE_NOTIFIER_H_
