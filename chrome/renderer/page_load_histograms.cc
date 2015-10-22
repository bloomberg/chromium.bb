// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/page_load_histograms.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/searchbox/search_bouncer.h"
#include "components/data_reduction_proxy/content/common/data_reduction_proxy_messages.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/url_pattern.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebPerformance.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/renderer/extensions/chrome_extensions_renderer_client.h"
#include "extensions/renderer/dispatcher.h"
#endif

using blink::WebDataSource;
using blink::WebFrame;
using blink::WebPerformance;
using blink::WebString;
using base::Time;
using base::TimeDelta;
using content::DocumentState;

const size_t kPLTCount = 100;

namespace {

// ID indicating that no GWS-Chrome joint experiment is active.
const int kNoExperiment = 0;

// Max ID of GWS-Chrome joint experiment. If you change this value, please
// update PLT_HISTOGRAM_WITH_GWS_VARIANT accordingly.
const int kMaxExperimentID = 20;

TimeDelta kPLTMin() {
  return TimeDelta::FromMilliseconds(10);
}
TimeDelta kPLTMax() {
  return TimeDelta::FromMinutes(10);
}

// This function corresponds to PLT_HISTOGRAM macro invocation without caching.
// Use this for PLT histograms with dynamically generated names, which
// otherwise can't use the caching PLT_HISTOGRAM macro without code duplication.
void PltHistogramWithNoMacroCaching(const std::string& name,
                                    const TimeDelta& sample) {
  // The parameters should exacly match the parameters in
  // UMA_HISTOGRAM_CUSTOM_TIMES macro.
  base::HistogramBase* histogram_pointer = base::Histogram::FactoryTimeGet(
      name, kPLTMin(), kPLTMax(), kPLTCount,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->AddTime(sample);
}

// This records UMA corresponding to the PLT_HISTOGRAM macro without caching.
void PltHistogramWithGwsPreview(const char* name,
                                const TimeDelta& sample,
                                bool is_preview,
                                int experiment_id) {
  std::string preview_suffix = is_preview ? "_Preview" : "_NoPreview";
  PltHistogramWithNoMacroCaching(name + preview_suffix, sample);

  if (experiment_id != kNoExperiment) {
    std::string name_with_experiment_id = base::StringPrintf(
          "%s%s_Experiment%d", name, preview_suffix.c_str(), experiment_id);
    PltHistogramWithNoMacroCaching(name_with_experiment_id, sample);
  }
}

#define PLT_HISTOGRAM(name, sample) \
    UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, kPLTMin(), kPLTMax(), kPLTCount);

#define PLT_HISTOGRAM_WITH_GWS_VARIANT(                                        \
    name, sample, came_from_websearch, websearch_chrome_joint_experiment_id,   \
    is_preview) {                                                              \
  PLT_HISTOGRAM(name, sample);                                                 \
  if (came_from_websearch) {                                                   \
    PLT_HISTOGRAM(base::StringPrintf("%s_FromGWS", name), sample)              \
    if (websearch_chrome_joint_experiment_id != kNoExperiment) {               \
      std::string name_with_experiment_id = base::StringPrintf(                \
          "%s_FromGWS_Experiment%d",                                           \
          name, websearch_chrome_joint_experiment_id);                         \
      PltHistogramWithNoMacroCaching(name_with_experiment_id, sample);         \
    }                                                                          \
  }                                                                            \
  PltHistogramWithGwsPreview(name, sample, is_preview,                         \
                             websearch_chrome_joint_experiment_id);            \
}

// In addition to PLT_HISTOGRAM, add the *_DataReductionProxy variant
// conditionally. This macro runs only in one thread.
#define PLT_HISTOGRAM_DRP(                                              \
    name, sample, data_reduction_proxy_was_used, scheme_type)           \
  do {                                                                  \
    static base::HistogramBase* counter(NULL);                          \
    static base::HistogramBase* drp_counter(NULL);                      \
    static base::HistogramBase* https_drp_counter(NULL);                \
    if (!counter) {                                                     \
      DCHECK(drp_counter == NULL);                                      \
      DCHECK(https_drp_counter == NULL);                                \
      counter = base::Histogram::FactoryTimeGet(                        \
          name, kPLTMin(), kPLTMax(), kPLTCount,                        \
          base::Histogram::kUmaTargetedHistogramFlag);                  \
    }                                                                   \
    counter->AddTime(sample);                                           \
    if (!data_reduction_proxy_was_used) break;                          \
    if ((scheme_type & URLPattern::SCHEME_HTTPS) == 0) {                \
      if (!https_drp_counter) {                                         \
        https_drp_counter = base::Histogram::FactoryTimeGet(            \
            std::string(name) + "_DataReductionProxy",                  \
            kPLTMin(), kPLTMax(), kPLTCount,                            \
            base::Histogram::kUmaTargetedHistogramFlag);                \
      }                                                                 \
      https_drp_counter->AddTime(sample);                               \
    } else {                                                            \
      if (!drp_counter) {                                               \
        drp_counter = base::Histogram::FactoryTimeGet(                  \
            std::string(name) + "_HTTPS_DataReductionProxy",            \
            kPLTMin(), kPLTMax(), kPLTCount,                            \
            base::Histogram::kUmaTargetedHistogramFlag);                \
      }                                                                 \
      drp_counter->AddTime(sample);                                     \
    }                                                                   \
  } while (0)

