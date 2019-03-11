// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "components/optimization_guide/optimization_guide_service_observer.h"
#include "components/previews/content/hint_cache.h"
#include "components/previews/core/previews_experiments.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}  // namespace base
namespace optimization_guide {
struct HintsComponentInfo;
class OptimizationGuideService;
namespace proto {
class Hint;
}  // namespace proto
}  // namespace optimization_guide

namespace previews {

class HintsFetcher;
class PreviewsHints;
class PreviewsTopHostProvider;
class PreviewsUserData;

// A Previews optimization guide that makes decisions guided by hints received
// from the OptimizationGuideService.
class PreviewsOptimizationGuide
    : public optimization_guide::OptimizationGuideServiceObserver {
 public:
  // The embedder guarantees |optimization_guide_service| outlives |this|.
  // The embedder guarantees that |previews_top_host_provider_| outlives |this|.
  PreviewsOptimizationGuide(
      optimization_guide::OptimizationGuideService* optimization_guide_service,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const base::FilePath& profile_path,
      PreviewsTopHostProvider* previews_top_host_provider);

  ~PreviewsOptimizationGuide() override;

  // Returns whether |type| is whitelisted for |url|. If so |out_ect_threshold|
  // provides the maximum effective connection type to trigger the preview for.
  // |previews_data| can be modified (for further details provided by hints).
  // Virtual so it can be mocked in tests.
  virtual bool IsWhitelisted(
      PreviewsUserData* previews_data,
      const GURL& url,
      PreviewsType type,
      net::EffectiveConnectionType* out_ect_threshold) const;

  // Returns whether |type| is blacklisted for |url|.
  // Virtual so it can be mocked in tests.
  virtual bool IsBlacklisted(const GURL& url, PreviewsType type) const;

  // Returns whether |request| may have associated optimization hints
  // (specifically, PageHints). If so, but the hints are not available
  // synchronously, this method will request that they be loaded (from disk or
  // network). The callback is run after the hint is loaded and can be used as
  // a signal during tests.
  bool MaybeLoadOptimizationHints(const GURL& url, base::OnceClosure callback);

  // Whether |url| has loaded resource loading hints and, if it does, populates
  // |out_resource_patterns_to_block| with the resource patterns to block.
  bool GetResourceLoadingHints(
      const GURL& url,
      std::vector<std::string>* out_resource_patterns_to_block) const;

  // Logs UMA for whether the OptimizationGuide HintCache has a matching Hint
  // guidance for |url|. This is useful for measuring the effectiveness of the
  // page hints provided by Cacao.
  void LogHintCacheMatch(const GURL& url,
                         bool is_committed,
                         net::EffectiveConnectionType ect) const;

  // optimization_guide::OptimizationGuideServiceObserver implementation:
  // Called by OptimizationGuideService when a new component is available for
  // processing.
  void OnHintsComponentAvailable(
      const optimization_guide::HintsComponentInfo& info) override;

  PreviewsHints* GetHintsForTesting() { return hints_.get(); }

  // |next_update_closure| is called the next time OnHintsComponentAvailable is
  // called and the corresponding hints have been updated.
  void ListenForNextUpdateForTesting(base::OnceClosure next_update_closure);

 private:
  // Callback run after the hint cache is fully initialized. At this point, the
  // PreviewsOptimizationGuide is ready to process components from the
  // OptimizationGuideService and registers as an observer with it.
  void OnHintCacheInitialized();

  // Updates the hints to the latest hints sent by the Component Updater.
  // |update_closure| is called once the hints are updated.
  void UpdateHints(base::OnceClosure update_closure,
                   std::unique_ptr<PreviewsHints> hints);

  // Called when the hints have been fully updated with the latest hints from
  // the Component Updater. This is used as a signal during tests.
  // |update_closure| is called immediately if not null.
  void OnHintsUpdated(base::OnceClosure update_closure);

  // Callback when a hint is loaded.
  void OnLoadedHint(base::OnceClosure callback,
                    const GURL& document_url,
                    const optimization_guide::proto::Hint* loaded_hint) const;

  // Method to request OnePlatform client hints for user's sites with top
  // engagement scores and creates a remote request using |hints_fetcher_| On
  // request success OnOnePlatformHintsReceived callback will be called.
  void GetOnePlatformClientHints();

  // Called when the response from the OnePlatform Guide Service is handled and
  // stored by the |hints_fetcher_|. received.
  void OnOnePlatformHintsReceived();

  // The OptimizationGuideService that this guide is listening to. Not owned.
  optimization_guide::OptimizationGuideService* optimization_guide_service_;

  // Runner for UI thread tasks.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Background thread where hints processing should be performed.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // The hint cache used by PreviewsHints. It is owned by
  // PreviewsOptimizationGuide so that the existing hint cache can be reused on
  // component updates. Otherwise, a new cache and store would need to be
  // created during each component update.
  std::unique_ptr<HintCache> hint_cache_;

  // The current hints used for this optimization guide.
  std::unique_ptr<PreviewsHints> hints_;

  // Used in testing to subscribe to an update event in this class.
  base::OnceClosure next_update_closure_;

  // HintsFetcher handles the request to update Hints from OnePlatform Guide
  // Service.
  std::unique_ptr<HintsFetcher> hints_fetcher_;

  // TopHostProvider that this guide can query. Not owned.
  PreviewsTopHostProvider* previews_top_host_provider_ = nullptr;

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<PreviewsOptimizationGuide> ui_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsOptimizationGuide);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_
