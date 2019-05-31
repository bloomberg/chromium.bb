// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/page_load_metrics/metrics_render_frame_observer.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/renderer/page_load_metrics/page_timing_metrics_sender.h"
#include "chrome/renderer/page_load_metrics/page_timing_sender.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_performance.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

base::TimeDelta ClampDelta(double event, double start) {
  if (event - start < 0)
    event = start;
  return base::Time::FromDoubleT(event) - base::Time::FromDoubleT(start);
}

class MojoPageTimingSender : public PageTimingSender {
 public:
  explicit MojoPageTimingSender(content::RenderFrame* render_frame) {
    DCHECK(render_frame);
    render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
        &page_load_metrics_);
  }
  ~MojoPageTimingSender() override {}
  void SendTiming(
      const mojom::PageLoadTimingPtr& timing,
      const mojom::PageLoadMetadataPtr& metadata,
      mojom::PageLoadFeaturesPtr new_features,
      std::vector<mojom::ResourceDataUpdatePtr> resources,
      const mojom::FrameRenderDataUpdate& render_data,
      const mojom::CpuTimingPtr& cpu_timing,
      mojom::DeferredResourceCountsPtr new_deferred_resource_data) override {
    DCHECK(page_load_metrics_);
    page_load_metrics_->UpdateTiming(
        timing->Clone(), metadata->Clone(), std::move(new_features),
        std::move(resources), render_data.Clone(), cpu_timing->Clone(),
        std::move(new_deferred_resource_data));
  }

 private:
  // Use associated interface to make sure mojo messages are ordered with regard
  // to legacy IPC messages.
  mojom::PageLoadMetricsAssociatedPtr page_load_metrics_;
};

}  //  namespace

MetricsRenderFrameObserver::MetricsRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      scoped_ad_resource_observer_(this) {}

MetricsRenderFrameObserver::~MetricsRenderFrameObserver() {}

void MetricsRenderFrameObserver::DidChangePerformanceTiming() {
  SendMetrics();
}

void MetricsRenderFrameObserver::DidChangeCpuTiming(base::TimeDelta time) {
  if (!page_timing_metrics_sender_)
    return;
  if (HasNoRenderFrame())
    return;
  page_timing_metrics_sender_->UpdateCpuTiming(time);
}

void MetricsRenderFrameObserver::DidObserveLoadingBehavior(
    blink::WebLoadingBehaviorFlag behavior) {
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->DidObserveLoadingBehavior(behavior);
}

void MetricsRenderFrameObserver::DidObserveNewFeatureUsage(
    blink::mojom::WebFeature feature) {
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->DidObserveNewFeatureUsage(feature);
}

void MetricsRenderFrameObserver::DidObserveNewCssPropertyUsage(
    int css_property,
    bool is_animated) {
  if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidObserveNewCssPropertyUsage(css_property,
                                                               is_animated);
  }
}

void MetricsRenderFrameObserver::DidObserveLayoutJank(
    double jank_fraction,
    bool after_input_or_scroll) {
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->DidObserveLayoutJank(jank_fraction,
                                                      after_input_or_scroll);
}

void MetricsRenderFrameObserver::DidObserveLazyLoadBehavior(
    blink::WebLocalFrameClient::LazyLoadBehavior lazy_load_behavior) {
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->DidObserveLazyLoadBehavior(lazy_load_behavior);
}

void MetricsRenderFrameObserver::DidStartResponse(
    const GURL& response_url,
    int request_id,
    const network::ResourceResponseHead& response_head,
    content::ResourceType resource_type,
    content::PreviewsState previews_state) {
  if (provisional_frame_resource_data_use_ &&
      content::IsResourceTypeFrame(resource_type)) {
    // TODO(rajendrant): This frame request might start before the provisional
    // load starts, and data use of the frame request might be missed in that
    // case. There should be a guarantee that DidStartProvisionalLoad be called
    // before DidStartResponse for the frame request.
    provisional_frame_resource_data_use_->DidStartResponse(
        response_url, request_id, response_head, resource_type, previews_state);
  } else if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidStartResponse(
        response_url, request_id, response_head, resource_type, previews_state);
    UpdateResourceMetadata(request_id);
  }
}