// Returns the scheme type of the given URL if its type is one for which we
// dump page load histograms. Otherwise returns NULL.
URLPattern::SchemeMasks GetSupportedSchemeType(const GURL& url) {
  if (url.SchemeIs("http"))
    return URLPattern::SCHEME_HTTP;
  else if (url.SchemeIs("https"))
    return URLPattern::SCHEME_HTTPS;
  return static_cast<URLPattern::SchemeMasks>(0);
}

// Helper function to check for string in 'via' header. Returns true if
// |via_value| is one of the values listed in the Via header.
bool ViaHeaderContains(WebFrame* frame, const std::string& via_value) {
  const char kViaHeaderName[] = "Via";
  std::vector<std::string> values;
  // Multiple via headers have already been coalesced and hence each value
  // separated by a comma corresponds to a proxy. The value added by a proxy is
  // not expected to contain any commas.
  // Example., Via: 1.0 Compression proxy, 1.1 Google Instant Proxy Preview
  values = base::SplitString(
      frame->dataSource()->response().httpHeaderField(kViaHeaderName).utf8(),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  return std::find(values.begin(), values.end(), via_value) != values.end();
}

// Returns true if the provided URL is a referrer string that came from
// a Google Web Search results page. This is a little non-deterministic
// because desktop and mobile websearch differ and sometimes just provide
// http://www.google.com/ as the referrer. In the case of /url we can be sure
// that it came from websearch but we will be generous and allow for cases
// where a non-Google URL was provided a bare Google URL as a referrer.
// The domain validation matches the code used by the prerenderer for similar
// purposes.
// TODO(pmeenan): Remove the fuzzy logic when the referrer is reliable
bool IsFromGoogleSearchResult(const GURL& url, const GURL& referrer) {
  if (!base::StartsWith(referrer.host(), "www.google.",
                        base::CompareCase::SENSITIVE))
    return false;
  if (base::StartsWith(referrer.path(), "/url",
                       base::CompareCase::SENSITIVE))
    return true;
  bool is_possible_search_referrer =
      referrer.path().empty() || referrer.path() == "/" ||
      base::StartsWith(referrer.path(), "/search",
                       base::CompareCase::SENSITIVE) ||
      base::StartsWith(referrer.path(), "/webhp",
                       base::CompareCase::SENSITIVE);
  if (is_possible_search_referrer &&
      !base::StartsWith(url.host(), "www.google",
                        base::CompareCase::SENSITIVE))
    return true;
  return false;
}

// Extracts a Google Web Search and Chrome joint experiment ID from a referrer
// that came from a Google Web Search results page. An experiment ID is embedded
// in a query string as a "gcjeid=" parameter value.
int GetQueryStringBasedExperiment(const GURL& referrer) {
  std::string value;
  if (!net::GetValueForKeyInQuery(referrer, "gcjeid", &value))
    return kNoExperiment;

  int experiment_id;
  if (!base::StringToInt(value, &experiment_id))
    return kNoExperiment;
  if (0 < experiment_id && experiment_id <= kMaxExperimentID)
    return experiment_id;
  return kNoExperiment;
}

void DumpHistograms(const WebPerformance& performance,
                    DocumentState* document_state,
                    bool data_reduction_proxy_was_used,
                    bool lofi_active_for_page,  // LoFi was used, unless part of
                                                // the control group.
                    bool came_from_websearch,
                    int websearch_chrome_joint_experiment_id,
                    bool is_preview,
                    URLPattern::SchemeMasks scheme_type) {
  // This function records new histograms based on the Navigation Timing
  // records. As such, the histograms should not depend on the deprecated timing
  // information collected in DocumentState. However, here for some reason we
  // check if document_state->request_time() is null. TODO(ppi): find out why
  // and remove DocumentState from the parameter list.
  Time request = document_state->request_time();

  Time navigation_start = Time::FromDoubleT(performance.navigationStart());
  Time redirect_start = Time::FromDoubleT(performance.redirectStart());
  Time redirect_end = Time::FromDoubleT(performance.redirectEnd());
  Time fetch_start = Time::FromDoubleT(performance.fetchStart());
  Time domain_lookup_start = Time::FromDoubleT(performance.domainLookupStart());
  Time domain_lookup_end = Time::FromDoubleT(performance.domainLookupEnd());
  Time connect_start = Time::FromDoubleT(performance.connectStart());
  Time connect_end = Time::FromDoubleT(performance.connectEnd());
  Time request_start = Time::FromDoubleT(performance.requestStart());
  Time response_start = Time::FromDoubleT(performance.responseStart());
  Time response_end = Time::FromDoubleT(performance.responseEnd());
  Time dom_loading = Time::FromDoubleT(performance.domLoading());
  Time dom_interactive = Time::FromDoubleT(performance.domInteractive());
  Time dom_content_loaded_start =
      Time::FromDoubleT(performance.domContentLoadedEventStart());
  Time dom_content_loaded_end =
      Time::FromDoubleT(performance.domContentLoadedEventEnd());
  Time load_event_start = Time::FromDoubleT(performance.loadEventStart());
  Time load_event_end = Time::FromDoubleT(performance.loadEventEnd());
  Time begin = (request.is_null() ? navigation_start : request_start);
  Time first_paint = document_state->first_paint_time();

  DCHECK(!navigation_start.is_null());

  // It is possible for a document to have navigation_start time, but no
  // request_start. An example is doing a window.open, which synchronously
  // loads "about:blank", then using document.write add a meta http-equiv
  // refresh tag, which causes a navigation. In such case, we will arrive at
  // this function with no request/response timing data and identical load
  // start/end values. Avoid logging this case, as it doesn't add any
  // meaningful information to the histogram.
  if (request_start.is_null())
    return;

  // TODO(dominich): Investigate conditions under which |load_event_start| and
  // |load_event_end| may be NULL as in the non-PT_ case below. Examples in
  // http://crbug.com/112006.
  // DCHECK(!load_event_start.is_null());
  // DCHECK(!load_event_end.is_null());

  if (document_state->web_timing_histograms_recorded())
    return;
  document_state->set_web_timing_histograms_recorded(true);

  if (!redirect_start.is_null() && !redirect_end.is_null()) {
    PLT_HISTOGRAM_DRP("PLT.NT_Redirect",
                      redirect_end - redirect_start,
                      data_reduction_proxy_was_used,
                      scheme_type);
    PLT_HISTOGRAM_DRP(
        "PLT.NT_DelayBeforeFetchRedirect",
        (fetch_start - navigation_start) - (redirect_end - redirect_start),
        data_reduction_proxy_was_used,
        scheme_type);
  } else {
    PLT_HISTOGRAM_DRP("PLT.NT_DelayBeforeFetch",
                      fetch_start - navigation_start,
                      data_reduction_proxy_was_used,
                      scheme_type);
  }
  PLT_HISTOGRAM_DRP("PLT.NT_DelayBeforeDomainLookup",
                    domain_lookup_start - fetch_start,
                    data_reduction_proxy_was_used,
                    scheme_type);
  PLT_HISTOGRAM_DRP("PLT.NT_DomainLookup",
                    domain_lookup_end - domain_lookup_start,
                    data_reduction_proxy_was_used,
                    scheme_type);
  PLT_HISTOGRAM_DRP("PLT.NT_DelayBeforeConnect",
                    connect_start - domain_lookup_end,
                    data_reduction_proxy_was_used,
                    scheme_type);
  PLT_HISTOGRAM_DRP("PLT.NT_Connect",
                    connect_end - connect_start,
                    data_reduction_proxy_was_used,
                    scheme_type);
  PLT_HISTOGRAM_DRP("PLT.NT_DelayBeforeRequest",
                    request_start - connect_end,
                    data_reduction_proxy_was_used,
                    scheme_type);
  PLT_HISTOGRAM_DRP("PLT.NT_Request",
                    response_start - request_start,
                    data_reduction_proxy_was_used,
                    scheme_type);
  PLT_HISTOGRAM_DRP("PLT.NT_Response",
                    response_end - response_start,
                    data_reduction_proxy_was_used,
                    scheme_type);

  if (!dom_loading.is_null()) {
    PLT_HISTOGRAM_DRP("PLT.NT_DelayBeforeDomLoading",
                      dom_loading - response_start,
                      data_reduction_proxy_was_used,
                      scheme_type);
  }
  if (!dom_interactive.is_null() && !dom_loading.is_null()) {
    PLT_HISTOGRAM_DRP("PLT.NT_DomLoading",
                      dom_interactive - dom_loading,
                      data_reduction_proxy_was_used,
                      scheme_type);
  }
  if (!dom_content_loaded_start.is_null() && !dom_interactive.is_null()) {
    PLT_HISTOGRAM_DRP("PLT.NT_DomInteractive",
                      dom_content_loaded_start - dom_interactive,
                      data_reduction_proxy_was_used,
                      scheme_type);
  }
  if (!dom_content_loaded_start.is_null() &&
      !dom_content_loaded_end.is_null() ) {
    PLT_HISTOGRAM_DRP("PLT.NT_DomContentLoaded",
                      dom_content_loaded_end - dom_content_loaded_start,
                      data_reduction_proxy_was_used,
                      scheme_type);
  }
  if (!dom_content_loaded_end.is_null() && !load_event_start.is_null()) {
    PLT_HISTOGRAM_DRP("PLT.NT_DelayBeforeLoadEvent",
                      load_event_start - dom_content_loaded_end,
                      data_reduction_proxy_was_used,
                      scheme_type);
  }

  // TODO(simonjam): There is no way to distinguish between abandonment and
  // intentional Javascript navigation before the load event fires.
  // TODO(dominich): Load type breakdown
  if (!load_event_start.is_null()) {
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_BeginToFinishDoc",
                                   load_event_start - begin,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   is_preview);
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_CommitToFinishDoc",
                                   load_event_start - response_start,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   is_preview);
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_RequestToFinishDoc",
                                   load_event_start - navigation_start,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   is_preview);
    if (data_reduction_proxy_was_used) {
      if ((scheme_type & URLPattern::SCHEME_HTTPS) == 0) {
        PLT_HISTOGRAM("PLT.PT_BeginToFinishDoc_DataReductionProxy",
                      load_event_start - begin);
        PLT_HISTOGRAM("PLT.PT_CommitToFinishDoc_DataReductionProxy",
                      load_event_start - response_start);
        PLT_HISTOGRAM("PLT.PT_RequestToFinishDoc_DataReductionProxy",
                      load_event_start - navigation_start);
      } else {
        PLT_HISTOGRAM("PLT.PT_BeginToFinishDoc_HTTPS_DataReductionProxy",
                      load_event_start - begin);
        PLT_HISTOGRAM("PLT.PT_CommitToFinishDoc_HTTPS_DataReductionProxy",
                      load_event_start - response_start);
        PLT_HISTOGRAM("PLT.PT_RequestToFinishDoc_HTTPS_DataReductionProxy",
                      load_event_start - navigation_start);
      }
    }
  }
  if (!load_event_end.is_null()) {
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_BeginToFinish",
                                   load_event_end - begin,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   is_preview);
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_CommitToFinish",
                                   load_event_end - response_start,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   is_preview);
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_RequestToFinish",
                                   load_event_end - navigation_start,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   is_preview);
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_StartToFinish",
                                   load_event_end - request_start,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   is_preview);
    if (data_reduction_proxy_was_used) {
      // TODO(megjablon): Move these repetitive cases into an anonymous
      // function.
      bool in_lofi_enabled_group =
          data_reduction_proxy::params::IsIncludedInLoFiEnabledFieldTrial();
      bool in_lofi_control_group =
          data_reduction_proxy::params::IsIncludedInLoFiControlFieldTrial();
      if ((scheme_type & URLPattern::SCHEME_HTTPS) == 0) {
        PLT_HISTOGRAM("PLT.PT_BeginToFinish_DataReductionProxy",
                      load_event_end - begin);
        PLT_HISTOGRAM("PLT.PT_CommitToFinish_DataReductionProxy",
                      load_event_end - response_start);
        PLT_HISTOGRAM("PLT.PT_RequestToFinish_DataReductionProxy",
                      load_event_end - navigation_start);
        PLT_HISTOGRAM("PLT.PT_StartToFinish_DataReductionProxy",
                      load_event_end - request_start);
        if (lofi_active_for_page && in_lofi_enabled_group) {
          PLT_HISTOGRAM("PLT.PT_BeginToFinish_DataReductionProxy_AutoLoFiOn",
                        load_event_end - begin);
          PLT_HISTOGRAM("PLT.PT_CommitToFinish_DataReductionProxy_AutoLoFiOn",
                        load_event_end - response_start);
          PLT_HISTOGRAM("PLT.PT_RequestToFinish_DataReductionProxy_AutoLoFiOn",
                        load_event_end - navigation_start);
          PLT_HISTOGRAM("PLT.PT_StartToFinish_DataReductionProxy_AutoLoFiOn",
                        load_event_end - request_start);
          if (!first_paint.is_null()) {
            PLT_HISTOGRAM("PLT.BeginToFirstPaint_DataReductionProxy_AutoLoFiOn",
                          first_paint - begin);
          }
        } else if (lofi_active_for_page && in_lofi_control_group) {
          PLT_HISTOGRAM("PLT.PT_BeginToFinish_DataReductionProxy_AutoLoFiOff",
                        load_event_end - begin);
          PLT_HISTOGRAM("PLT.PT_CommitToFinish_DataReductionProxy_AutoLoFiOff",
                        load_event_end - response_start);
          PLT_HISTOGRAM("PLT.PT_RequestToFinish_DataReductionProxy_AutoLoFiOff",
                        load_event_end - navigation_start);
          PLT_HISTOGRAM("PLT.PT_StartToFinish_DataReductionProxy_AutoLoFiOff",
                        load_event_end - request_start);
          if (!first_paint.is_null()) {
            PLT_HISTOGRAM(
                "PLT.BeginToFirstPaint_DataReductionProxy_AutoLoFiOff",
                first_paint - begin);
          }
        }
      } else {
        PLT_HISTOGRAM("PLT.PT_BeginToFinish_HTTPS_DataReductionProxy",
                      load_event_end - begin);
        PLT_HISTOGRAM("PLT.PT_CommitToFinish_HTTPS_DataReductionProxy",
                      load_event_end - response_start);
        PLT_HISTOGRAM("PLT.PT_RequestToFinish_HTTPS_DataReductionProxy",
                      load_event_end - navigation_start);
        PLT_HISTOGRAM("PLT.PT_StartToFinish_HTTPS_DataReductionProxy",
                      load_event_end - request_start);
        if (lofi_active_for_page && in_lofi_enabled_group) {
          PLT_HISTOGRAM(
              "PLT.PT_BeginToFinish_HTTPS_DataReductionProxy_AutoLoFiOn",
              load_event_end - begin);
          PLT_HISTOGRAM(
              "PLT.PT_CommitToFinish_HTTPS_DataReductionProxy_AutoLoFiOn",
              load_event_end - response_start);
          PLT_HISTOGRAM(
              "PLT.PT_RequestToFinish_HTTPS_DataReductionProxy_AutoLoFiOn",
              load_event_end - navigation_start);
          PLT_HISTOGRAM(
              "PLT.PT_StartToFinish_HTTPS_DataReductionProxy_AutoLoFiOn",
              load_event_end - request_start);
          if (!first_paint.is_null()) {
            PLT_HISTOGRAM(
                "PLT.BeginToFirstPaint_HTTPS_DataReductionProxy_AutoLoFiOn",
                first_paint - begin);
          }
        } else if (lofi_active_for_page && in_lofi_control_group) {
          PLT_HISTOGRAM(
              "PLT.PT_BeginToFinish_HTTPS_DataReductionProxy_AutoLoFiOff",
              load_event_end - begin);
          PLT_HISTOGRAM(
              "PLT.PT_CommitToFinish_HTTPS_DataReductionProxy_AutoLoFiOff",
              load_event_end - response_start);
          PLT_HISTOGRAM(
              "PLT.PT_RequestToFinish_HTTPS_DataReductionProxy_AutoLoFiOff",
              load_event_end - navigation_start);
          PLT_HISTOGRAM(
              "PLT.PT_StartToFinish_HTTPS_DataReductionProxy_AutoLoFiOff",
              load_event_end - request_start);
          if (!first_paint.is_null()) {
            PLT_HISTOGRAM(
                "PLT.BeginToFirstPaint_HTTPS_DataReductionProxy_AutoLoFiOff",
                first_paint - begin);
          }
        }
      }
    }
  }
  if (!load_event_start.is_null() && !load_event_end.is_null()) {
    PLT_HISTOGRAM("PLT.PT_FinishDocToFinish",
                  load_event_end - load_event_start);
    PLT_HISTOGRAM_DRP("PLT.NT_LoadEvent",
                      load_event_end - load_event_start,
                      data_reduction_proxy_was_used,
                      scheme_type);

    if (data_reduction_proxy_was_used) {
      if ((scheme_type & URLPattern::SCHEME_HTTPS) == 0) {
        PLT_HISTOGRAM("PLT.PT_FinishDocToFinish_DataReductionProxy",
                      load_event_end - load_event_start);
      } else {
        PLT_HISTOGRAM("PLT.PT_FinishDocToFinish_HTTPS_DataReductionProxy",
                      load_event_end - load_event_start);
      }
    }
  }
  if (!dom_content_loaded_start.is_null()) {
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_RequestToDomContentLoaded",
                                   dom_content_loaded_start - navigation_start,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   is_preview);
    if (data_reduction_proxy_was_used) {
      bool in_lofi_enabled_group =
          data_reduction_proxy::params::IsIncludedInLoFiEnabledFieldTrial();
      bool in_lofi_control_group =
          data_reduction_proxy::params::IsIncludedInLoFiControlFieldTrial();
      if ((scheme_type & URLPattern::SCHEME_HTTPS) == 0) {
        PLT_HISTOGRAM("PLT.PT_RequestToDomContentLoaded_DataReductionProxy",
                      dom_content_loaded_start - navigation_start);
        if (lofi_active_for_page && in_lofi_enabled_group) {
          PLT_HISTOGRAM(
              "PLT.PT_RequestToDomContentLoaded_DataReductionProxy_AutoLoFiOn",
              dom_content_loaded_start - navigation_start);
        } else if (lofi_active_for_page && in_lofi_control_group) {
          PLT_HISTOGRAM(
              "PLT.PT_RequestToDomContentLoaded_DataReductionProxy_AutoLoFiOff",
              dom_content_loaded_start - navigation_start);
        }
      } else {
        PLT_HISTOGRAM(
            "PLT.PT_RequestToDomContentLoaded_HTTPS_DataReductionProxy",
            dom_content_loaded_start - navigation_start);
        if (lofi_active_for_page && in_lofi_enabled_group) {
          PLT_HISTOGRAM(
              "PLT.PT_RequestToDomContentLoaded_HTTPS_DataReductionProxy_"
              "AutoLoFiOn",
              dom_content_loaded_start - navigation_start);
        } else if (lofi_active_for_page && in_lofi_control_group) {
          PLT_HISTOGRAM(
              "PLT.PT_RequestToDomContentLoaded_HTTPS_DataReductionProxy_"
              "AutoLoFiOff",
              dom_content_loaded_start - navigation_start);
        }
      }
    }
  }
  PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_BeginToCommit",
                                 response_start - begin,
                                 came_from_websearch,
                                 websearch_chrome_joint_experiment_id,
                                 is_preview);
  PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_RequestToStart",
                                 request_start - navigation_start,
                                 came_from_websearch,
                                 websearch_chrome_joint_experiment_id,
                                 is_preview);
  PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_StartToCommit",
                                 response_start - request_start,
                                 came_from_websearch,
                                 websearch_chrome_joint_experiment_id,
                                 is_preview);
  PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_RequestToCommit",
                                 response_start - navigation_start,
                                 came_from_websearch,
                                 websearch_chrome_joint_experiment_id,
                                 is_preview);
  if (data_reduction_proxy_was_used) {
    if ((scheme_type & URLPattern::SCHEME_HTTPS) == 0) {
      PLT_HISTOGRAM("PLT.PT_BeginToCommit_DataReductionProxy",
                    response_start - begin);
      PLT_HISTOGRAM("PLT.PT_RequestToStart_DataReductionProxy",
                    request_start - navigation_start);
      PLT_HISTOGRAM("PLT.PT_StartToCommit_DataReductionProxy",
                    response_start - request_start);
      PLT_HISTOGRAM("PLT.PT_RequestToCommit_DataReductionProxy",
                    response_start - navigation_start);
    } else {
      PLT_HISTOGRAM("PLT.PT_BeginToCommit_HTTPS_DataReductionProxy",
                    response_start - begin);
      PLT_HISTOGRAM("PLT.PT_RequestToStart_HTTPS_DataReductionProxy",
                    request_start - navigation_start);
      PLT_HISTOGRAM("PLT.PT_StartToCommit_HTTPS_DataReductionProxy",
                    response_start - request_start);
      PLT_HISTOGRAM("PLT.PT_RequestToCommit_HTTPS_DataReductionProxy",
                    response_start - navigation_start);
    }
  }
}

