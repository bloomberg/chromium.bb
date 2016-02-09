// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram.h"

#include "components/dom_distiller/content/common/distiller_messages.h"
#include "components/dom_distiller/content/renderer/distillability_agent.h"
#include "components/dom_distiller/core/distillable_page_detector.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/dom_distiller/core/page_features.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/renderer/render_frame.h"

#include "third_party/WebKit/public/platform/WebDistillability.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace dom_distiller {

using namespace blink;

namespace {

// Returns whether it is necessary to send updates back to the browser.
// The number of updates can be from 0 to 2. See the tests in
// "distillable_page_utils_browsertest.cc".
// Most heuristics types only require one update after parsing.
// Adaboost is the only one doing the second update, which is after loading.
bool NeedToUpdate(bool is_loaded) {
  switch (GetDistillerHeuristicsType()) {
    case DistillerHeuristicsType::ALWAYS_TRUE:
      return !is_loaded;
    case DistillerHeuristicsType::OG_ARTICLE:
      return !is_loaded;
    case DistillerHeuristicsType::ADABOOST_MODEL:
      return true;
    case DistillerHeuristicsType::NONE:
    default:
      return false;
  }
}

// Returns whether this update is the last one for the page.
bool IsLast(bool is_loaded) {
  if (GetDistillerHeuristicsType() == DistillerHeuristicsType::ADABOOST_MODEL)
    return is_loaded;

  return true;
}

bool IsDistillablePageAdaboost(WebDocument& doc,
                               const DistillablePageDetector* detector,
                               bool is_last) {
  WebDistillabilityFeatures features = doc.distillabilityFeatures();
  GURL parsed_url(doc.url());
  if (!parsed_url.is_valid()) {
    return false;
  }
  bool distillable = detector->Classify(CalculateDerivedFeatures(
    features.openGraph,
    parsed_url,
    features.elementCount,
    features.anchorCount,
    features.formCount,
    features.mozScore,
    features.mozScoreAllSqrt,
    features.mozScoreAllLinear
  ));

  int bucket = static_cast<unsigned>(features.isMobileFriendly) |
      (static_cast<unsigned>(distillable) << 1);
  if (is_last) {
    UMA_HISTOGRAM_ENUMERATION("DomDistiller.PageDistillableAfterLoading",
        bucket, 4);
  } else {
    UMA_HISTOGRAM_ENUMERATION("DomDistiller.PageDistillableAfterParsing",
        bucket, 4);
  }
  return distillable && (!features.isMobileFriendly);
}

bool IsDistillablePage(WebDocument& doc, bool is_last) {
  switch (GetDistillerHeuristicsType()) {
    case DistillerHeuristicsType::ALWAYS_TRUE:
      return true;
    case DistillerHeuristicsType::OG_ARTICLE:
      return doc.distillabilityFeatures().openGraph;
    case DistillerHeuristicsType::ADABOOST_MODEL:
      return IsDistillablePageAdaboost(
          doc, DistillablePageDetector::GetNewModel(), is_last);
    case DistillerHeuristicsType::NONE:
    default:
      return false;
  }
}

}  // namespace

DistillabilityAgent::DistillabilityAgent(
    content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
}

void DistillabilityAgent::DidMeaningfulLayout(
    WebMeaningfulLayout layout_type) {
  if (layout_type != WebMeaningfulLayout::FinishedParsing &&
      layout_type != WebMeaningfulLayout::FinishedLoading) {
    return;
  }

  DCHECK(render_frame());
  if (!render_frame()->IsMainFrame()) return;
  DCHECK(render_frame()->GetWebFrame());
  WebDocument doc = render_frame()->GetWebFrame()->document();
  if (doc.isNull() || doc.body().isNull()) return;
  if (!url_utils::IsUrlDistillable(doc.url())) return;

  bool is_loaded = layout_type == WebMeaningfulLayout::FinishedLoading;
  if (!NeedToUpdate(is_loaded)) return;

  bool is_last = IsLast(is_loaded);
  Send(new FrameHostMsg_Distillability(routing_id(),
      IsDistillablePage(doc, is_last), is_last));
}


DistillabilityAgent::~DistillabilityAgent() {}

}  // namespace dom_distiller
