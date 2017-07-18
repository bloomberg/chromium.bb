// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_update_dispatcher.h"

#include <ostream>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "chrome/browser/page_load_metrics/browser_page_track_decider.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_embedder_interface.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/common/page_load_metrics/page_load_metrics_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"

namespace page_load_metrics {

namespace internal {

const char kPageLoadTimingStatus[] = "PageLoad.Internal.PageLoadTimingStatus";
const char kPageLoadTimingDispatchStatus[] =
    "PageLoad.Internal.PageLoadTimingStatus.AtTimingCallbackDispatch";
const char kHistogramOutOfOrderTiming[] =
    "PageLoad.Internal.OutOfOrderInterFrameTiming";
const char kHistogramOutOfOrderTimingBuffered[] =
    "PageLoad.Internal.OutOfOrderInterFrameTiming.AfterBuffering";

}  // namespace internal

namespace {

// Helper to allow use of Optional<> values in LOG() messages.
std::ostream& operator<<(std::ostream& os,
                         const base::Optional<base::TimeDelta>& opt) {
  if (opt)
    os << opt.value();
  else
    os << "(unset)";
  return os;
}

// If second is non-zero, first must also be non-zero and less than or equal to
// second.
bool EventsInOrder(const base::Optional<base::TimeDelta>& first,
                   const base::Optional<base::TimeDelta>& second) {
  if (!second) {
    return true;
  }
  return first && first <= second;
}

internal::PageLoadTimingStatus IsValidPageLoadTiming(
    const mojom::PageLoadTiming& timing) {
  if (page_load_metrics::IsEmpty(timing))
    return internal::INVALID_EMPTY_TIMING;

  // If we have a non-empty timing, it should always have a navigation start.
  if (timing.navigation_start.is_null()) {
    LOG(ERROR) << "Received null navigation_start.";
    return internal::INVALID_NULL_NAVIGATION_START;
  }

  // Verify proper ordering between the various timings.

  if (!EventsInOrder(timing.response_start, timing.parse_timing->parse_start)) {
    // We sometimes get a zero response_start with a non-zero parse start. See
    // crbug.com/590212.
    LOG(ERROR) << "Invalid response_start " << timing.response_start
               << " for parse_start " << timing.parse_timing->parse_start;
    // When browser-side navigation is enabled, we sometimes encounter this
    // error case. For now, we disable reporting of this error, since most
    // PageLoadMetricsObservers don't care about response_start and we want to
    // see how much closer fixing this error will get us to page load metrics
    // being consistent with and without browser side navigation enabled. See
    // crbug.com/716587 for more details.
    //
    // return internal::INVALID_ORDER_RESPONSE_START_PARSE_START;
  }

  if (!EventsInOrder(timing.parse_timing->parse_start,
                     timing.parse_timing->parse_stop)) {
    LOG(ERROR) << "Invalid parse_start " << timing.parse_timing->parse_start
               << " for parse_stop " << timing.parse_timing->parse_stop;
    return internal::INVALID_ORDER_PARSE_START_PARSE_STOP;
  }

  if (timing.parse_timing->parse_stop) {
    const base::TimeDelta parse_duration =
        timing.parse_timing->parse_stop.value() -
        timing.parse_timing->parse_start.value();
    if (timing.parse_timing->parse_blocked_on_script_load_duration >
        parse_duration) {
      LOG(ERROR) << "Invalid parse_blocked_on_script_load_duration "
                 << timing.parse_timing->parse_blocked_on_script_load_duration
                 << " for parse duration " << parse_duration;
      return internal::INVALID_SCRIPT_LOAD_LONGER_THAN_PARSE;
    }
    if (timing.parse_timing->parse_blocked_on_script_execution_duration >
        parse_duration) {
      LOG(ERROR)
          << "Invalid parse_blocked_on_script_execution_duration "
          << timing.parse_timing->parse_blocked_on_script_execution_duration
          << " for parse duration " << parse_duration;
      return internal::INVALID_SCRIPT_EXEC_LONGER_THAN_PARSE;
    }
  }

  if (timing.parse_timing
          ->parse_blocked_on_script_load_from_document_write_duration >
      timing.parse_timing->parse_blocked_on_script_load_duration) {
    LOG(ERROR)
        << "Invalid parse_blocked_on_script_load_from_document_write_duration "
        << timing.parse_timing
               ->parse_blocked_on_script_load_from_document_write_duration
        << " for parse_blocked_on_script_load_duration "
        << timing.parse_timing->parse_blocked_on_script_load_duration;
    return internal::INVALID_SCRIPT_LOAD_DOC_WRITE_LONGER_THAN_SCRIPT_LOAD;
  }

  if (timing.parse_timing
          ->parse_blocked_on_script_execution_from_document_write_duration >
      timing.parse_timing->parse_blocked_on_script_execution_duration) {
    LOG(ERROR)
        << "Invalid "
           "parse_blocked_on_script_execution_from_document_write_duration "
        << timing.parse_timing
               ->parse_blocked_on_script_execution_from_document_write_duration
        << " for parse_blocked_on_script_execution_duration "
        << timing.parse_timing->parse_blocked_on_script_execution_duration;
    return internal::INVALID_SCRIPT_EXEC_DOC_WRITE_LONGER_THAN_SCRIPT_EXEC;
  }

  if (!EventsInOrder(timing.parse_timing->parse_stop,
                     timing.document_timing->dom_content_loaded_event_start)) {
    LOG(ERROR) << "Invalid parse_stop " << timing.parse_timing->parse_stop
               << " for dom_content_loaded_event_start "
               << timing.document_timing->dom_content_loaded_event_start;
    return internal::INVALID_ORDER_PARSE_STOP_DOM_CONTENT_LOADED;
  }

  if (!EventsInOrder(timing.document_timing->dom_content_loaded_event_start,
                     timing.document_timing->load_event_start)) {
    LOG(ERROR) << "Invalid dom_content_loaded_event_start "
               << timing.document_timing->dom_content_loaded_event_start
               << " for load_event_start "
               << timing.document_timing->load_event_start;
    return internal::INVALID_ORDER_DOM_CONTENT_LOADED_LOAD;
  }

  if (!EventsInOrder(timing.parse_timing->parse_start,
                     timing.document_timing->first_layout)) {
    LOG(ERROR) << "Invalid parse_start " << timing.parse_timing->parse_start
               << " for first_layout " << timing.document_timing->first_layout;
    return internal::INVALID_ORDER_PARSE_START_FIRST_LAYOUT;
  }

  if (!EventsInOrder(timing.document_timing->first_layout,
                     timing.paint_timing->first_paint)) {
    // This can happen when we process an XHTML document that doesn't contain
    // well formed XML. See crbug.com/627607.
    DLOG(ERROR) << "Invalid first_layout "
                << timing.document_timing->first_layout << " for first_paint "
                << timing.paint_timing->first_paint;
    return internal::INVALID_ORDER_FIRST_LAYOUT_FIRST_PAINT;
  }

  if (!EventsInOrder(timing.paint_timing->first_paint,
                     timing.paint_timing->first_text_paint)) {
    LOG(ERROR) << "Invalid first_paint " << timing.paint_timing->first_paint
               << " for first_text_paint "
               << timing.paint_timing->first_text_paint;
    return internal::INVALID_ORDER_FIRST_PAINT_FIRST_TEXT_PAINT;
  }

  if (!EventsInOrder(timing.paint_timing->first_paint,
                     timing.paint_timing->first_image_paint)) {
    LOG(ERROR) << "Invalid first_paint " << timing.paint_timing->first_paint
               << " for first_image_paint "
               << timing.paint_timing->first_image_paint;
    return internal::INVALID_ORDER_FIRST_PAINT_FIRST_IMAGE_PAINT;
  }

  if (!EventsInOrder(timing.paint_timing->first_paint,
                     timing.paint_timing->first_contentful_paint)) {
    LOG(ERROR) << "Invalid first_paint " << timing.paint_timing->first_paint
               << " for first_contentful_paint "
               << timing.paint_timing->first_contentful_paint;
    return internal::INVALID_ORDER_FIRST_PAINT_FIRST_CONTENTFUL_PAINT;
  }

  if (!EventsInOrder(timing.paint_timing->first_paint,
                     timing.paint_timing->first_meaningful_paint)) {
    LOG(ERROR) << "Invalid first_paint " << timing.paint_timing->first_paint
               << " for first_meaningful_paint "
               << timing.paint_timing->first_meaningful_paint;
    return internal::INVALID_ORDER_FIRST_PAINT_FIRST_MEANINGFUL_PAINT;
  }

  return internal::VALID;
}

// If the updated value has an earlier time than the current value, log so we
// can keep track of how often this happens.
void LogIfOutOfOrderTiming(const base::Optional<base::TimeDelta>& current,
                           const base::Optional<base::TimeDelta>& update) {
  if (!current || !update)
    return;

  if (update < current) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramOutOfOrderTimingBuffered,
                        current.value() - update.value());
  }
}