bool WasWebRequestUsedBySomeExtensions() {
#if defined(ENABLE_EXTENSIONS)
  return ChromeExtensionsRendererClient::GetInstance()->extension_dispatcher()
      ->WasWebRequestUsedBySomeExtensions();
#else
  return false;
#endif
}

// These histograms are based on the timing information collected in
// DocumentState. They should be transitioned to equivalents based on the
// Navigation Timing records (see DumpPerformanceTiming()) or dropped if not
// needed. Please do not add new metrics based on DocumentState.
void DumpDeprecatedHistograms(const WebPerformance& performance,
                              DocumentState* document_state,
                              bool data_reduction_proxy_was_used,
                              bool came_from_websearch,
                              int websearch_chrome_joint_experiment_id,
                              bool is_preview,
                              URLPattern::SchemeMasks scheme_type) {
  // If we've already dumped, do nothing.
  // This simple bool works because we only dump for the main frame.
  if (document_state->load_histograms_recorded())
    return;

  // Abort if any of these is missing.
  Time start = document_state->start_load_time();
  Time commit = document_state->commit_load_time();
  Time navigation_start =
      Time::FromDoubleT(performance.navigationStart());
  if (start.is_null() || commit.is_null() || navigation_start.is_null())
    return;

  // We properly handle null values for the next 3 variables.
  Time request = document_state->request_time();
  Time first_paint = document_state->first_paint_time();
  Time first_paint_after_load = document_state->first_paint_after_load_time();
  Time finish_doc = document_state->finish_document_load_time();
  Time finish_all_loads = document_state->finish_load_time();

  // Handle case where user hits "stop" or "back" before loading completely.
  // Note that this makes abandoned page loads be recorded as if they were
  // completed, polluting the metrics with artifically short completion times.
  // We are not fixing this as these metrics are being dropped as deprecated.
  if (finish_doc.is_null()) {
    finish_doc = Time::Now();
    document_state->set_finish_document_load_time(finish_doc);
  }
  if (finish_all_loads.is_null()) {
    finish_all_loads = Time::Now();
    document_state->set_finish_load_time(finish_all_loads);
  }

  document_state->set_load_histograms_recorded(true);

  // Note: Client side redirects will have no request time.
  Time begin = request.is_null() ? start : request;
  TimeDelta begin_to_finish_doc = finish_doc - begin;
  TimeDelta begin_to_finish_all_loads = finish_all_loads - begin;
  TimeDelta start_to_finish_all_loads = finish_all_loads - start;
  TimeDelta start_to_commit = commit - start;

  DocumentState::LoadType load_type = document_state->load_type();

  // The above code sanitized all values of times, in preparation for creating
  // actual histograms.  The remainder of this code could be run at destructor
  // time for the document_state, since all data is intact.

  // Aggregate PLT data across all link types.
  UMA_HISTOGRAM_ENUMERATION("PLT.LoadType", load_type,
      DocumentState::kLoadTypeMax);
  PLT_HISTOGRAM("PLT.StartToCommit", start_to_commit);
  PLT_HISTOGRAM("PLT.CommitToFinishDoc", finish_doc - commit);
  PLT_HISTOGRAM("PLT.FinishDocToFinish", finish_all_loads - finish_doc);
  PLT_HISTOGRAM("PLT.BeginToCommit", commit - begin);
  PLT_HISTOGRAM("PLT.StartToFinish", start_to_finish_all_loads);
  if (!request.is_null()) {
    PLT_HISTOGRAM("PLT.RequestToStart", start - request);
    PLT_HISTOGRAM("PLT.RequestToFinish", finish_all_loads - request);
  }
  PLT_HISTOGRAM("PLT.CommitToFinish", finish_all_loads - commit);

  scoped_ptr<TimeDelta> begin_to_first_paint;
  scoped_ptr<TimeDelta> commit_to_first_paint;
  if (!first_paint.is_null()) {
    // 'first_paint' can be before 'begin' for an unknown reason.
    // See bug http://crbug.com/125273 for details.
    if (begin <= first_paint) {
      begin_to_first_paint.reset(new TimeDelta(first_paint - begin));
      PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.BeginToFirstPaint",
                                     *begin_to_first_paint,
                                     came_from_websearch,
                                     websearch_chrome_joint_experiment_id,
                                     is_preview);
    } else {
      // Track the frequency and magnitude of cases where first_paint precedes
      // begin. The current hypothesis is that this is due to using the
      // non-monotonic timer. If that's the case, we expect first_paint
      // preceding begin to be rare, and the delta between values to be close to
      // zero. This is a temporary addition that we will remove once we better
      // understand the frequency and magnitude of first_paint preceding begin.
      PLT_HISTOGRAM("PLT.BeginToFirstPaint_Negative", begin - first_paint);
    }

    // Conditional was previously a DCHECK. Changed due to multiple bot
    // failures, listed in crbug.com/383963
    if (commit <= first_paint) {
      commit_to_first_paint.reset(new TimeDelta(first_paint - commit));
      PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.CommitToFirstPaint",
                                     *commit_to_first_paint,
                                     came_from_websearch,
                                     websearch_chrome_joint_experiment_id,
                                     is_preview);
    }
  }
  if (!first_paint_after_load.is_null()) {
    // 'first_paint_after_load' can be before 'begin' for an unknown reason.
    // See bug http://crbug.com/125273 for details.
    if (begin <= first_paint_after_load) {
      PLT_HISTOGRAM("PLT.BeginToFirstPaintAfterLoad",
          first_paint_after_load - begin);
    }
    // Both following conditionals were previously DCHECKs. Changed due to
    // multiple bot failures, listed in crbug.com/383963
    if (commit <= first_paint_after_load) {
      PLT_HISTOGRAM("PLT.CommitToFirstPaintAfterLoad",
          first_paint_after_load - commit);
    }
    if (finish_all_loads <= first_paint_after_load) {
      PLT_HISTOGRAM("PLT.FinishToFirstPaintAfterLoad",
          first_paint_after_load - finish_all_loads);
    }
  }
  PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.BeginToFinishDoc", begin_to_finish_doc,
                                 came_from_websearch,
                                 websearch_chrome_joint_experiment_id,
                                 is_preview);
  PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.BeginToFinish", begin_to_finish_all_loads,
                                 came_from_websearch,
                                 websearch_chrome_joint_experiment_id,
                                 is_preview);

  // Load type related histograms.
  switch (load_type) {
    case DocumentState::UNDEFINED_LOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_UndefLoad", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_UndefLoad", begin_to_finish_all_loads);
      break;
    case DocumentState::RELOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_Reload", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_Reload", begin_to_finish_all_loads);
      break;
    case DocumentState::HISTORY_LOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_HistoryLoad", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_HistoryLoad", begin_to_finish_all_loads);
      break;
    case DocumentState::NORMAL_LOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_NormalLoad", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_NormalLoad", begin_to_finish_all_loads);
      break;
    case DocumentState::LINK_LOAD_NORMAL:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadNormal",
          begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadNormal",
                    begin_to_finish_all_loads);
      break;
    case DocumentState::LINK_LOAD_RELOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadReload",
           begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadReload",
                    begin_to_finish_all_loads);
      break;
    case DocumentState::LINK_LOAD_CACHE_STALE_OK:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadStaleOk",
           begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadStaleOk",
                    begin_to_finish_all_loads);
      break;
    case DocumentState::LINK_LOAD_CACHE_ONLY:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadCacheOnly",
           begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadCacheOnly",
                    begin_to_finish_all_loads);
      break;
    default:
      break;
  }

  if (data_reduction_proxy_was_used) {
    PLT_HISTOGRAM("PLT.BeginToFinishDoc_SpdyProxy", begin_to_finish_doc);
    PLT_HISTOGRAM("PLT.BeginToFinish_SpdyProxy", begin_to_finish_all_loads);
  }

  if (document_state->was_prefetcher()) {
    PLT_HISTOGRAM("PLT.BeginToFinishDoc_ContentPrefetcher",
                  begin_to_finish_doc);
    PLT_HISTOGRAM("PLT.BeginToFinish_ContentPrefetcher",
                  begin_to_finish_all_loads);
  }
  if (document_state->was_referred_by_prefetcher()) {
    PLT_HISTOGRAM("PLT.BeginToFinishDoc_ContentPrefetcherReferrer",
                  begin_to_finish_doc);
    PLT_HISTOGRAM("PLT.BeginToFinish_ContentPrefetcherReferrer",
                  begin_to_finish_all_loads);
  }

  const bool use_webrequest_histogram = WasWebRequestUsedBySomeExtensions();
  if (use_webrequest_histogram) {
    switch (load_type) {
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_NormalLoad_ExtensionWebRequest",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadNormal_ExtensionWebRequest",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadReload_ExtensionWebRequest",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadStaleOk_ExtensionWebRequest",
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }
}

}  // namespace

