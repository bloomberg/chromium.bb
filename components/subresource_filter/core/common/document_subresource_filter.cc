// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/document_subresource_filter.h"

#include <utility>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "components/subresource_filter/core/common/first_party_origin.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

namespace {

ActivationState ComputeActivationStateImpl(
    const GURL& document_url,
    const url::Origin& parent_document_origin,
    const ActivationState& parent_activation_state,
    const IndexedRulesetMatcher& matcher) {
  ActivationState activation_state = parent_activation_state;
  if (activation_state.filtering_disabled_for_document)
    return activation_state;

  // TODO(pkalinnikov): Match several activation types in a batch.
  if (matcher.ShouldDisableFilteringForDocument(
          document_url, parent_document_origin,
          proto::ACTIVATION_TYPE_DOCUMENT)) {
    activation_state.filtering_disabled_for_document = true;
  } else if (!activation_state.generic_blocking_rules_disabled &&
             matcher.ShouldDisableFilteringForDocument(
                 document_url, parent_document_origin,
                 proto::ACTIVATION_TYPE_GENERICBLOCK)) {
    activation_state.generic_blocking_rules_disabled = true;
  }
  return activation_state;
}

}  // namespace

ActivationState ComputeActivationState(
    const GURL& document_url,
    const url::Origin& parent_document_origin,
    const ActivationState& parent_activation_state,
    const MemoryMappedRuleset* ruleset) {
  DCHECK(ruleset);
  IndexedRulesetMatcher matcher(ruleset->data(), ruleset->length());
  return ComputeActivationStateImpl(document_url, parent_document_origin,
                                    parent_activation_state, matcher);
}

ActivationState ComputeActivationState(
    ActivationLevel activation_level,
    bool measure_performance,
    const std::vector<GURL>& ancestor_document_urls,
    const MemoryMappedRuleset* ruleset) {
  SCOPED_UMA_HISTOGRAM_MICRO_TIMER(
      "SubresourceFilter.DocumentLoad.Activation.WallDuration");
  SCOPED_UMA_HISTOGRAM_MICRO_THREAD_TIMER(
      "SubresourceFilter.DocumentLoad.Activation.CPUDuration");

  ActivationState activation_state(activation_level);
  activation_state.measure_performance = measure_performance;
  DCHECK(ruleset);

  IndexedRulesetMatcher matcher(ruleset->data(), ruleset->length());

  url::Origin parent_document_origin;
  for (auto iter = ancestor_document_urls.rbegin(),
            rend = ancestor_document_urls.rend();
       iter != rend; ++iter) {
    const GURL& document_url(*iter);
    activation_state = ComputeActivationStateImpl(
        document_url, parent_document_origin, activation_state, matcher);
    parent_document_origin = url::Origin(document_url);
  }

  return activation_state;
}

DocumentSubresourceFilter::DocumentSubresourceFilter(
    url::Origin document_origin,
    ActivationState activation_state,
    scoped_refptr<const MemoryMappedRuleset> ruleset)
    : activation_state_(activation_state),
      ruleset_(std::move(ruleset)),
      ruleset_matcher_(ruleset_->data(), ruleset_->length()) {
  DCHECK_NE(activation_state_.activation_level, ActivationLevel::DISABLED);
  if (!activation_state_.filtering_disabled_for_document)
    document_origin_.reset(new FirstPartyOrigin(std::move(document_origin)));
}

DocumentSubresourceFilter::~DocumentSubresourceFilter() = default;

LoadPolicy DocumentSubresourceFilter::GetLoadPolicy(
    const GURL& subresource_url,
    proto::ElementType subresource_type) {
  TRACE_EVENT1("loader", "DocumentSubresourceFilter::GetLoadPolicy", "url",
               subresource_url.spec());

  ++statistics_.num_loads_total;

  if (activation_state_.filtering_disabled_for_document)
    return LoadPolicy::ALLOW;
  if (subresource_url.SchemeIs(url::kDataScheme))
    return LoadPolicy::ALLOW;

  auto wall_duration_timer = ScopedTimers::StartIf(
      activation_state_.measure_performance &&
          ScopedThreadTimers::IsSupported(),
      [this](base::TimeDelta delta) {
        statistics_.evaluation_total_wall_duration += delta;
        UMA_HISTOGRAM_MICRO_TIMES(
            "SubresourceFilter.SubresourceLoad.Evaluation.WallDuration", delta);
      });
  auto cpu_duration_timer = ScopedThreadTimers::StartIf(
      activation_state_.measure_performance, [this](base::TimeDelta delta) {
        statistics_.evaluation_total_cpu_duration += delta;
        UMA_HISTOGRAM_MICRO_TIMES(
            "SubresourceFilter.SubresourceLoad.Evaluation.CPUDuration", delta);
      });

  ++statistics_.num_loads_evaluated;
  DCHECK(document_origin_);
  if (ruleset_matcher_.ShouldDisallowResourceLoad(
          subresource_url, *document_origin_, subresource_type,
          activation_state_.generic_blocking_rules_disabled)) {
    ++statistics_.num_loads_matching_rules;
    if (activation_state_.activation_level == ActivationLevel::ENABLED) {
      ++statistics_.num_loads_disallowed;
      return LoadPolicy::DISALLOW;
    } else if (activation_state_.activation_level == ActivationLevel::DRYRUN) {
      return LoadPolicy::WOULD_DISALLOW;
    }
  }
  return LoadPolicy::ALLOW;
}

}  // namespace subresource_filter