void MetricsRenderFrameObserver::DidCompleteResponse(
    int request_id,
    const network::URLLoaderCompletionStatus& status) {
  if (provisional_frame_resource_data_use_ &&
      provisional_frame_resource_data_use_->resource_id() == request_id) {
    provisional_frame_resource_data_use_->DidCompleteResponse(status);
  } else if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidCompleteResponse(request_id, status);
    // The provisional frame can be flagged as an ad anytime during the load.
    if (request_id == provisional_frame_resource_id)
      UpdateResourceMetadata(request_id);
  }
}

void MetricsRenderFrameObserver::DidCancelResponse(int request_id) {
  if (provisional_frame_resource_data_use_ &&
      provisional_frame_resource_data_use_->resource_id() == request_id) {
    provisional_frame_resource_data_use_.reset();
  } else if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidCancelResponse(request_id);
    // The provisional frame can be flagged as an ad anytime during the load.
    if (request_id == provisional_frame_resource_id)
      UpdateResourceMetadata(request_id);
  }
}

void MetricsRenderFrameObserver::DidReceiveTransferSizeUpdate(
    int request_id,
    int received_data_length) {
  if (provisional_frame_resource_data_use_ &&
      provisional_frame_resource_data_use_->resource_id() == request_id) {
    provisional_frame_resource_data_use_->DidReceiveTransferSizeUpdate(
        received_data_length);
  } else if (page_timing_metrics_sender_) {
    page_timing_metrics_sender_->DidReceiveTransferSizeUpdate(
        request_id, received_data_length);
    // The provisional frame can be flagged as an ad anytime during the load.
    if (request_id == provisional_frame_resource_id)
      UpdateResourceMetadata(request_id);
  }
}

void MetricsRenderFrameObserver::FrameDetached() {
  page_timing_metrics_sender_.reset();
}

void MetricsRenderFrameObserver::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  // Create a new data use tracker for the new document load.
  provisional_frame_resource_data_use_ =
      std::make_unique<PageResourceDataUse>();
  provisional_frame_resource_id = 0;
}

void MetricsRenderFrameObserver::DidFailProvisionalLoad(
    const blink::WebURLError& error) {
  // Clear the data use tracker for the provisional navigation that started.
  provisional_frame_resource_data_use_.reset();
}

void MetricsRenderFrameObserver::DidCommitProvisionalLoad(
    bool is_same_document_navigation,
    ui::PageTransition transition) {
  // Same-document navigations (e.g. a navigation from a fragment link) aren't
  // full page loads, since they don't go to network to load the main HTML
  // resource. DidStartProvisionalLoad doesn't get invoked for same document
  // navigations, so we may still have an active page_timing_metrics_sender_ at
  // this point.
  if (is_same_document_navigation)
    return;

  // Make sure to release the sender for a previous navigation, if we have one.
  page_timing_metrics_sender_.reset();

  if (HasNoRenderFrame())
    return;

  // Update metadata once the load has been committed. There is no guarantee
  // that the provisional resource will have been reported as an ad by this
  // point. Therefore, need to update metadata for the resource after the load.
  // Consumers may receive the correct ad information late.
  UpdateResourceMetadata(provisional_frame_resource_data_use_->resource_id());

  provisional_frame_resource_id =
      provisional_frame_resource_data_use_->resource_id();

  page_timing_metrics_sender_ = std::make_unique<PageTimingMetricsSender>(
      CreatePageTimingSender(), CreateTimer(), GetTiming(),
      std::move(provisional_frame_resource_data_use_));
}

