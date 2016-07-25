// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_MEDIA_LICENSES_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_MEDIA_LICENSES_COUNTER_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "url/gurl.h"

class Profile;

// MediaLicensesCounter is used to determine the number of origins that
// have plugin private filesystem data (used by EME). It does not include
// origins that have content licenses owned by Flash.
class MediaLicensesCounter : public browsing_data::BrowsingDataCounter {
 public:
  // MediaLicenseResult is the result of counting the number of origins
  // that have plugin private filesystem data. It also contains one of the
  // origins so that a message can displayed providing an example of the
  // origins that hold content licenses.
  class MediaLicenseResult : public FinishedResult {
   public:
    MediaLicenseResult(const MediaLicensesCounter* source,
                       const std::set<GURL>& origins);
    ~MediaLicenseResult() override;

    const std::string& GetOneOrigin() const;

   private:
    std::string one_origin_;
  };

  explicit MediaLicensesCounter(Profile* profile);
  ~MediaLicensesCounter() override;

  const char* GetPrefName() const override;

 private:
  Profile* profile_;

  // BrowsingDataCounter implementation.
  void Count() final;

  // Determining the set of origins used by the plugin private filesystem is
  // done asynchronously. This callback returns the results, which are
  // subsequently reported.
  void OnContentLicensesObtained(const std::set<GURL>& origins);

  base::WeakPtrFactory<MediaLicensesCounter> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_MEDIA_LICENSES_COUNTER_H_