// PaintTimingMerger merges paint timing values received from different frames
// together.
class PaintTimingMerger {
 public:
  explicit PaintTimingMerger(mojom::PaintTiming* target) : target_(target) {}

  // Merge paint timing values from |new_paint_timing| into the target
  // PaintTiming.
  void Merge(base::TimeDelta navigation_start_offset,
             const mojom::PaintTiming& new_paint_timing,
             bool is_main_frame);

  // Whether we merged a new value, for a paint timing field we didn't
  // previously have a value for in the target PaintTiming.
  bool did_merge_new_timing_value() const {
    return did_merge_new_timing_value_;
  }

 private:
  void MaybeUpdateTimeDelta(
      base::Optional<base::TimeDelta>* inout_existing_value,
      base::TimeDelta navigation_start_offset,
      const base::Optional<base::TimeDelta>& optional_candidate_new_value);

  // The target PaintTiming we are merging values into.
  mojom::PaintTiming* const target_;

  // Whether we merged a new value, for a paint timing field we didn't
  // previously have a value for in |target_|.
  bool did_merge_new_timing_value_ = false;

  DISALLOW_COPY_AND_ASSIGN(PaintTimingMerger);
};

// Updates *|inout_existing_value| with |optional_candidate_new_value|, if
// either *|inout_existing_value| isn't set, or |optional_candidate_new_value| <
// |inout_existing_value|.
void PaintTimingMerger::MaybeUpdateTimeDelta(
    base::Optional<base::TimeDelta>* inout_existing_value,
    base::TimeDelta navigation_start_offset,
    const base::Optional<base::TimeDelta>& optional_candidate_new_value) {
  // If we don't get a new value, there's nothing to do
  if (!optional_candidate_new_value)
    return;

  // optional_candidate_new_value is relative to navigation start in its
  // frame. We need to adjust it to be relative to navigation start in the main
  // frame, so offset it by navigation_start_offset.
  base::TimeDelta candidate_new_value =
      navigation_start_offset + optional_candidate_new_value.value();

  if (*inout_existing_value) {
    // If we have a new value, but it is after the existing value, then keep the
    // existing value.
    if (*inout_existing_value <= candidate_new_value)
      return;

    // We received a new timing event, but with a timestamp before the timestamp
    // of a timing update we had received previously. We expect this to happen
    // occasionally, as inter-frame updates can arrive out of order. Record a
    // histogram to track how frequently it happens, along with the magnitude
    // of the delta.
    PAGE_LOAD_HISTOGRAM(internal::kHistogramOutOfOrderTiming,
                        inout_existing_value->value() - candidate_new_value);
  } else {
    did_merge_new_timing_value_ = true;
  }

  *inout_existing_value = candidate_new_value;
}

