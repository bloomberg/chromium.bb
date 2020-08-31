// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MANAGER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "url/origin.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class NavigationHandle;
}  // namespace content

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class PrefService;
class Profile;

namespace optimization_guide {

enum class OptimizationGuideDecision;
class OptimizationGuideStore;
class PredictionModel;
class PredictionModelFetcher;
class TopHostProvider;

using HostModelFeaturesMRUCache =
    base::HashingMRUCache<std::string, base::flat_map<std::string, float>>;

// A PredictionManager supported by the optimization guide that makes an
// OptimizationTargetDecision by evaluating the corresponding prediction model
// for an OptimizationTarget.
class PredictionManager
    : public network::NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  PredictionManager(
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          optimization_targets_at_initialization,
      const base::FilePath& profile_path,
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      TopHostProvider* top_host_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      Profile* profile);

  PredictionManager(
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          optimization_targets_at_initialization,
      std::unique_ptr<OptimizationGuideStore> model_and_features_store,
      TopHostProvider* top_host_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      Profile* profile);

  ~PredictionManager() override;

  // Register the optimization targets that may have ShouldTargetNavigtation
  // requested by consumers of the Optimization Guide.
  void RegisterOptimizationTargets(
      const std::vector<proto::OptimizationTarget>& optimization_targets);

  // Determine if the navigation matches the critieria for
  // |optimization_target|. Return kUnknown if a PredictionModel for the
  // optimization target is not registered and kModelNotAvailableOnClient if the
  // if model for the optimization target is not currently on the client.
  OptimizationTargetDecision ShouldTargetNavigation(
      content::NavigationHandle* navigation_handle,
      proto::OptimizationTarget optimization_target);

  // Update |session_fcp_| and |previous_fcp_| with |fcp|.
  void UpdateFCPSessionStatistics(base::TimeDelta fcp);

  OptimizationGuideSessionStatistic* GetFCPSessionStatisticsForTesting() const {
    return const_cast<OptimizationGuideSessionStatistic*>(&session_fcp_);
  }

  // network::NetworkQualityTracker::EffectiveConnectionTypeObserver
  // implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override;

  // Set the prediction model fetcher for testing.
  void SetPredictionModelFetcherForTesting(
      std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher);

  PredictionModelFetcher* prediction_model_fetcher() const {
    return prediction_model_fetcher_.get();
  }

  OptimizationGuideStore* model_and_features_store() const {
    return model_and_features_store_.get();
  }

  // Return the optimization targets that are registered.
  base::flat_set<optimization_guide::proto::OptimizationTarget>
  registered_optimization_targets() const {
    return registered_optimization_targets_;
  }

  // Override |clock_| for testing.
  void SetClockForTesting(const base::Clock* clock);

  // Clear host model features from the in memory host model features map and
  // from the models and features store.
  void ClearHostModelFeatures();

 protected:
  // Return the prediction model for the optimization target used by this
  // PredictionManager for testing.
  PredictionModel* GetPredictionModelForTesting(
      proto::OptimizationTarget optimization_target) const;

  // Return the host model features for all hosts used by this
  // PredictionManager for testing.
  const HostModelFeaturesMRUCache* GetHostModelFeaturesForTesting() const;

  // Returns the host model features for a host if available.
  base::Optional<base::flat_map<std::string, float>>
  GetHostModelFeaturesForHost(const std::string& host) const;

  // Return the set of features that each host in |host_model_features_map_|
  // contains for testing.
  base::flat_set<std::string> GetSupportedHostModelFeaturesForTesting() const;

  // Create a PredictionModel, virtual for testing.
  virtual std::unique_ptr<PredictionModel> CreatePredictionModel(
      const proto::PredictionModel& model) const;

  // Process |host_model_features| to be stored in memory in the host model
  // features map for immediate use and asynchronously write them to the model
  // and features store to be persisted.
  void UpdateHostModelFeatures(
      const google::protobuf::RepeatedPtrField<proto::HostModelFeatures>&
          host_model_features);

  // Process |prediction_models| to be stored in the in memory optimization
  // target prediction model map for immediate use and asynchronously write the
  // models to the model and features store to be persisted.
  void UpdatePredictionModels(
      const google::protobuf::RepeatedPtrField<proto::PredictionModel>&
          prediction_models);

 private:
  // Called on construction to register optimization targets, initialize the
  // prediction model and host model features store, and register as an observer
  // to the network quality tracker.
  void Initialize(const std::vector<proto::OptimizationTarget>&
                      optimization_targets_at_intialization);

  // Construct and return a map containing the current feature values for the
  // requested set of model features. The host model features cache is updated
  // based on if host model features were used.
  base::flat_map<std::string, float> BuildFeatureMap(
      content::NavigationHandle* navigation_handle,
      const base::flat_set<std::string>& model_features);

  // Calculate and return the current value for the client feature specified
  // by |model_feature|. Return nullopt if the client does not support the
  // model feature.
  base::Optional<float> GetValueForClientFeature(
      const std::string& model_feature,
      content::NavigationHandle* navigation_handle) const;

  // Called to make a request to fetch models and host model features from the
  // remote Optimization Guide Service. Used to fetch models for the registered
  // optimization targets as well as the host model features for top hosts
  // needed to evaluate these models.
  void FetchModelsAndHostModelFeatures();

