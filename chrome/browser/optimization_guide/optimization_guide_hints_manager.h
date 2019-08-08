// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_HINTS_MANAGER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_HINTS_MANAGER_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/optimization_guide_service_observer.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class NavigationHandle;
}  // namespace content

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace optimization_guide {
class HintCache;
class HintUpdateData;
class OptimizationFilter;
class OptimizationGuideService;
}  // namespace optimization_guide

class PrefService;

class OptimizationGuideHintsManager
    : public optimization_guide::OptimizationGuideServiceObserver {
 public:
  OptimizationGuideHintsManager(
      optimization_guide::OptimizationGuideService* optimization_guide_service,
      const base::FilePath& profile_path,
      PrefService* pref_service,
      leveldb_proto::ProtoDatabaseProvider* database_provider);

  ~OptimizationGuideHintsManager() override;

  // Unhooks the observer to |optimization_guide_service_|.
  void Shutdown();

  // optimization_guide::OptimizationGuideServiceObserver implementation:
  void OnHintsComponentAvailable(
      const optimization_guide::HintsComponentInfo& info) override;

  // |next_update_closure| is called the next time OnHintsComponentAvailable()
  // is called and the corresponding hints have been updated.
  void ListenForNextUpdateForTesting(base::OnceClosure next_update_closure);

  // Loads the hint if available.
  // |callback| is run when the request has finished regardless of whether there
  // was actually a hint for that load or not. The callback can be used as a
  // signal for tests.
  void LoadHintForNavigation(content::NavigationHandle* navigation_handle,
                             base::OnceClosure callback);

  // Registers the optimization types that have the potential for hints to be
  // called by consumers of the Optimization Guide.
  void RegisterOptimizationTypes(
      std::vector<optimization_guide::proto::OptimizationType>
          optimization_types);

  // Returns whether there have been any optimization types registered.
  bool HasRegisteredOptimizationTypes() const {
    return !registered_optimization_types_.empty();
  }

  // Returns whether there is an optimization filter loaded for
  // |optimization_type|.
  bool HasLoadedOptimizationFilter(
      optimization_guide::proto::OptimizationType optimization_type);

 private:
  // Processes the hints component.
  //
  // Should always be called on the thread that belongs to
  // |background_task_runner_|.
  std::unique_ptr<optimization_guide::HintUpdateData> ProcessHintsComponent(
      const optimization_guide::HintsComponentInfo& info,
      const std::unordered_set<optimization_guide::proto::OptimizationType>&
          registered_optimization_types,
      std::unique_ptr<optimization_guide::HintUpdateData> update_data);

  // Processes the optimization filters contained in the hints component.
  //
  // Should always be called on the thread that belongs to
  // |background_task_runner_|.
  void ProcessOptimizationFilters(
      const google::protobuf::RepeatedPtrField<
          optimization_guide::proto::OptimizationFilter>&
          blacklist_optimization_filters,
      const std::unordered_set<optimization_guide::proto::OptimizationType>&
          registered_optimization_types);

  // Callback run after the hint cache is fully initialized. At this point, the
  // OptimizationGuideHintsManager is ready to process hints.
  void OnHintCacheInitialized();

  // Updates the cache with the latest hints sent by the Component Updater.
  void UpdateComponentHints(
      base::OnceClosure update_closure,
      std::unique_ptr<optimization_guide::HintUpdateData> hint_update_data);

  // Called when the hints have been fully updated with the latest hints from
  // the Component Updater. This is used as a signal during tests.
  void OnComponentHintsUpdated(base::OnceClosure update_closure,
                               bool hints_updated) const;

  // Called when the request to load a hint has completed.
  void OnHintLoaded(base::OnceClosure callback,
                    const optimization_guide::proto::Hint* loaded_hint) const;

  // The OptimizationGuideService that this guide is listening to. Not owned.
  optimization_guide::OptimizationGuideService* const
      optimization_guide_service_;

  // The information of the latest component delivered by
  // |optimization_guide_service_|.
  base::Optional<optimization_guide::HintsComponentInfo> hints_component_info_;

  // The set of optimization types that have been registered with the hints
  // manager.
  //
  // Should only be read and modified on the UI thread.
  std::unordered_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types_;

  // Synchronizes access to member variables related to optimization filters.
  base::Lock optimization_filters_lock_;

  // The set of optimization types that the component specified by
  // |component_info_| has optimization filters for.
  std::unordered_set<optimization_guide::proto::OptimizationType>
      available_optimization_filters_ GUARDED_BY(optimization_filters_lock_);

  // A map from optimization type to the host filter that holds the blacklist
  // for that type.
  std::unordered_map<optimization_guide::proto::OptimizationType,
                     std::unique_ptr<optimization_guide::OptimizationFilter>>
      blacklist_optimization_filters_ GUARDED_BY(optimization_filters_lock_);

  // Background thread where hints processing should be performed.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // A reference to the PrefService for this profile. Not owned.
  PrefService* pref_service_ = nullptr;

  // The hint cache that holds both hints received from the component and
  // fetched from the remote Optimization Guide Service.
  std::unique_ptr<optimization_guide::HintCache> hint_cache_;

  // Used in testing to subscribe to an update event in this class.
  base::OnceClosure next_update_closure_;

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<OptimizationGuideHintsManager> ui_weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideHintsManager);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_HINTS_MANAGER_H_
