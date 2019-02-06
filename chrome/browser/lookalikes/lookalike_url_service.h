// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class Profile;

namespace base {
class Clock;
}

// A service that handles operations on lookalike URLs. It can fetch the list of
// engaged sites in a background thread and cache the results until the next
// update. This is more efficient than fetching the list on each navigation for
// each tab separately.
class LookalikeUrlService : public KeyedService {
 public:
  explicit LookalikeUrlService(Profile* profile);
  ~LookalikeUrlService() override;

  using EngagedSitesCallback =
      base::OnceCallback<void(const std::vector<GURL>&)>;

  static LookalikeUrlService* Get(Profile* profile);

  // Triggers an update to engaged site list and passes the most recent result
  // |callback|. The list is updated only after a certain amount of time passes
  // after the last update. As a result, calling this method may or may not
  // change the contents of engaged_sites, depending on the timing.
  void GetEngagedSites(EngagedSitesCallback callback);

  void SetClockForTesting(base::Clock* clock);
  void ClearEngagedSitesForTesting();

 private:
  void OnFetchEngagedSites(EngagedSitesCallback callback,
                           std::vector<mojom::SiteEngagementDetails> details);

  Profile* profile_;
  base::Clock* clock_;
  base::Time last_engagement_fetch_time_;
  std::vector<GURL> engaged_sites_;
  base::WeakPtrFactory<LookalikeUrlService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LookalikeUrlService);
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_H_