void PaintTimingMerger::Merge(base::TimeDelta navigation_start_offset,
                              const mojom::PaintTiming& new_paint_timing,
                              bool is_main_frame) {
  MaybeUpdateTimeDelta(&target_->first_paint, navigation_start_offset,
                       new_paint_timing.first_paint);
  MaybeUpdateTimeDelta(&target_->first_text_paint, navigation_start_offset,
                       new_paint_timing.first_text_paint);
  MaybeUpdateTimeDelta(&target_->first_image_paint, navigation_start_offset,
                       new_paint_timing.first_image_paint);
  MaybeUpdateTimeDelta(&target_->first_contentful_paint,
                       navigation_start_offset,
                       new_paint_timing.first_contentful_paint);
  if (is_main_frame) {
    // First meaningful paint is only tracked in the main frame.
    target_->first_meaningful_paint = new_paint_timing.first_meaningful_paint;
  }
}

}  // namespace

PageLoadMetricsUpdateDispatcher::PageLoadMetricsUpdateDispatcher(
    PageLoadMetricsUpdateDispatcher::Client* client,
    content::NavigationHandle* navigation_handle,
    PageLoadMetricsEmbedderInterface* embedder_interface)
    : client_(client),
      embedder_interface_(embedder_interface),
      timer_(embedder_interface->CreateTimer()),
      navigation_start_(navigation_handle->NavigationStart()),
      current_merged_page_timing_(CreatePageLoadTiming()),
      pending_merged_page_timing_(CreatePageLoadTiming()),
      main_frame_metadata_(mojom::PageLoadMetadata::New()),
      subframe_metadata_(mojom::PageLoadMetadata::New()) {}

