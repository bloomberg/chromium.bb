// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/optimization_guide/hint_cache.h"
#include "components/optimization_guide/hint_cache_store.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

namespace {

// The component version used with a manual config. This ensures that any hint
// component received from the OptimizationGuideService on a subsequent startup
// will have a newer version than it.
constexpr char kManualConfigComponentVersion[] = "0.0.0";

std::unique_ptr<optimization_guide::HintUpdateData> ProcessHintsComponent(
    const optimization_guide::HintsComponentInfo& info,
    std::unique_ptr<optimization_guide::HintUpdateData> update_data) {
  optimization_guide::ProcessHintsComponentResult out_result;
  std::unique_ptr<optimization_guide::proto::Configuration> config =
      optimization_guide::ProcessHintsComponent(info, &out_result);
  if (!config) {
    optimization_guide::RecordProcessHintsComponentResult(out_result);
    return nullptr;
  }

  bool did_process_hints = optimization_guide::ProcessHints(
      config->mutable_hints(), update_data.get());
  optimization_guide::RecordProcessHintsComponentResult(
      did_process_hints
          ? optimization_guide::ProcessHintsComponentResult::kSuccess
          : optimization_guide::ProcessHintsComponentResult::kProcessedNoHints);

  return update_data;
}

void MaybeRunUpdateClosure(base::OnceClosure update_closure) {
  if (update_closure)
    std::move(update_closure).Run();
}

// Returns whether the particular component version can be processed, and if it
// can be, locks the semaphore (in the form of a pref) to signal that the
// processing of this particular version has started.
bool CanProcessComponentVersion(PrefService* pref_service,
                                const base::Version& version) {
  DCHECK(version.IsValid());

  const std::string previous_attempted_version_string = pref_service->GetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion);
  if (!previous_attempted_version_string.empty()) {
    const base::Version previous_attempted_version =
        base::Version(previous_attempted_version_string);
    if (!previous_attempted_version.IsValid()) {
      DLOG(ERROR) << "Bad contents in hints processing pref";
      // Clear pref for fresh start next time.
      pref_service->ClearPref(
          optimization_guide::prefs::kPendingHintsProcessingVersion);
      return false;
    }
    if (previous_attempted_version.CompareTo(version) == 0) {
      // Previously attempted same version without completion.
      return false;
    }
  }

  // Write config version to pref.
  pref_service->SetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion,
      version.GetString());
  return true;
}

}  // namespace

OptimizationGuideHintsManager::OptimizationGuideHintsManager(
    optimization_guide::OptimizationGuideService* optimization_guide_service,
    const base::FilePath& profile_path,
    PrefService* pref_service,
    leveldb_proto::ProtoDatabaseProvider* database_provider)
    : optimization_guide_service_(optimization_guide_service),
      background_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT})),
      pref_service_(pref_service),
      hint_cache_(std::make_unique<optimization_guide::HintCache>(
          std::make_unique<optimization_guide::HintCacheStore>(
              database_provider,
              profile_path,
              pref_service_,
              background_task_runner_))) {
  DCHECK(optimization_guide_service_);
  hint_cache_->Initialize(
      optimization_guide::switches::ShouldPurgeHintCacheStoreOnStartup(),
      base::BindOnce(&OptimizationGuideHintsManager::OnHintCacheInitialized,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

OptimizationGuideHintsManager::~OptimizationGuideHintsManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  optimization_guide_service_->RemoveObserver(this);
}

void OptimizationGuideHintsManager::Shutdown() {
  optimization_guide_service_->RemoveObserver(this);
}

void OptimizationGuideHintsManager::OnHintsComponentAvailable(
    const optimization_guide::HintsComponentInfo& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check for if hint component is disabled. This check is needed because the
  // optimization guide still registers with the service as an observer for
  // components as a signal during testing.
  if (optimization_guide::switches::IsHintComponentProcessingDisabled()) {
    MaybeRunUpdateClosure(std::move(next_update_closure_));
    return;
  }

  std::unique_ptr<optimization_guide::HintUpdateData> update_data =
      hint_cache_->MaybeCreateUpdateDataForComponentHints(info.version);
  if (!update_data) {
    optimization_guide::RecordProcessHintsComponentResult(
        optimization_guide::ProcessHintsComponentResult::
            kSkippedProcessingHints);
    MaybeRunUpdateClosure(std::move(next_update_closure_));
    return;
  }

  if (!CanProcessComponentVersion(pref_service_, info.version)) {
    optimization_guide::RecordProcessHintsComponentResult(
        optimization_guide::ProcessHintsComponentResult::
            kFailedFinishProcessing);
    MaybeRunUpdateClosure(std::move(next_update_closure_));
    return;
  }

  // Processes the hints from the newly available component on a background
  // thread, providing a HintUpdateData for component update from the hint
  // cache, so that each hint within the component can be moved into it. In the
  // case where the component's version is not newer than the hint cache store's
  // component version, HintUpdateData will be a nullptr and hint
  // processing will be skipped. After PreviewsHints::Create() returns the newly
  // created PreviewsHints, it is initialized in UpdateHints() on the UI thread.
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ProcessHintsComponent, info, std::move(update_data)),
      base::BindOnce(&OptimizationGuideHintsManager::UpdateComponentHints,
                     ui_weak_ptr_factory_.GetWeakPtr(),
                     std::move(next_update_closure_)));
}

