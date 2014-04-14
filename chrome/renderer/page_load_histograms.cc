// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/page_load_histograms.h"

#include <string>

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
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/url_pattern.h"
#include "net/base/url_util.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebPerformance.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"

#if defined(SPDY_PROXY_AUTH_ORIGIN)
#include "net/http/http_response_headers.h"
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

// Various preview applicability states.
enum GwsPreviewState {
  PREVIEW_NONE,
  // Instant search clicks [not] applied, data reduction proxy used,
  // from web search.
  PREVIEW_NOT_USED,
  // Instant search clicks applied, data reduction proxy [not] used,
  // from web search
  PREVIEW,
  // Instant search clicks applied, data reduction proxy used,
  // [not] from web search
  PREVIEW_WAS_SHOWN,
};

// This records UMA corresponding to the PLT_HISTOGRAM macro without caching.
void PltHistogramWithGwsPreview(const char* name,
                                const TimeDelta& sample,
                                GwsPreviewState preview_state,
                                int preview_experiment_id) {
  std::string preview_suffix;
  switch (preview_state) {
    case PREVIEW_WAS_SHOWN:
      preview_suffix = "_WithPreview";
      break;
    case PREVIEW:
      preview_suffix = "_Preview";
      break;
    case PREVIEW_NOT_USED:
      preview_suffix = "_NoPreview";
      break;
    default:
      return;
  }
  PltHistogramWithNoMacroCaching(name + preview_suffix, sample);

  if (preview_experiment_id != kNoExperiment) {
    std::string name_with_experiment_id = base::StringPrintf(
          "%s%s_Experiment%d", name, preview_suffix.c_str(),
          preview_experiment_id);
    PltHistogramWithNoMacroCaching(name_with_experiment_id, sample);
  }
}

#define PLT_HISTOGRAM(name, sample) \
    UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, kPLTMin(), kPLTMax(), kPLTCount);

#define PLT_HISTOGRAM_WITH_GWS_VARIANT(                                        \
    name, sample, came_from_websearch, websearch_chrome_joint_experiment_id,   \
    preview_state, preview_experiment_id) {                                    \
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
  PltHistogramWithGwsPreview(name, sample, preview_state,                      \
                             preview_experiment_id);                           \
}