PageLoadMetricsUpdateDispatcher::~PageLoadMetricsUpdateDispatcher() {
  ShutDown();
}

void PageLoadMetricsUpdateDispatcher::ShutDown() {
  if (timer_ && timer_->IsRunning()) {
    timer_->Stop();
    DispatchTimingUpdates();
  }
  timer_ = nullptr;
}

void PageLoadMetricsUpdateDispatcher::UpdateMetrics(
    content::RenderFrameHost* render_frame_host,
    const mojom::PageLoadTiming& new_timing,
    const mojom::PageLoadMetadata& new_metadata,
    const mojom::PageLoadFeatures& new_features) {
  if (render_frame_host->GetParent() == nullptr) {
    UpdateMainFrameMetadata(new_metadata);
    UpdateMainFrameTiming(new_timing);
  } else {
    UpdateSubFrameMetadata(new_metadata);
    UpdateSubFrameTiming(render_frame_host, new_timing);
  }
  client_->UpdateFeaturesUsage(new_features);
}

void PageLoadMetricsUpdateDispatcher::DidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  // We have a new committed navigation, so discard information about the
  // previously committed navigation.
  subframe_navigation_start_offset_.erase(
      navigation_handle->GetFrameTreeNodeId());

  BrowserPageTrackDecider decider(embedder_interface_,
                                  navigation_handle->GetWebContents(),
                                  navigation_handle);
  if (!decider.ShouldTrack())
    return;

  if (navigation_start_ > navigation_handle->NavigationStart()) {
    RecordInternalError(ERR_SUBFRAME_NAVIGATION_START_BEFORE_MAIN_FRAME);
    return;
  }
  base::TimeDelta navigation_delta =
      navigation_handle->NavigationStart() - navigation_start_;
  subframe_navigation_start_offset_.insert(std::make_pair(
      navigation_handle->GetFrameTreeNodeId(), navigation_delta));
}

void PageLoadMetricsUpdateDispatcher::UpdateSubFrameTiming(
    content::RenderFrameHost* render_frame_host,
    const mojom::PageLoadTiming& new_timing) {
  const auto it = subframe_navigation_start_offset_.find(
      render_frame_host->GetFrameTreeNodeId());
  if (it == subframe_navigation_start_offset_.end()) {
    // We received timing information for an untracked load. Ignore it.
    return;
  }

  client_->OnSubFrameTimingChanged(new_timing);

  base::TimeDelta navigation_start_offset = it->second;
  PaintTimingMerger merger(pending_merged_page_timing_->paint_timing.get());
  merger.Merge(navigation_start_offset, *new_timing.paint_timing,
               false /* is_main_frame */);

  MaybeDispatchTimingUpdates(merger.did_merge_new_timing_value());
}

void PageLoadMetricsUpdateDispatcher::UpdateSubFrameMetadata(
    const mojom::PageLoadMetadata& subframe_metadata) {
  // Merge the subframe loading behavior flags with any we've already observed,
  // possibly from other subframes.
  const int last_subframe_loading_behavior_flags =
      subframe_metadata_->behavior_flags;
  subframe_metadata_->behavior_flags |= subframe_metadata.behavior_flags;
  if (last_subframe_loading_behavior_flags ==
      subframe_metadata_->behavior_flags)
    return;

  client_->OnSubframeMetadataChanged();
}

void PageLoadMetricsUpdateDispatcher::UpdateMainFrameTiming(
    const mojom::PageLoadTiming& new_timing) {
  // Throw away IPCs that are not relevant to the current navigation.
  // Two timing structures cannot refer to the same navigation if they indicate
  // that a navigation started at different times, so a new timing struct with a
  // different start time from an earlier struct is considered invalid.
  const bool valid_timing_descendent =
      pending_merged_page_timing_->navigation_start.is_null() ||
      pending_merged_page_timing_->navigation_start ==
          new_timing.navigation_start;
  if (!valid_timing_descendent) {
    RecordInternalError(ERR_BAD_TIMING_IPC_INVALID_TIMING_DESCENDENT);
    return;
  }

  internal::PageLoadTimingStatus status = IsValidPageLoadTiming(new_timing);
  UMA_HISTOGRAM_ENUMERATION(internal::kPageLoadTimingStatus, status,
                            internal::LAST_PAGE_LOAD_TIMING_STATUS);
  if (status != internal::VALID) {
    RecordInternalError(ERR_BAD_TIMING_IPC_INVALID_TIMING);
    return;
  }

  mojom::PaintTimingPtr last_paint_timing =
      std::move(pending_merged_page_timing_->paint_timing);
  // Update the pending_merged_page_timing_, making sure to merge the previously
  // observed |paint_timing|, which is tracked across all frames in the page.
  pending_merged_page_timing_ = new_timing.Clone();
  pending_merged_page_timing_->paint_timing = std::move(last_paint_timing);

  PaintTimingMerger merger(pending_merged_page_timing_->paint_timing.get());
  merger.Merge(base::TimeDelta(), *new_timing.paint_timing,
               true /* is_main_frame */);

  MaybeDispatchTimingUpdates(merger.did_merge_new_timing_value());
}

