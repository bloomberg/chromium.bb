// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_optimization_guide.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/content/hint_cache_leveldb_store.h"
#include "components/previews/content/previews_hints.h"
#include "components/previews/content/previews_hints_util.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_constants.h"
#include "components/previews/core/previews_switches.h"
#include "url/gurl.h"

namespace previews {

namespace {

// The component version used with a manual config. This ensures that any hint
// component received from the OptimizationGuideService on a subsequent startup
// will have a newer version than it.
constexpr char kManualConfigComponentVersion[] = "0.0.0";

// Hints are purged during startup if the explicit purge switch exists or if
// a proto override is being used--in which case the hints need to come from the
// override instead.
bool ShouldPurgeHintCacheStoreOnStartup() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  return cmd_line->HasSwitch(switches::kHintsProtoOverride) ||
         cmd_line->HasSwitch(switches::kPurgeHintCacheStore);
}

// Available hint components are only processed if a proto override isn't being
// used; otherwise, the hints from the proto override are used instead.
bool IsHintComponentProcessingDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kHintsProtoOverride);
}

// Attempts to parse a base64 encoded Optimization Guide Configuration proto
// from the command line. If no proto is given or if it is encoded incorrectly,
// nullptr is returned.
std::unique_ptr<optimization_guide::proto::Configuration>
ParseHintsProtoFromCommandLine() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kHintsProtoOverride))
    return nullptr;

  std::string b64_pb =
      cmd_line->GetSwitchValueASCII(switches::kHintsProtoOverride);

  std::string binary_pb;
  if (!base::Base64Decode(b64_pb, &binary_pb)) {
    LOG(ERROR) << "Invalid base64 encoding of the Hints Proto Override";
    return nullptr;
  }

  std::unique_ptr<optimization_guide::proto::Configuration>
      proto_configuration =
          std::make_unique<optimization_guide::proto::Configuration>();
  if (!proto_configuration->ParseFromString(binary_pb)) {
    LOG(ERROR) << "Invalid proto provided to the Hints Proto Override";
    return nullptr;
  }

  return proto_configuration;
}

}  // namespace

PreviewsOptimizationGuide::PreviewsOptimizationGuide(
    optimization_guide::OptimizationGuideService* optimization_guide_service,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const base::FilePath& profile_path)
    : optimization_guide_service_(optimization_guide_service),
      ui_task_runner_(ui_task_runner),
      background_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      hint_cache_(std::make_unique<HintCache>(
          std::make_unique<HintCacheLevelDBStore>(profile_path,
                                                  background_task_runner_))),
      ui_weak_ptr_factory_(this) {
  DCHECK(optimization_guide_service_);
  hint_cache_->Initialize(
      ShouldPurgeHintCacheStoreOnStartup(),
      base::BindOnce(&PreviewsOptimizationGuide::OnHintCacheInitialized,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

PreviewsOptimizationGuide::~PreviewsOptimizationGuide() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  optimization_guide_service_->RemoveObserver(this);
}

bool PreviewsOptimizationGuide::IsWhitelisted(
    PreviewsUserData* previews_data,
    const GURL& url,
    PreviewsType type,
    net::EffectiveConnectionType* out_ect_threshold) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (!hints_) {
    return false;
  }

  *out_ect_threshold = params::GetECTThresholdForPreview(type);
  int inflation_percent = 0;
  if (!hints_->IsWhitelisted(url, type, &inflation_percent,
                             out_ect_threshold)) {
    return false;
  }

  if (inflation_percent != 0 && previews_data) {
    previews_data->set_data_savings_inflation_percent(inflation_percent);
  }

  return true;
}

bool PreviewsOptimizationGuide::IsBlacklisted(const GURL& url,
                                              PreviewsType type) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (!hints_) {
    return false;
  }

  return hints_->IsBlacklisted(url, type);
}