// In addition to PLT_HISTOGRAM, add the *_DataReductionProxy variant
// conditionally. This macro runs only in one thread.
#define PLT_HISTOGRAM_DRP(name, sample, data_reduction_proxy_was_used) \
  do {                                                                  \
    static base::HistogramBase* counter(NULL);                          \
    static base::HistogramBase* drp_counter(NULL);                      \
    if (!counter) {                                                     \
      DCHECK(drp_counter == NULL);                                      \
      counter = base::Histogram::FactoryTimeGet(                        \
          name, kPLTMin(), kPLTMax(), kPLTCount,                        \
          base::Histogram::kUmaTargetedHistogramFlag);                  \
    }                                                                   \
    counter->AddTime(sample);                                           \
    if (!data_reduction_proxy_was_used) break;                          \
    if (!drp_counter) {                                                 \
      drp_counter = base::Histogram::FactoryTimeGet(                    \
          std::string(name) + "_DataReductionProxy",                    \
          kPLTMin(), kPLTMax(), kPLTCount,                              \
          base::Histogram::kUmaTargetedHistogramFlag);                  \
    }                                                                   \
    drp_counter->AddTime(sample);                                       \
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
// |via_value| is one of the values listed in the Via header and the response
// was fetched via a proxy.
bool ViaHeaderContains(WebFrame* frame, const std::string& via_value) {
  const char kViaHeaderName[] = "Via";

  DocumentState* document_state =
      DocumentState::FromDataSource(frame->dataSource());
  if (!document_state->was_fetched_via_proxy())
    return false;

  std::vector<std::string> values;
  // Multiple via headers have already been coalesced and hence each value
  // separated by a comma corresponds to a proxy. The value added by a proxy is
  // not expected to contain any commas.
  // Example., Via: 1.0 Compression proxy, 1.1 Google Instant Proxy Preview
  base::SplitString(
      frame->dataSource()->response().httpHeaderField(kViaHeaderName).utf8(),
      ',', &values);
  return std::find(values.begin(), values.end(), via_value) != values.end();
}

// Returns true if the data reduction proxy was used. Note, this function will
// produce a false positive if a page is fetched using SPDY and using a proxy,
// and the data reduction proxy's via value is added to the Via header.
// TODO(bengr): Plumb the hostname of the proxy and check if it matches
// |SPDY_PROXY_AUTH_ORIGIN|.
bool DataReductionProxyWasUsed(WebFrame* frame) {
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  DocumentState* document_state =
      DocumentState::FromDataSource(frame->dataSource());
  if (!document_state->was_fetched_via_proxy())
    return false;

  std::string via_header =
      base::UTF16ToUTF8(frame->dataSource()->response().httpHeaderField("Via"));

  if (via_header.empty())
    return false;
  std::string headers = "HTTP/1.1 200 OK\nVia: " + via_header + "\n\n";
  // Produce raw headers, expected by the |HttpResponseHeaders| constructor.
  std::replace(headers.begin(), headers.end(), '\n', '\0');
  scoped_refptr<net::HttpResponseHeaders> response_headers(
      new net::HttpResponseHeaders(headers));
  return response_headers->IsDataReductionProxyResponse();
#else
  return false;
#endif
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
  if (!StartsWithASCII(referrer.host(), "www.google.", true))
    return false;
  if (StartsWithASCII(referrer.path(), "/url", true))
    return true;
  bool is_possible_search_referrer =
      referrer.path().empty() ||
      referrer.path() == "/" ||
      StartsWithASCII(referrer.path(), "/search", true) ||
      StartsWithASCII(referrer.path(), "/webhp", true);
  if (is_possible_search_referrer &&
      !StartsWithASCII(url.host(), "www.google", true))
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

// Returns preview state by looking at url and referer url.
void GetPreviewState(WebFrame* frame,
                     bool came_from_websearch,
                     bool data_reduction_proxy_was_used,
                     GwsPreviewState* preview_state,
                     int* preview_experiment_id) {
  // Conditions for GWS preview are,
  // 1. Data reduction proxy was used.
  // Determine the preview state (PREVIEW, PREVIEW_WAS_SHOWN, PREVIEW_NOT_USED)
  // by inspecting the Via header.
  if (data_reduction_proxy_was_used) {
    if (came_from_websearch) {
      *preview_state = ViaHeaderContains(
          frame,
          "1.1 Google Instant Proxy Preview") ? PREVIEW : PREVIEW_NOT_USED;
    } else if (ViaHeaderContains(frame, "1.1 Google Instant Proxy")) {
      *preview_state = PREVIEW_WAS_SHOWN;
      *preview_experiment_id = GetQueryStringBasedExperiment(
          GURL(frame->document().referrer()));
    }
  }
}

void DumpPerformanceTiming(const WebPerformance& performance,
                           DocumentState* document_state,
                           bool data_reduction_proxy_was_used,
                           bool came_from_websearch,
                           int websearch_chrome_joint_experiment_id,
                           GwsPreviewState preview_state,
                           int preview_experiment_id) {
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
    PLT_HISTOGRAM_DRP("PLT.NT_Redirect", redirect_end - redirect_start,
                      data_reduction_proxy_was_used);
    PLT_HISTOGRAM_DRP(
        "PLT.NT_DelayBeforeFetchRedirect",
        (fetch_start - navigation_start) - (redirect_end - redirect_start),
        data_reduction_proxy_was_used);
  } else {
    PLT_HISTOGRAM_DRP("PLT.NT_DelayBeforeFetch",
                      fetch_start - navigation_start,
                      data_reduction_proxy_was_used);
  }
  PLT_HISTOGRAM_DRP("PLT.NT_DelayBeforeDomainLookup",
                    domain_lookup_start - fetch_start,
                    data_reduction_proxy_was_used);
  PLT_HISTOGRAM_DRP("PLT.NT_DomainLookup",
                    domain_lookup_end - domain_lookup_start,
                    data_reduction_proxy_was_used);
  PLT_HISTOGRAM_DRP("PLT.NT_DelayBeforeConnect",
                    connect_start - domain_lookup_end,
                    data_reduction_proxy_was_used);
  PLT_HISTOGRAM_DRP("PLT.NT_Connect", connect_end - connect_start,
                    data_reduction_proxy_was_used);
  PLT_HISTOGRAM_DRP("PLT.NT_DelayBeforeRequest",
                    request_start - connect_end,
                    data_reduction_proxy_was_used);
  PLT_HISTOGRAM_DRP("PLT.NT_Request", response_start - request_start,
                    data_reduction_proxy_was_used);
  PLT_HISTOGRAM_DRP("PLT.NT_Response", response_end - response_start,
                    data_reduction_proxy_was_used);

  if (!dom_loading.is_null()) {
    PLT_HISTOGRAM_DRP("PLT.NT_DelayBeforeDomLoading",
                      dom_loading - response_start,
                      data_reduction_proxy_was_used);
  }
  if (!dom_interactive.is_null() && !dom_loading.is_null()) {
    PLT_HISTOGRAM_DRP("PLT.NT_DomLoading",
                      dom_interactive - dom_loading,
                      data_reduction_proxy_was_used);
  }
  if (!dom_content_loaded_start.is_null() && !dom_interactive.is_null()) {
    PLT_HISTOGRAM_DRP("PLT.NT_DomInteractive",
                      dom_content_loaded_start - dom_interactive,
                      data_reduction_proxy_was_used);
  }
  if (!dom_content_loaded_start.is_null() &&
      !dom_content_loaded_end.is_null() ) {
    PLT_HISTOGRAM_DRP("PLT.NT_DomContentLoaded",
                      dom_content_loaded_end - dom_content_loaded_start,
                      data_reduction_proxy_was_used);
  }
  if (!dom_content_loaded_end.is_null() && !load_event_start.is_null()) {
    PLT_HISTOGRAM_DRP("PLT.NT_DelayBeforeLoadEvent",
                      load_event_start - dom_content_loaded_end,
                      data_reduction_proxy_was_used);
  }

  // TODO(simonjam): There is no way to distinguish between abandonment and
  // intentional Javascript navigation before the load event fires.
  // TODO(dominich): Load type breakdown
  if (!load_event_start.is_null()) {
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_BeginToFinishDoc",
                                   load_event_start - begin,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   preview_state, preview_experiment_id);
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_CommitToFinishDoc",
                                   load_event_start - response_start,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   preview_state, preview_experiment_id);
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_RequestToFinishDoc",
                                   load_event_start - navigation_start,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   preview_state, preview_experiment_id);
    if (data_reduction_proxy_was_used) {
      PLT_HISTOGRAM("PLT.PT_BeginToFinishDoc_DataReductionProxy",
                    load_event_start - begin);
      PLT_HISTOGRAM("PLT.PT_CommitToFinishDoc_DataReductionProxy",
                    load_event_start - response_start);
      PLT_HISTOGRAM("PLT.PT_RequestToFinishDoc_DataReductionProxy",
                    load_event_start - navigation_start);
    }
  }
  if (!load_event_end.is_null()) {
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_BeginToFinish",
                                   load_event_end - begin,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   preview_state, preview_experiment_id);
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_CommitToFinish",
                                   load_event_end - response_start,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   preview_state, preview_experiment_id);
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_RequestToFinish",
                                   load_event_end - navigation_start,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   preview_state, preview_experiment_id);
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_StartToFinish",
                                   load_event_end - request_start,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   preview_state, preview_experiment_id);
    if (data_reduction_proxy_was_used) {
      PLT_HISTOGRAM("PLT.PT_BeginToFinish_DataReductionProxy",
                    load_event_end - begin);
      PLT_HISTOGRAM("PLT.PT_CommitToFinish_DataReductionProxy",
                    load_event_end - response_start);
      PLT_HISTOGRAM("PLT.PT_RequestToFinish_DataReductionProxy",
                    load_event_end - navigation_start);
      PLT_HISTOGRAM("PLT.PT_StartToFinish_DataReductionProxy",
                    load_event_end - request_start);
    }
  }
  if (!load_event_start.is_null() && !load_event_end.is_null()) {
    PLT_HISTOGRAM("PLT.PT_FinishDocToFinish",
                  load_event_end - load_event_start);
    PLT_HISTOGRAM_DRP("PLT.NT_LoadEvent",
                      load_event_end - load_event_start,
                      data_reduction_proxy_was_used);

    if (data_reduction_proxy_was_used)
      PLT_HISTOGRAM("PLT.PT_FinishDocToFinish_DataReductionProxy",
                    load_event_end - load_event_start);
  }
  if (!dom_content_loaded_start.is_null()) {
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_RequestToDomContentLoaded",
                                   dom_content_loaded_start - navigation_start,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   preview_state, preview_experiment_id);
    if (data_reduction_proxy_was_used)
      PLT_HISTOGRAM("PLT.PT_RequestToDomContentLoaded_DataReductionProxy",
                    dom_content_loaded_start - navigation_start);
  }
  PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_BeginToCommit",
                                 response_start - begin,
                                 came_from_websearch,
                                 websearch_chrome_joint_experiment_id,
                                 preview_state, preview_experiment_id);
  PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_RequestToStart",
                                 request_start - navigation_start,
                                 came_from_websearch,
                                 websearch_chrome_joint_experiment_id,
                                 preview_state, preview_experiment_id);
  PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_StartToCommit",
                                 response_start - request_start,
                                 came_from_websearch,
                                 websearch_chrome_joint_experiment_id,
                                 preview_state, preview_experiment_id);
  PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.PT_RequestToCommit",
                                 response_start - navigation_start,
                                 came_from_websearch,
                                 websearch_chrome_joint_experiment_id,
                                 preview_state, preview_experiment_id);
  if (data_reduction_proxy_was_used) {
    PLT_HISTOGRAM("PLT.PT_BeginToCommit_DataReductionProxy",
                  response_start - begin);
    PLT_HISTOGRAM("PLT.PT_RequestToStart_DataReductionProxy",
                  request_start - navigation_start);
    PLT_HISTOGRAM("PLT.PT_StartToCommit_DataReductionProxy",
                  response_start - request_start);
    PLT_HISTOGRAM("PLT.PT_RequestToCommit_DataReductionProxy",
                  response_start - navigation_start);
  }
}

