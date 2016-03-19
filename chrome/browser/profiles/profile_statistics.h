// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_H_
#define CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class FilePath;
}  // namespace base
class Profile;
class ProfileStatisticsAggregator;

// Instances of ProfileStatistics should be created directly. Use
// ProfileStatisticsFactory instead.
class ProfileStatistics : public KeyedService {
 public:
  // Profile Statistics --------------------------------------------------------

  // This function collects statistical information about |profile|, also
  // returns the information via |callback| if |callback| is not null. The
  // statistical information is also copied to ProfileAttributesStorage.
  // Currently bookmarks, history, logins and preferences are counted. The
  // callback function will probably be called more than once, so binding
  // parameters with bind::Passed() is prohibited. Most of the async tasks
  // involved in this function can be cancelled if |tracker| is not null.
  void GatherStatistics(const profiles::ProfileStatisticsCallback& callback);

  bool HasAggregator() const;
  ProfileStatisticsAggregator* GetAggregator() const;

  // ProfileAttributesStorage --------------------------------------------------

  // Gets statistical information from the profiles attributes storage.
  static profiles::ProfileCategoryStats
      GetProfileStatisticsFromAttributesStorage(
           const base::FilePath& profile_path);

  // Sets an individual statistic to the profiles attributes storage.
  static void SetProfileStatisticsToAttributesStorage(
      const base::FilePath& profile_path,
      const std::string& category,
      int count);

 private:
  friend class ProfileStatisticsFactory;

  explicit ProfileStatistics(Profile* profile);
  ~ProfileStatistics() override;
  void RegisterAggregator(ProfileStatisticsAggregator* const aggregator);
  void DeregisterAggregator();

  Profile* profile_;
  ProfileStatisticsAggregator* aggregator_;
  base::WeakPtrFactory<ProfileStatistics> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_H_