void PreviewsOptimizationGuide::OnLoadedHint(
    base::OnceClosure callback,
    const GURL& document_url,
    const optimization_guide::proto::Hint* loaded_hint) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Record that the hint finished loading. This is used as a signal during
  // tests.
  LOCAL_HISTOGRAM_BOOLEAN(
      kPreviewsOptimizationGuideOnLoadedHintResultHistogramString, loaded_hint);

  // Run the callback now that the hint is loaded. This is used as a signal by
  // tests.
  std::move(callback).Run();
}

bool PreviewsOptimizationGuide::MaybeLoadOptimizationHints(
    const GURL& url,
    base::OnceClosure callback) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (!hints_) {
    return false;
  }

  return hints_->MaybeLoadOptimizationHints(
      url, base::BindOnce(&PreviewsOptimizationGuide::OnLoadedHint,
                          ui_weak_ptr_factory_.GetWeakPtr(),
                          std::move(callback), url));
}

bool PreviewsOptimizationGuide::GetResourceLoadingHints(
    const GURL& url,
    std::vector<std::string>* out_resource_patterns_to_block) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (!hints_)
    return false;

  return hints_->GetResourceLoadingHints(url, out_resource_patterns_to_block);
}

void PreviewsOptimizationGuide::LogHintCacheMatch(
    const GURL& url,
    bool is_committed,
    net::EffectiveConnectionType ect) const {
  if (!hints_) {
    return;
  }

  hints_->LogHintCacheMatch(url, is_committed, ect);
}

void PreviewsOptimizationGuide::OnHintCacheInitialized() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  // Check if there is a valid hint proto given on the command line first. We
  // don't normally expect one, but if one is provided then use that and do not
  // register as an observer as the opt_guide service.
  std::unique_ptr<optimization_guide::proto::Configuration> manual_config =
      ParseHintsProtoFromCommandLine();
  if (manual_config) {
    // Allow |UpdateHints| to block startup so that the first navigation gets
    // the hints when a command line hint proto is provided.
    UpdateHints(PreviewsHints::CreateFromHintsConfiguration(
        std::move(manual_config),
        hint_cache_->MaybeCreateComponentUpdateData(
            base::Version(kManualConfigComponentVersion))));
  }
  // Register as an observer regardless of hint proto override usage. This is
  // needed as a signal during testing.
  optimization_guide_service_->AddObserver(this);
}

void PreviewsOptimizationGuide::OnHintsComponentAvailable(
    const optimization_guide::HintsComponentInfo& info) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Check for if hint component is disabled. This check is needed because the
  // optimization guide still registers with the service as an observer for
  // components as a signal during testing.
  if (IsHintComponentProcessingDisabled()) {
    return;
  }

  // Create PreviewsHints from the newly available component on a background
  // thread, providing a ComponentUpdateData from the hint cache, so that each
  // hint within the component can be moved into it. In the case where the
  // component's version is not newer than the hint cache store's component
  // version, ComponentUpdateData will be a nullptr and hint processing will be
  // skipped. After PreviewsHints::Create() returns the newly created
  // PreviewsHints, it is initialized in UpdateHints() on the UI thread.
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&PreviewsHints::CreateFromHintsComponent, info,
                     hint_cache_->MaybeCreateComponentUpdateData(info.version)),
      base::BindOnce(&PreviewsOptimizationGuide::UpdateHints,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void PreviewsOptimizationGuide::UpdateHints(
    std::unique_ptr<PreviewsHints> hints) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  hints_ = std::move(hints);
  if (hints_) {
    hints_->Initialize(
        hint_cache_.get(),
        base::BindOnce(&PreviewsOptimizationGuide::OnHintsUpdated,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnHintsUpdated();
  }
}

void PreviewsOptimizationGuide::OnHintsUpdated() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  // Record the result of updating the hints. This is used as a signal for the
  // hints being fully processed in testing.
  LOCAL_HISTOGRAM_BOOLEAN(
      kPreviewsOptimizationGuideUpdateHintsResultHistogramString,
      hints_ != NULL);
}

}  // namespace previews