void MetricsRenderFrameObserver::SetAdResourceTracker(
    subresource_filter::AdResourceTracker* ad_resource_tracker) {
  // Remove all sources and set a new source for the observer.
  scoped_ad_resource_observer_.RemoveAll();
  scoped_ad_resource_observer_.Add(ad_resource_tracker);
}

void MetricsRenderFrameObserver::OnAdResourceTrackerGoingAway() {
  scoped_ad_resource_observer_.RemoveAll();
}

void MetricsRenderFrameObserver::OnAdResourceObserved(int request_id) {
  ad_request_ids_.insert(request_id);
}

void MetricsRenderFrameObserver::UpdateResourceMetadata(int request_id) {
  if (!page_timing_metrics_sender_)
    return;
  // If the request is an ad, pop it off the list of known ad requests.
  auto ad_resource_it = ad_request_ids_.find(request_id);
  bool reported_as_ad_resource =
      ad_request_ids_.find(request_id) != ad_request_ids_.end();
  if (reported_as_ad_resource)
    ad_request_ids_.erase(ad_resource_it);
  bool is_main_frame_resource = render_frame()->IsMainFrame();
  if (provisional_frame_resource_data_use_ &&
      provisional_frame_resource_data_use_->resource_id() == request_id) {
    if (reported_as_ad_resource)
      provisional_frame_resource_data_use_->SetReportedAsAdResource(
          reported_as_ad_resource);
    provisional_frame_resource_data_use_->SetIsMainFrameResource(
        is_main_frame_resource);
  } else {
    page_timing_metrics_sender_->UpdateResourceMetadata(
        request_id, reported_as_ad_resource, is_main_frame_resource);
  }
}

void MetricsRenderFrameObserver::SendMetrics() {
  if (!page_timing_metrics_sender_)
    return;
  if (HasNoRenderFrame())
    return;
  page_timing_metrics_sender_->Send(GetTiming());
}

