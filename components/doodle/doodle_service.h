// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOODLE_DOODLE_SERVICE_H_
#define COMPONENTS_DOODLE_DOODLE_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/doodle/doodle_fetcher.h"
#include "components/doodle/doodle_types.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;
class PrefService;

namespace gfx {
class Image;
}

namespace image_fetcher {
class ImageFetcher;
struct RequestMetadata;
}

namespace doodle {

class DoodleService : public KeyedService {
 public:
  class Observer {
   public:
    virtual void OnDoodleConfigRevalidated(bool from_cache) = 0;
    virtual void OnDoodleConfigUpdated(const base::Optional<DoodleConfig>&) = 0;
  };

  using ImageCallback = base::Callback<void(const gfx::Image& image)>;

  static void RegisterProfilePrefs(PrefRegistrySimple* pref_registry);

  // All pointer parameters must be non-null. If |min_refresh_interval| doesn't
  // have a value, the default value is used.
  DoodleService(PrefService* pref_service,
                std::unique_ptr<DoodleFetcher> fetcher,
                std::unique_ptr<base::OneShotTimer> expiry_timer,
                std::unique_ptr<base::Clock> clock,
                std::unique_ptr<base::TickClock> tick_clock,
                base::Optional<base::TimeDelta> override_min_refresh_interval,
                std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher);
  ~DoodleService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Returns the current (cached) config, if any.
  const base::Optional<DoodleConfig>& config() const { return cached_config_; }

  // Returns the image for the currently-cached doodle config via |callback|,
  // which may be run synchronously or asynchronously. If the doodle is
  // animated, this returns the static CTA image.
  void GetImage(const ImageCallback& callback);

  // Adds a new observer to the service. It'll only be called when the config
  // changes; to get the current (cached) config, call |config()|.
  void AddObserver(Observer* observer);

  // Prevents |observer| from receiving future updates. This is safe to call
  // even when the observer is being notified of an update.
  void RemoveObserver(Observer* observer);

  // Requests an asynchronous refresh of the DoodleConfig from the network.
  // After the update completes, the observers will be notified whether the
  // config changed or active config remains valid.
  void Refresh();

 private:
  // Note: Keep in sync with the corresponding enum in histograms.xml. Never
  // remove values, and only insert new values at the end.
  enum DownloadOutcome {
    OUTCOME_NEW_DOODLE = 0,
    OUTCOME_REVALIDATED_DOODLE = 1,
    OUTCOME_CHANGED_DOODLE = 2,
    OUTCOME_NO_DOODLE = 3,
    OUTCOME_EXPIRED = 4,
    OUTCOME_DOWNLOAD_ERROR = 5,
    OUTCOME_PARSING_ERROR = 6,
    OUTCOME_REFRESH_INTERVAL_NOT_PASSED = 7,
    // Insert new values here!
    OUTCOME_COUNT = 8
  };

  static bool DownloadOutcomeIsSuccess(DownloadOutcome outcome);
  static void RecordDownloadMetrics(DownloadOutcome outcome,
                                    base::TimeDelta download_time);

  static DownloadOutcome DetermineDownloadOutcome(
      const base::Optional<DoodleConfig>& old_config,
      const base::Optional<DoodleConfig>& new_config,
      DoodleState state,
      bool expired);

  // Callback for the fetcher.
  void DoodleFetched(base::TimeTicks start_time,
                     DoodleState state,
                     base::TimeDelta time_to_live,
                     const base::Optional<DoodleConfig>& doodle_config);

  DownloadOutcome HandleNewConfig(
      DoodleState state,
      base::TimeDelta time_to_live,
      const base::Optional<DoodleConfig>& doodle_config);

  void UpdateCachedConfig(base::TimeDelta time_to_live,
                          const base::Optional<DoodleConfig>& new_config);

  // Callback for the expiry timer.
  void DoodleExpired();

  void ImageFetched(const ImageCallback& callback,
                    const std::string& id,
                    const gfx::Image& image,
                    const image_fetcher::RequestMetadata& metadata);

  PrefService* pref_service_;

  // The fetcher for getting fresh DoodleConfigs from the network.
  std::unique_ptr<DoodleFetcher> fetcher_;

  std::unique_ptr<base::OneShotTimer> expiry_timer_;
  std::unique_ptr<base::Clock> clock_;
  std::unique_ptr<base::TickClock> tick_clock_;

  // The minimum interval between server fetches. After a successful fetch,
  // refresh requests are ignored for this period.
  const base::TimeDelta min_refresh_interval_;

  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  // The result of the last network fetch.
  base::Optional<DoodleConfig> cached_config_;

  // The time of the most recent successful fetch.
  base::TimeTicks last_successful_fetch_;

  // The list of observers to be notified when the DoodleConfig changes.
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(DoodleService);
};

}  // namespace doodle

#endif  // COMPONENTS_DOODLE_DOODLE_SERVICE_H_
