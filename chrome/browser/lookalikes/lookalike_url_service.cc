// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace {
constexpr uint32_t kEngagedSiteUpdateIntervalInSeconds = 5 * 60;

class LookalikeUrlServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static LookalikeUrlService* GetForProfile(Profile* profile) {
    return static_cast<LookalikeUrlService*>(
        GetInstance()->GetServiceForBrowserContext(profile,
                                                   /*create_service=*/true));
  }
  static LookalikeUrlServiceFactory* GetInstance() {
    return base::Singleton<LookalikeUrlServiceFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<LookalikeUrlServiceFactory>;

  // LookalikeUrlServiceFactory();
  LookalikeUrlServiceFactory()
      : BrowserContextKeyedServiceFactory(
            "LookalikeUrlServiceFactory",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(SiteEngagementServiceFactory::GetInstance());
  }

  ~LookalikeUrlServiceFactory() override {}

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override {
    return new LookalikeUrlService(static_cast<Profile*>(profile));
  }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return chrome::GetBrowserContextOwnInstanceInIncognito(context);
  }

  DISALLOW_COPY_AND_ASSIGN(LookalikeUrlServiceFactory);
};

}  // namespace

LookalikeUrlService::LookalikeUrlService(Profile* profile)
    : profile_(profile),
      clock_(base::DefaultClock::GetInstance()),
      weak_factory_(this) {}

LookalikeUrlService::~LookalikeUrlService() {}

// static
LookalikeUrlService* LookalikeUrlService::Get(Profile* profile) {
  return LookalikeUrlServiceFactory::GetForProfile(profile);
}

void LookalikeUrlService::GetEngagedSites(EngagedSitesCallback callback) {
  const base::Time now = clock_->Now();

  if (!last_engagement_fetch_time_.is_null()) {
    const base::TimeDelta elapsed = now - last_engagement_fetch_time_;
    if (elapsed <
        base::TimeDelta::FromSeconds(kEngagedSiteUpdateIntervalInSeconds)) {
      std::move(callback).Run(engaged_sites_);
      return;
    }
  }
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &SiteEngagementService::GetAllDetailsInBackground, now,
          base::WrapRefCounted(
              HostContentSettingsMapFactory::GetForProfile(profile_))),
      base::BindOnce(&LookalikeUrlService::OnFetchEngagedSites,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LookalikeUrlService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void LookalikeUrlService::ClearEngagedSitesForTesting() {
  engaged_sites_.clear();
  last_engagement_fetch_time_ = clock_->Now();
}

void LookalikeUrlService::OnFetchEngagedSites(
    EngagedSitesCallback callback,
    std::vector<mojom::SiteEngagementDetails> details) {
  SiteEngagementService* service = SiteEngagementService::Get(profile_);
  engaged_sites_.clear();
  for (const mojom::SiteEngagementDetails& detail : details) {
    // Ignore sites with an engagement score below threshold.
    if (!service->IsEngagementAtLeast(detail.origin,
                                      blink::mojom::EngagementLevel::MEDIUM)) {
      continue;
    }
    engaged_sites_.push_back(detail.origin);
  }

  last_engagement_fetch_time_ = clock_->Now();
  std::move(callback).Run(engaged_sites_);
}
