// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MOCK_BROWSING_DATA_COOKIE_HELPER_H_
#define CHROME_BROWSER_MOCK_BROWSING_DATA_COOKIE_HELPER_H_
#pragma once

#include <map>
#include <string>

#include "chrome/browser/browsing_data_cookie_helper.h"

// Mock for BrowsingDataCookieHelper.
class MockBrowsingDataCookieHelper : public BrowsingDataCookieHelper {
 public:
  explicit MockBrowsingDataCookieHelper(Profile* profile);

  // BrowsingDataCookieHelper methods.
  virtual void StartFetching(
      const net::CookieMonster::GetCookieListCallback &callback) OVERRIDE;
  virtual void CancelNotification() OVERRIDE;
  virtual void DeleteCookie(
      const net::CookieMonster::CanonicalCookie& cookie) OVERRIDE;

  // Adds some cookie samples.
  void AddCookieSamples(const GURL& url, const std::string& cookie_line);

  // Notifies the callback.
  void Notify();

  // Marks all cookies as existing.
  void Reset();

  // Returns true if all cookies since the last Reset() invocation were
  // deleted.
  bool AllDeleted();

 private:
  virtual ~MockBrowsingDataCookieHelper();

  Profile* profile_;
  net::CookieMonster::GetCookieListCallback callback_;

  net::CookieList cookie_list_;

  // Stores which cookies exist.
  std::map<const std::string, bool> cookies_;
};

#endif  // CHROME_BROWSER_MOCK_BROWSING_DATA_COOKIE_HELPER_H_