enum MissingStartType {
  START_MISSING = 0x1,
  COMMIT_MISSING = 0x2,
  NAV_START_MISSING = 0x4,
  MISSING_START_TYPE_MAX = 0x8
};

enum AbandonType {
  FINISH_DOC_MISSING = 0x1,
  FINISH_ALL_LOADS_MISSING = 0x2,
  LOAD_EVENT_START_MISSING = 0x4,
  LOAD_EVENT_END_MISSING = 0x8,
  ABANDON_TYPE_MAX = 0x10
};

}  // namespace

PageLoadHistograms::PageLoadHistograms(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      cross_origin_access_count_(0),
      same_origin_access_count_(0) {
}

void PageLoadHistograms::Dump(WebFrame* frame) {
  // We only dump histograms for main frames.
  // In the future, it may be interesting to tag subframes and dump them too.
  if (!frame || frame->parent())
    return;

  // Only dump for supported schemes.
  URLPattern::SchemeMasks scheme_type =
      GetSupportedSchemeType(frame->document().url());
  if (scheme_type == 0)
    return;

  // Ignore multipart requests.
  if (frame->dataSource()->response().isMultipartPayload())
    return;

  DocumentState* document_state =
      DocumentState::FromDataSource(frame->dataSource());

  bool data_reduction_proxy_was_used = DataReductionProxyWasUsed(frame);
  bool came_from_websearch =
      IsFromGoogleSearchResult(frame->document().url(),
                               GURL(frame->document().referrer()));
  int websearch_chrome_joint_experiment_id = kNoExperiment;
  if (came_from_websearch) {
    websearch_chrome_joint_experiment_id =
        GetQueryStringBasedExperiment(GURL(frame->document().referrer()));
  }

  GwsPreviewState preview_state = PREVIEW_NONE;
  int preview_experiment_id = websearch_chrome_joint_experiment_id;
  GetPreviewState(frame, came_from_websearch, data_reduction_proxy_was_used,
                  &preview_state, &preview_experiment_id);

  // Times based on the Web Timing metrics.
  // http://www.w3.org/TR/navigation-timing/
  // TODO(tonyg, jar): We are in the process of vetting these metrics against
  // the existing ones. Once we understand any differences, we will standardize
  // on a single set of metrics.
  DumpPerformanceTiming(frame->performance(), document_state,
                        data_reduction_proxy_was_used,
                        came_from_websearch,
                        websearch_chrome_joint_experiment_id,
                        preview_state, preview_experiment_id);

  // If we've already dumped, do nothing.
  // This simple bool works because we only dump for the main frame.
  if (document_state->load_histograms_recorded())
    return;

  // Collect measurement times.
  Time start = document_state->start_load_time();
  Time commit = document_state->commit_load_time();

  // TODO(tonyg, jar): Start can be missing after an in-document navigation and
  // possibly other cases like a very premature abandonment of the page.
  // The PLT.MissingStart histogram should help us troubleshoot and then we can
  // remove this.
  Time navigation_start =
      Time::FromDoubleT(frame->performance().navigationStart());
  int missing_start_type = 0;
  missing_start_type |= start.is_null() ? START_MISSING : 0;
  missing_start_type |= commit.is_null() ? COMMIT_MISSING : 0;
  missing_start_type |= navigation_start.is_null() ? NAV_START_MISSING : 0;
  UMA_HISTOGRAM_ENUMERATION("PLT.MissingStart", missing_start_type,
                            MISSING_START_TYPE_MAX);
  if (missing_start_type)
    return;

  // We properly handle null values for the next 3 variables.
  Time request = document_state->request_time();
  Time first_paint = document_state->first_paint_time();
  Time first_paint_after_load = document_state->first_paint_after_load_time();
  Time finish_doc = document_state->finish_document_load_time();
  Time finish_all_loads = document_state->finish_load_time();

  // TODO(tonyg, jar): We suspect a bug in abandonment counting, this temporary
  // histogram should help us to troubleshoot.
  Time load_event_start =
      Time::FromDoubleT(frame->performance().loadEventStart());
  Time load_event_end = Time::FromDoubleT(frame->performance().loadEventEnd());
  int abandon_type = 0;
  abandon_type |= finish_doc.is_null() ? FINISH_DOC_MISSING : 0;
  abandon_type |= finish_all_loads.is_null() ? FINISH_ALL_LOADS_MISSING : 0;
  abandon_type |= load_event_start.is_null() ? LOAD_EVENT_START_MISSING : 0;
  abandon_type |= load_event_end.is_null() ? LOAD_EVENT_END_MISSING : 0;
  UMA_HISTOGRAM_ENUMERATION("PLT.AbandonType", abandon_type, ABANDON_TYPE_MAX);

  // Handle case where user hits "stop" or "back" before loading completely.
  bool abandoned_page = finish_doc.is_null();
  if (abandoned_page) {
    finish_doc = Time::Now();
    document_state->set_finish_document_load_time(finish_doc);
  }

  // TODO(jar): We should really discriminate the definition of "abandon" more
  // finely.  We should have:
  // abandon_before_document_loaded
  // abandon_before_onload_fired

  if (finish_all_loads.is_null()) {
    finish_all_loads = Time::Now();
    document_state->set_finish_load_time(finish_all_loads);
  } else {
    DCHECK(!abandoned_page);  // How can the doc have finished but not the page?
    if (abandoned_page)
      return;  // Don't try to record a stat which is broken.
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
  UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned", abandoned_page ? 1 : 0, 2);
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
                                     preview_state, preview_experiment_id);
    }
    DCHECK(commit <= first_paint);
    commit_to_first_paint.reset(new TimeDelta(first_paint - commit));
    PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.CommitToFirstPaint",
                                   *commit_to_first_paint,
                                   came_from_websearch,
                                   websearch_chrome_joint_experiment_id,
                                   preview_state, preview_experiment_id);
  }
  if (!first_paint_after_load.is_null()) {
    // 'first_paint_after_load' can be before 'begin' for an unknown reason.
    // See bug http://crbug.com/125273 for details.
    if (begin <= first_paint_after_load) {
      PLT_HISTOGRAM("PLT.BeginToFirstPaintAfterLoad",
          first_paint_after_load - begin);
    }
    DCHECK(commit <= first_paint_after_load);
    PLT_HISTOGRAM("PLT.CommitToFirstPaintAfterLoad",
        first_paint_after_load - commit);
    DCHECK(finish_all_loads <= first_paint_after_load);
    PLT_HISTOGRAM("PLT.FinishToFirstPaintAfterLoad",
        first_paint_after_load - finish_all_loads);
  }
  PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.BeginToFinishDoc", begin_to_finish_doc,
                                 came_from_websearch,
                                 websearch_chrome_joint_experiment_id,
                                 preview_state, preview_experiment_id);
  PLT_HISTOGRAM_WITH_GWS_VARIANT("PLT.BeginToFinish", begin_to_finish_all_loads,
                                 came_from_websearch,
                                 websearch_chrome_joint_experiment_id,
                                 preview_state, preview_experiment_id);

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
    UMA_HISTOGRAM_ENUMERATION(
        "PLT.Abandoned_SpdyProxy", abandoned_page ? 1 : 0, 2);
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
  if (document_state->was_after_preconnect_request()) {
    PLT_HISTOGRAM("PLT.BeginToFinishDoc_AfterPreconnectRequest",
                  begin_to_finish_doc);
    PLT_HISTOGRAM("PLT.BeginToFinish_AfterPreconnectRequest",
                  begin_to_finish_all_loads);
  }

  // TODO(mpcomplete): remove the extension-related histograms after we collect
  // enough data. http://crbug.com/100411
  const bool use_adblock_histogram =
      ChromeContentRendererClient::IsAdblockInstalled();
  if (use_adblock_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        "PLT.Abandoned_ExtensionAdblock",
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_NormalLoad_ExtensionAdblock",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadNormal_ExtensionAdblock",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadReload_ExtensionAdblock",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadStaleOk_ExtensionAdblock",
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  const bool use_adblockplus_histogram =
      ChromeContentRendererClient::IsAdblockPlusInstalled();
  if (use_adblockplus_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        "PLT.Abandoned_ExtensionAdblockPlus",
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_NormalLoad_ExtensionAdblockPlus",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadNormal_ExtensionAdblockPlus",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadReload_ExtensionAdblockPlus",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadStaleOk_ExtensionAdblockPlus",
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  const bool use_webrequest_adblock_histogram =
      ChromeContentRendererClient::IsAdblockWithWebRequestInstalled();
  if (use_webrequest_adblock_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        "PLT.Abandoned_ExtensionWebRequestAdblock",
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_NormalLoad_ExtensionWebRequestAdblock",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadNormal_ExtensionWebRequestAdblock",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadReload_ExtensionWebRequestAdblock",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadStaleOk_ExtensionWebRequestAdblock",
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  const bool use_webrequest_adblockplus_histogram =
      ChromeContentRendererClient::
          IsAdblockPlusWithWebRequestInstalled();
  if (use_webrequest_adblockplus_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        "PLT.Abandoned_ExtensionWebRequestAdblockPlus",
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_NormalLoad_ExtensionWebRequestAdblockPlus",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadNormal_ExtensionWebRequestAdblockPlus",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadReload_ExtensionWebRequestAdblockPlus",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadStaleOk_ExtensionWebRequestAdblockPlus",
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  const bool use_webrequest_other_histogram =
      ChromeContentRendererClient::
          IsOtherExtensionWithWebRequestInstalled();
  if (use_webrequest_other_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        "PLT.Abandoned_ExtensionWebRequestOther",
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_NormalLoad_ExtensionWebRequestOther",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadNormal_ExtensionWebRequestOther",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadReload_ExtensionWebRequestOther",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadStaleOk_ExtensionWebRequestOther",
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Record SpdyCwnd results.
  if (document_state->was_fetched_via_spdy()) {
    switch (load_type) {
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadNormal_cwndDynamic",
                      begin_to_finish_all_loads);
        PLT_HISTOGRAM("PLT.StartToFinish_LinkLoadNormal_cwndDynamic",
                      start_to_finish_all_loads);
        PLT_HISTOGRAM("PLT.StartToCommit_LinkLoadNormal_cwndDynamic",
                      start_to_commit);
        break;
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM("PLT.BeginToFinish_NormalLoad_cwndDynamic",
                      begin_to_finish_all_loads);
        PLT_HISTOGRAM("PLT.StartToFinish_NormalLoad_cwndDynamic",
                      start_to_finish_all_loads);
        PLT_HISTOGRAM("PLT.StartToCommit_NormalLoad_cwndDynamic",
                      start_to_commit);
        break;
      default:
        break;
    }
  }

  // Record page load time and abandonment rates for proxy cases.
  if (document_state->was_fetched_via_proxy()) {
    if (scheme_type == URLPattern::SCHEME_HTTPS) {
      PLT_HISTOGRAM("PLT.StartToFinish.Proxy.https", start_to_finish_all_loads);
      UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned.Proxy.https",
                                abandoned_page ? 1 : 0, 2);
    } else {
      DCHECK(scheme_type == URLPattern::SCHEME_HTTP);
      PLT_HISTOGRAM("PLT.StartToFinish.Proxy.http", start_to_finish_all_loads);
      UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned.Proxy.http",
                                abandoned_page ? 1 : 0, 2);
    }
  } else {
    if (scheme_type == URLPattern::SCHEME_HTTPS) {
      PLT_HISTOGRAM("PLT.StartToFinish.NoProxy.https",
                    start_to_finish_all_loads);
      UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned.NoProxy.https",
                                abandoned_page ? 1 : 0, 2);
    } else {
      DCHECK(scheme_type == URLPattern::SCHEME_HTTP);
      PLT_HISTOGRAM("PLT.StartToFinish.NoProxy.http",
                    start_to_finish_all_loads);
      UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned.NoProxy.http",
                                abandoned_page ? 1 : 0, 2);
    }
  }

  // Site isolation metrics.
  UMA_HISTOGRAM_COUNTS("SiteIsolation.PageLoadsWithCrossSiteFrameAccess",
                       cross_origin_access_count_);
  UMA_HISTOGRAM_COUNTS("SiteIsolation.PageLoadsWithSameSiteFrameAccess",
                       same_origin_access_count_);

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

void PageLoadHistograms::ResetCrossFramePropertyAccess() {
  cross_origin_access_count_ = 0;
  same_origin_access_count_ = 0;
}

void PageLoadHistograms::FrameWillClose(WebFrame* frame) {
  Dump(frame);
}

void PageLoadHistograms::ClosePage() {
  // TODO(davemoore) This code should be removed once willClose() gets
  // called when a page is destroyed. page_load_histograms_.Dump() is safe
  // to call multiple times for the same frame, but it will simplify things.
  Dump(render_view()->GetWebView()->mainFrame());
  ResetCrossFramePropertyAccess();
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