  // Callback when the models and host model features have been fetched from the
  // remote Optimization Guide Service and are ready for parsing. Processes the
  // prediction models and the host model features in the response and stores
  // them for use. The metadata entry containing the time that updates should be
  // fetched from the remote Optimization Guide Service is updated, even when
  // the response is empty.
  void OnModelsAndHostFeaturesFetched(
      base::Optional<std::unique_ptr<proto::GetModelsResponse>>
          get_models_response_data);

  // Callback run after the model and host model features store is fully
  // initialized. The prediction manager can load models from
  // the store for registered optimization targets. |store_is_ready_| is set to
  // true.
  void OnStoreInitialized();

  // Callback run after prediction models are stored in
  // |model_and_features_store_|.
  void OnPredictionModelsStored();

  // Callback run after host model features are stored in
  // |model_and_features_store_|. |fetch_timer_| is stopped and the timer is
  // rescheduled based on when models and host model features should be fetched
  // again.
  void OnHostModelFeaturesStored();

  // Request the store to load all the host model features it contains. This
  // must be completed before any prediction models can be loaded from the
  // store.
  void LoadHostModelFeatures();

  // Callback run after host model features are loaded from the store and are
  // ready to be processed and placed in |host_model_features_map_|.
  // |host_model_features_loaded_| is set to true when called. Prediction models
  // for all registered optimization targets that are not already loaded are
  // requested to be loaded.
  void OnLoadHostModelFeatures(
      std::unique_ptr<std::vector<proto::HostModelFeatures>>
          all_host_model_features);

  // Load models for every target in |optimization_targets| that have not yet
  // been loaded from the store.
  void LoadPredictionModels(
      const base::flat_set<proto::OptimizationTarget>& optimization_targets);

  // Callback run after a prediction model is loaded from the store.
  // |prediction_model| is used to construct a PredictionModel capable of making
  // prediction for the appropriate optimization target.
  void OnLoadPredictionModel(
      std::unique_ptr<proto::PredictionModel> prediction_model);

  // Process |model| into a PredictionModel object and store it in the
  // |optimization_target_prediction_model_map_|. Return true if a prediction
  // model object was created and successfully stored, otherwise false.
  bool ProcessAndStorePredictionModel(const proto::PredictionModel& model);

  // Process |host_model_features| from the into host model features
  // usable by the PredictionManager. The processed host model features are
  // stored in |host_model_features_map_|. Return true if host model features
  // can be constructed and successfully stored, otherwise, return false.
  bool ProcessAndStoreHostModelFeatures(
      const proto::HostModelFeatures& host_model_features);

  // Return the time when a prediction model and host model features fetch was
  // last attempted.
  base::Time GetLastFetchAttemptTime() const;

  // Set the last time when a prediction model and host model features fetch
  // was last attempted to |last_attempt_time|.
  void SetLastModelAndFeaturesFetchAttemptTime(base::Time last_attempt_time);

  // Determine whether to schedule fetching new prediction models and host model
  // features or fetch immediately due to override.
  void MaybeScheduleModelAndHostModelFeaturesFetch();

  // Schedule |fetch_timer_| to fire based on:
  // 1. The update time for host model features in the store and
  // 2. The last time a fetch attempt was made.
  void ScheduleModelsAndHostModelFeaturesFetch();

  // A map of optimization target to the prediction model capable of making
  // an optimization target decision for it.
  base::flat_map<proto::OptimizationTarget, std::unique_ptr<PredictionModel>>
      optimization_target_prediction_model_map_;

  // The set of optimization targets that have been registered with the
  // prediction manager.
  base::flat_set<proto::OptimizationTarget> registered_optimization_targets_;

  // A MRU cache of host to host model features known to the prediction manager.
  HostModelFeaturesMRUCache host_model_features_cache_;

  // The current session's FCP statistics for HTTP/HTTPS navigations.
  OptimizationGuideSessionStatistic session_fcp_;

  // A float representation of the time to FCP of the previous HTTP/HTTPS page
  // load. This is nullopt when no previous page load exists (the first page
  // load of a session).
  base::Optional<float> previous_load_fcp_ms_;

  // The fetcher than handles making requests to update the models and host
  // model features from the remote Optimization Guide Service.
  std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher_;

  // The top host provider that can be queried. Not owned.
  TopHostProvider* top_host_provider_ = nullptr;

  // The optimization guide store that contains prediction models and host
  // model features from the remote Optimization Guide Service.
  std::unique_ptr<OptimizationGuideStore> model_and_features_store_;

  // A stored response from a model and host model features fetch used to hold
  // models to be stored once host model features are processed and stored.
  std::unique_ptr<proto::GetModelsResponse> get_models_response_data_to_store_;

  // The URL loader factory used for fetching model and host feature updates
  // from the remote Optimization Guide Service.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The current estimate of the EffectiveConnectionType.
  net::EffectiveConnectionType current_effective_connection_type_ =
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  // A reference to the PrefService for this profile. Not owned.
  PrefService* pref_service_ = nullptr;

  // A reference to the profile. Not owned.
  Profile* profile_ = nullptr;

  // The timer used to schedule fetching prediction models and host model
  // features from the remote Optimization Guide Service.
  base::OneShotTimer fetch_timer_;

  // The clock used to schedule fetching from the remote Optimization Guide
  // Service.
  const base::Clock* clock_;

  // Whether the |model_and_features_store_| is initialized and ready for use.
  bool store_is_ready_ = false;

  // Whether host model features have been loaded from the store and are ready
  // for use.
  bool host_model_features_loaded_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<PredictionManager> ui_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PredictionManager);
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MANAGER_H_