void PageLoadMetricsUpdateDispatcher::UpdateMainFrameMetadata(
    const mojom::PageLoadMetadata& new_metadata) {
  if (main_frame_metadata_->Equals(new_metadata))
    return;

  // Ensure flags sent previously are still present in the new metadata fields.
  const bool valid_behavior_descendent =
      (main_frame_metadata_->behavior_flags & new_metadata.behavior_flags) ==
      main_frame_metadata_->behavior_flags;
  if (!valid_behavior_descendent) {
    RecordInternalError(ERR_BAD_TIMING_IPC_INVALID_BEHAVIOR_DESCENDENT);
    return;
  }

  main_frame_metadata_ = new_metadata.Clone();
  client_->OnMainFrameMetadataChanged();
}

void PageLoadMetricsUpdateDispatcher::MaybeDispatchTimingUpdates(
    bool did_merge_new_timing_value) {
  // If we merged a new timing value, then we should buffer updates for
  // |kBufferTimerDelayMillis|, to allow for any other out of order timings to
  // arrive before we dispatch the minimum observed timings to observers.
  if (did_merge_new_timing_value) {
    timer_->Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kBufferTimerDelayMillis),
        base::Bind(&PageLoadMetricsUpdateDispatcher::DispatchTimingUpdates,
                   base::Unretained(this)));
  } else if (!timer_->IsRunning()) {
    DispatchTimingUpdates();
  }
}

void PageLoadMetricsUpdateDispatcher::DispatchTimingUpdates() {
  DCHECK(!timer_->IsRunning());

  if (pending_merged_page_timing_->paint_timing->first_paint) {
    if (!pending_merged_page_timing_->parse_timing->parse_start ||
        !pending_merged_page_timing_->document_timing->first_layout) {
      // When merging paint events across frames, we can sometimes encounter
      // cases where we've received a first paint event for a child frame before
      // receiving required earlier events in the main frame, due to buffering
      // in the render process which results in out of order delivery. For
      // example, we may receive a notification for a first paint in a child
      // frame before we've received a notification for parse start or first
      // layout in the main frame. In these cases, we delay sending timing
      // updates until we've received all expected events (e.g. wait to receive
      // a parse or layout event before dispatching a paint event), so observers
      // can make assumptions about ordering of these events in their callbacks.
      return;
    }
  }

  if (current_merged_page_timing_->Equals(*pending_merged_page_timing_))
    return;

  LogIfOutOfOrderTiming(current_merged_page_timing_->paint_timing->first_paint,
                        pending_merged_page_timing_->paint_timing->first_paint);
  LogIfOutOfOrderTiming(
      current_merged_page_timing_->paint_timing->first_text_paint,
      pending_merged_page_timing_->paint_timing->first_text_paint);
  LogIfOutOfOrderTiming(
      current_merged_page_timing_->paint_timing->first_image_paint,
      pending_merged_page_timing_->paint_timing->first_image_paint);
  LogIfOutOfOrderTiming(
      current_merged_page_timing_->paint_timing->first_contentful_paint,
      pending_merged_page_timing_->paint_timing->first_contentful_paint);

  current_merged_page_timing_ = pending_merged_page_timing_->Clone();

  internal::PageLoadTimingStatus status =
      IsValidPageLoadTiming(*pending_merged_page_timing_);
  UMA_HISTOGRAM_ENUMERATION(internal::kPageLoadTimingDispatchStatus, status,
                            internal::LAST_PAGE_LOAD_TIMING_STATUS);

  client_->OnTimingChanged();
}

}  // namespace page_load_metrics