mojom::PageLoadTimingPtr MetricsRenderFrameObserver::GetTiming() const {
  const blink::WebPerformance& perf =
      render_frame()->GetWebFrame()->Performance();

  mojom::PageLoadTimingPtr timing(CreatePageLoadTiming());
  double start = perf.NavigationStart();
  timing->navigation_start = base::Time::FromDoubleT(start);
  if (perf.InputForNavigationStart() > 0.0) {
    timing->input_to_navigation_start =
        ClampDelta(start, perf.InputForNavigationStart());
  }
  if (perf.PageInteractive() > 0.0) {
    // PageInteractive and PageInteractiveDetection should be available at the
    // same time. This is a renderer side DCHECK to ensure this.
    DCHECK(perf.PageInteractiveDetection());
    timing->interactive_timing->interactive =
        ClampDelta(perf.PageInteractive(), start);
    timing->interactive_timing->interactive_detection =
        ClampDelta(perf.PageInteractiveDetection(), start);
  }
  if (perf.FirstInputInvalidatingInteractive() > 0.0) {
    timing->interactive_timing->first_invalidating_input =
        ClampDelta(perf.FirstInputInvalidatingInteractive(), start);
  }
  if (perf.FirstInputDelay() > 0.0) {
    timing->interactive_timing->first_input_delay =
        base::TimeDelta::FromSecondsD(perf.FirstInputDelay());
  }
  if (perf.FirstInputTimestamp() > 0.0) {
    timing->interactive_timing->first_input_timestamp =
        ClampDelta(perf.FirstInputTimestamp(), start);
  }
  if (perf.LongestInputDelay() > 0.0) {
    timing->interactive_timing->longest_input_delay =
        base::TimeDelta::FromSecondsD(perf.LongestInputDelay());
  }
  if (perf.LongestInputTimestamp() > 0.0) {
    timing->interactive_timing->longest_input_timestamp =
        ClampDelta(perf.LongestInputTimestamp(), start);
  }
  if (perf.ResponseStart() > 0.0)
    timing->response_start = ClampDelta(perf.ResponseStart(), start);
  if (perf.DomContentLoadedEventStart() > 0.0) {
    timing->document_timing->dom_content_loaded_event_start =
        ClampDelta(perf.DomContentLoadedEventStart(), start);
  }
  if (perf.LoadEventStart() > 0.0) {
    timing->document_timing->load_event_start =
        ClampDelta(perf.LoadEventStart(), start);
  }
  if (perf.FirstLayout() > 0.0)
    timing->document_timing->first_layout =
        ClampDelta(perf.FirstLayout(), start);
  if (perf.FirstPaint() > 0.0)
    timing->paint_timing->first_paint = ClampDelta(perf.FirstPaint(), start);
  if (perf.FirstImagePaint() > 0.0) {
    timing->paint_timing->first_image_paint =
        ClampDelta(perf.FirstImagePaint(), start);
  }
  if (perf.FirstContentfulPaint() > 0.0) {
    timing->paint_timing->first_contentful_paint =
        ClampDelta(perf.FirstContentfulPaint(), start);
  }
  if (perf.FirstMeaningfulPaint() > 0.0) {
    timing->paint_timing->first_meaningful_paint =
        ClampDelta(perf.FirstMeaningfulPaint(), start);
  }
  if (perf.LargestImagePaintSize() > 0) {
    timing->paint_timing->largest_image_paint =
        perf.LargestImagePaint() == 0.0
            ? base::TimeDelta()
            : ClampDelta(perf.LargestImagePaint(), start);
    timing->paint_timing->largest_image_paint_size =
        perf.LargestImagePaintSize();
  }
  if (perf.LargestTextPaintSize() > 0) {
    timing->paint_timing->largest_text_paint =
        perf.LargestTextPaint() == 0.0
            ? base::TimeDelta()
            : ClampDelta(perf.LargestTextPaint(), start);
    timing->paint_timing->largest_text_paint_size = perf.LargestTextPaintSize();
  }
  if (perf.ParseStart() > 0.0)
    timing->parse_timing->parse_start = ClampDelta(perf.ParseStart(), start);
  if (perf.ParseStop() > 0.0)
    timing->parse_timing->parse_stop = ClampDelta(perf.ParseStop(), start);
  if (timing->parse_timing->parse_start) {
    // If we started parsing, record all parser durations such as the amount of
    // time blocked on script load, even if those values are zero.
    timing->parse_timing->parse_blocked_on_script_load_duration =
        base::TimeDelta::FromSecondsD(perf.ParseBlockedOnScriptLoadDuration());
    timing->parse_timing
        ->parse_blocked_on_script_load_from_document_write_duration =
        base::TimeDelta::FromSecondsD(
            perf.ParseBlockedOnScriptLoadFromDocumentWriteDuration());
    timing->parse_timing->parse_blocked_on_script_execution_duration =
        base::TimeDelta::FromSecondsD(
            perf.ParseBlockedOnScriptExecutionDuration());
    timing->parse_timing
        ->parse_blocked_on_script_execution_from_document_write_duration =
        base::TimeDelta::FromSecondsD(
            perf.ParseBlockedOnScriptExecutionFromDocumentWriteDuration());
  }

  return timing;
}

std::unique_ptr<base::OneShotTimer> MetricsRenderFrameObserver::CreateTimer() {
  return std::make_unique<base::OneShotTimer>();
}

std::unique_ptr<PageTimingSender>
MetricsRenderFrameObserver::CreatePageTimingSender() {
  return base::WrapUnique<PageTimingSender>(
      new MojoPageTimingSender(render_frame()));
}

bool MetricsRenderFrameObserver::HasNoRenderFrame() const {
  bool no_frame = !render_frame() || !render_frame()->GetWebFrame();
  DCHECK(!no_frame);
  return no_frame;
}

void MetricsRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace page_load_metrics