void OptimizationGuideHintsManager::OnHintCacheInitialized() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check if there is a valid hint proto given on the command line first. We
  // don't normally expect one, but if one is provided then use that and do not
  // register as an observer as the opt_guide service.
  std::unique_ptr<optimization_guide::proto::Configuration> manual_config =
      optimization_guide::switches::ParseComponentConfigFromCommandLine();
  if (manual_config) {
    std::unique_ptr<optimization_guide::HintUpdateData> hint_update_data =
        hint_cache_->MaybeCreateUpdateDataForComponentHints(
            base::Version(kManualConfigComponentVersion));
    optimization_guide::ProcessHints(manual_config->mutable_hints(),
                                     hint_update_data.get());
    // Allow |UpdateComponentHints| to block startup so that the first
    // navigation gets the hints when a command line hint proto is provided.
    UpdateComponentHints(base::DoNothing(), std::move(hint_update_data));
  }

  // Register as an observer regardless of hint proto override usage. This is
  // needed as a signal during testing.
  optimization_guide_service_->AddObserver(this);
}

void OptimizationGuideHintsManager::UpdateComponentHints(
    base::OnceClosure update_closure,
    std::unique_ptr<optimization_guide::HintUpdateData> hint_update_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If we get here, the hints have been processed correctly.
  pref_service_->ClearPref(
      optimization_guide::prefs::kPendingHintsProcessingVersion);

  if (hint_update_data) {
    hint_cache_->UpdateComponentHints(
        std::move(hint_update_data),
        base::BindOnce(&OptimizationGuideHintsManager::OnComponentHintsUpdated,
                       ui_weak_ptr_factory_.GetWeakPtr(),
                       std::move(update_closure),
                       /* hints_updated=*/true));
  } else {
    OnComponentHintsUpdated(std::move(update_closure), /*hints_updated=*/false);
  }
}

void OptimizationGuideHintsManager::OnComponentHintsUpdated(
    base::OnceClosure update_closure,
    bool hints_updated) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Record the result of updating the hints. This is used as a signal for the
  // hints being fully processed in testing.
  LOCAL_HISTOGRAM_BOOLEAN(
      optimization_guide::kComponentHintsUpdatedResultHistogramString,
      hints_updated);

  MaybeRunUpdateClosure(std::move(update_closure));
}

void OptimizationGuideHintsManager::ListenForNextUpdateForTesting(
    base::OnceClosure next_update_closure) {
  DCHECK(!next_update_closure_)
      << "Only one update closure is supported at a time";
  next_update_closure_ = std::move(next_update_closure);
}

void OptimizationGuideHintsManager::LoadHintForNavigation(
    content::NavigationHandle* navigation_handle,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const auto& url = navigation_handle->GetURL();
  if (!url.has_host()) {
    std::move(callback).Run();
    return;
  }

  hint_cache_->LoadHint(
      url.host(),
      base::BindOnce(&OptimizationGuideHintsManager::OnHintLoaded,
                     ui_weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OptimizationGuideHintsManager::OnHintLoaded(
    base::OnceClosure callback,
    const optimization_guide::proto::Hint* loaded_hint) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Record the result of loading a hint. This is used as a signal for testing.
  LOCAL_HISTOGRAM_BOOLEAN("OptimizationGuide.LoadedHint.Result", loaded_hint);

  // Run the callback now that the hint is loaded. This is used as a signal by
  // tests.
  std::move(callback).Run();
}