PageLoadHistograms::PageLoadHistograms(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      dumped_first_layout_histograms_(false),
      weak_factory_(this) {
}

PageLoadHistograms::~PageLoadHistograms() {
}

bool PageLoadHistograms::ShouldDump(WebFrame* frame) {
  // We only dump histograms for main frames.
  // In the future, it may be interesting to tag subframes and dump them too.
  if (!frame || frame->parent())
    return false;

  // If the main frame lives in a different process, don't do anything.
  // Histogram data will be recorded by the real main frame.
  if (frame->isWebRemoteFrame())
    return false;

  // Only dump for supported schemes.
  URLPattern::SchemeMasks scheme_type =
      GetSupportedSchemeType(frame->document().url());
  if (scheme_type == 0)
    return false;

  // Don't dump stats for the NTP, as PageLoadHistograms should only be recorded
  // for pages visited due to an explicit user navigation.
  if (SearchBouncer::GetInstance()->IsNewTabPage(frame->document().url())) {
    return false;
  }

  // Ignore multipart requests.
  if (frame->dataSource()->response().isMultipartPayload())
    return false;

  return true;
}

void PageLoadHistograms::Dump(WebFrame* frame) {
  if (!ShouldDump(frame))
    return;

  URLPattern::SchemeMasks scheme_type =
      GetSupportedSchemeType(frame->document().url());

  DocumentState* document_state =
      DocumentState::FromDataSource(frame->dataSource());

  bool data_reduction_proxy_was_used = false;
  if (!document_state->proxy_server().IsEmpty()) {
    bool handled =
        Send(new DataReductionProxyViewHostMsg_IsDataReductionProxy(
            document_state->proxy_server(), &data_reduction_proxy_was_used));
    // If the IPC call is not handled, then |data_reduction_proxy_was_used|
    // should remain |false|.
    DCHECK(handled || !data_reduction_proxy_was_used);
  }

  bool came_from_websearch =
      IsFromGoogleSearchResult(frame->document().url(),
                               GURL(frame->document().referrer()));
  int websearch_chrome_joint_experiment_id = kNoExperiment;
  bool is_preview = false;
  if (came_from_websearch) {
    websearch_chrome_joint_experiment_id =
        GetQueryStringBasedExperiment(GURL(frame->document().referrer()));
    is_preview = ViaHeaderContains(frame, "1.1 Google Instant Proxy Preview");
  }

  MaybeDumpFirstLayoutHistograms();

  // Metrics based on the timing information recorded for the Navigation Timing
  // API - http://www.w3.org/TR/navigation-timing/.
  DumpHistograms(
      frame->performance(), document_state, data_reduction_proxy_was_used,
      false /* TODO: render_frame->IsUsingLoFi() */, came_from_websearch,
      websearch_chrome_joint_experiment_id, is_preview, scheme_type);

  // Old metrics based on the timing information stored in DocumentState. These
  // are deprecated and should go away.
  DumpDeprecatedHistograms(frame->performance(), document_state,
                           data_reduction_proxy_was_used,
                           came_from_websearch,
                           websearch_chrome_joint_experiment_id,
                           is_preview,
                           scheme_type);

  // Log the PLT to the info log.
  LogPageLoadTime(document_state, frame->dataSource());

  // Since there are currently no guarantees that renderer histograms will be
  // sent to the browser, we initiate a PostTask here to be sure that we send
  // the histograms we generated.  Without this call, pages that don't have an
  // on-close-handler might generate data that is lost when the renderer is
  // shutdown abruptly (perchance because the user closed the tab).
  // TODO(jar) BUG=33233: This needs to be moved to a PostDelayedTask, and it
  // should post when the onload is complete, so that it doesn't interfere with
  // the next load.
  content::RenderThread::Get()->UpdateHistograms(
      content::kHistogramSynchronizerReservedSequenceNumber);
}

void PageLoadHistograms::MaybeDumpFirstLayoutHistograms() {
  if (dumped_first_layout_histograms_)
    return;

  const WebPerformance& performance =
    render_view()->GetWebView()->mainFrame()->performance();
  Time first_layout = Time::FromDoubleT(performance.firstLayout());
  if (first_layout.is_null())
    return;

  Time navigation_start = Time::FromDoubleT(performance.navigationStart());
  if (!navigation_start.is_null())
    PLT_HISTOGRAM("PLT.PT.NavigationStartToFirstLayout",
                  first_layout - navigation_start);

  Time response_start = Time::FromDoubleT(performance.responseStart());
  if (!response_start.is_null())
    PLT_HISTOGRAM("PLT.PT.ResponseStartToFirstLayout",
                  first_layout - response_start);

  dumped_first_layout_histograms_ = true;
}

void PageLoadHistograms::FrameWillClose(WebFrame* frame) {
  Dump(frame);
}

void PageLoadHistograms::ClosePage() {
  // TODO(davemoore) This code should be removed once willClose() gets
  // called when a page is destroyed. page_load_histograms_.Dump() is safe
  // to call multiple times for the same frame, but it will simplify things.
  Dump(render_view()->GetWebView()->mainFrame());
}

void PageLoadHistograms::DidUpdateLayout() {
  DCHECK(content::RenderThread::Get());
  // Normally, PageLoadHistograms dumps all histograms in the FrameWillClose or
  // ClosePage callbacks, which happen as a page is being torn down. However,
  // renderers that are killed by fast shutdown (for example, renderers closed
  // due to the user closing a tab) don't get a chance to run these callbacks
  // (see crbug.com/382542 for details).
  //
  // Longer term, we need to migrate histogram recording to happen earlier in
  // the page load life cycle, so histograms aren't lost when tabs are
  // closed. As a first step, we use the RenderViewObserver::DidUpdateLayout
  // callback to log first layout histograms earlier in the page load life
  // cycle.

  if (dumped_first_layout_histograms_)
    return;

  WebFrame* frame = render_view()->GetWebView()->mainFrame();
  if (!ShouldDump(frame))
    return;

  // The canonical source for the 'first layout time' is the
  // blink::WebPerformance object, so we need to read the first layout timestamp
  // from that object, rather than taking our own timestamp in this
  // callback.
  //
  // This DidUpdateLayout callback gets invoked in the midst of the
  // layout process. The logic that records the first layout time in the
  // blink::WebPerformance object may run later in the layout process, after
  // DidUpdateLayout gets invoked. Thus, we schedule a callback to run
  // MaybeDumpFirstLayoutHistograms asynchronously, after the layout process is
  // complete.
  //
  // Note, too, that some layouts are performed with pending stylesheets, and
  // blink will not record firstLayout during those layouts, so firstLayout may
  // not be populated during the layout associated with the first
  // DidUpdateLayout callback.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&PageLoadHistograms::MaybeDumpFirstLayoutHistograms,
                 weak_factory_.GetWeakPtr()));
}

void PageLoadHistograms::LogPageLoadTime(const DocumentState* document_state,
                                         const WebDataSource* ds) const {
  // Because this function gets called on every page load,
  // take extra care to optimize it away if logging is turned off.
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;

  DCHECK(document_state);
  DCHECK(ds);
  GURL url(ds->request().url());
  Time start = document_state->start_load_time();
  Time finish = document_state->finish_load_time();
  // TODO(mbelshe): should we log more stats?
  VLOG(1) << "PLT: " << (finish - start).InMilliseconds() << "ms "
          << url.spec();
}
