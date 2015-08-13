// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COUNTER_H_

#include <string>

#include "base/callback.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/profiles/profile.h"

class BrowsingDataCounter {
 public:
  typedef base::Callback<void(bool, uint32)> Callback;

  BrowsingDataCounter();
  virtual ~BrowsingDataCounter();

  // Should be called once to initialize this class.
  void Init(Profile* profile,
            const Callback& callback);

  // Name of the preference associated with this counter.
  virtual const std::string& GetPrefName() const = 0;

  // The profile associated with this counter.
  Profile* GetProfile() const;

 protected:
  // Called when some conditions have changed and the counting should be
  // restarted, e.g. when the deletion preference changed state or when
  // we were notified of data changes.
  void RestartCounting();

  // Should be called from |Count| by any overriding class to indicate that
  // counting is finished and report the |result|.
  void ReportResult(uint32 result);

 private:
  // Called after the class is initialized by calling |Init|.
  virtual void OnInitialized();

  // Count the data.
  virtual void Count() = 0;

  // The profile for which we will count the data volume.
  Profile* profile_;

  // The callback that will be called when the UI should be updated with a new
  // counter value.
  Callback callback_;

  // The boolean preference indicating whether this data type is to be deleted.
  // If false, we will not count it.
  BooleanPrefMember pref_;

  // Whether this counter is currently in the process of counting.
  bool counting_;

  // Whether this class was properly initialized by calling |Init|.
  bool initialized_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COUNTER_H_
