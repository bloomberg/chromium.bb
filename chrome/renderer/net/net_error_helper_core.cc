// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper_core.h"

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/common/localized_error.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

struct CorrectionTypeToResourceTable {
  int resource_id;
  const char* correction_type;
};

const CorrectionTypeToResourceTable kCorrectionResourceTable[] = {
  {IDS_ERRORPAGES_SUGGESTION_VISIT_GOOGLE_CACHE, "cachedPage"},
  // "reloadPage" is has special handling.
  {IDS_ERRORPAGES_SUGGESTION_CORRECTED_URL, "urlCorrection"},
  {IDS_ERRORPAGES_SUGGESTION_ALTERNATE_URL, "siteDomain"},
  {IDS_ERRORPAGES_SUGGESTION_ALTERNATE_URL, "host"},
  {IDS_ERRORPAGES_SUGGESTION_ALTERNATE_URL, "sitemap"},
  {IDS_ERRORPAGES_SUGGESTION_ALTERNATE_URL, "pathParentFolder"},
  // "siteSearchQuery" is not yet supported.
  // TODO(mmenke):  Figure out what format "siteSearchQuery" uses for its
  // suggestions.
  // "webSearchQuery" has special handling.
  {IDS_ERRORPAGES_SUGGESTION_ALTERNATE_URL, "contentOverlap"},
  {IDS_ERRORPAGES_SUGGESTION_CORRECTED_URL, "emphasizedUrlCorrection"},
};

struct NavigationCorrection {
  NavigationCorrection() : is_porn(false), is_soft_porn(false) {
  }

  static void RegisterJSONConverter(
      base::JSONValueConverter<NavigationCorrection>* converter) {
    converter->RegisterStringField("correctionType",
                                   &NavigationCorrection::correction_type);
    converter->RegisterStringField("urlCorrection",
                                   &NavigationCorrection::url_correction);
    converter->RegisterStringField("clickType",
                                   &NavigationCorrection::click_type);
    converter->RegisterStringField("clickData",
                                   &NavigationCorrection::click_data);
    converter->RegisterBoolField("isPorn", &NavigationCorrection::is_porn);
    converter->RegisterBoolField("isSoftPorn",
                                 &NavigationCorrection::is_soft_porn);
  }

  std::string correction_type;
  std::string url_correction;
  std::string click_type;
  std::string click_data;
  bool is_porn;
  bool is_soft_porn;
};

struct NavigationCorrectionResponse {
  std::string event_id;
  std::string fingerprint;
  ScopedVector<NavigationCorrection> corrections;

  static void RegisterJSONConverter(
      base::JSONValueConverter<NavigationCorrectionResponse>* converter) {
    converter->RegisterStringField("result.eventId",
                                   &NavigationCorrectionResponse::event_id);
    converter->RegisterStringField("result.fingerprint",
                                   &NavigationCorrectionResponse::fingerprint);
    converter->RegisterRepeatedMessage(
        "result.UrlCorrections",
        &NavigationCorrectionResponse::corrections);
  }
};

base::TimeDelta GetAutoReloadTime(size_t reload_count) {
  static const int kDelaysMs[] = {
    0, 5000, 30000, 60000, 300000, 600000, 1800000
  };
  if (reload_count >= arraysize(kDelaysMs))
    reload_count = arraysize(kDelaysMs) - 1;
  return base::TimeDelta::FromMilliseconds(kDelaysMs[reload_count]);
}

// Returns whether |net_error| is a DNS-related error (and therefore whether
// the tab helper should start a DNS probe after receiving it.)
bool IsDnsError(const blink::WebURLError& error) {
  return error.domain.utf8() == net::kErrorDomain &&
         (error.reason == net::ERR_NAME_NOT_RESOLVED ||
          error.reason == net::ERR_NAME_RESOLUTION_FAILED);
}

GURL SanitizeURL(const GURL& url) {
  GURL::Replacements remove_params;
  remove_params.ClearUsername();
  remove_params.ClearPassword();
  remove_params.ClearQuery();
  remove_params.ClearRef();
  return url.ReplaceComponents(remove_params);
}

// Sanitizes and formats a URL for upload to the error correction service.
std::string PrepareUrlForUpload(const GURL& url) {
  // TODO(yuusuke): Change to net::FormatUrl when Link Doctor becomes
  // unicode-capable.
  std::string spec_to_send = SanitizeURL(url).spec();

  // Notify navigation correction service of the url truncation by sending of
  // "?" at the end.
  if (url.has_query())
    spec_to_send.append("?");
  return spec_to_send;
}

// Given a WebURLError, returns true if the FixURL service should be used
// for that error.  Also sets |error_param| to the string that should be sent to
// the FixURL service to identify the error type.
bool ShouldUseFixUrlServiceForError(const blink::WebURLError& error,
                                    std::string* error_param) {
  error_param->clear();

  // Don't use the correction service for HTTPS (for privacy reasons).
  GURL unreachable_url(error.unreachableURL);
  if (GURL(unreachable_url).SchemeIsSecure())
    return false;

  std::string domain = error.domain.utf8();
  if (domain == "http" && error.reason == 404) {
    *error_param = "http404";
    return true;
  }
  if (IsDnsError(error)) {
    *error_param = "dnserror";
    return true;
  }
  if (domain == net::kErrorDomain &&
      (error.reason == net::ERR_CONNECTION_FAILED ||
       error.reason == net::ERR_CONNECTION_REFUSED ||
       error.reason == net::ERR_ADDRESS_UNREACHABLE ||
       error.reason == net::ERR_CONNECTION_TIMED_OUT)) {
    *error_param = "connectionFailure";
    return true;
  }
  return false;
}

// Creates a request body for use with the fixurl service.  Sets parameters
// shared by all types of requests to the service.  |correction_params| must
// contain the parameters specific to the actual request type.
std::string CreateRequestBody(
    const std::string& method,
    const std::string& error_param,
    const NetErrorHelperCore::NavigationCorrectionParams& correction_params,
    scoped_ptr<base::DictionaryValue> params_dict) {
  // Set params common to all request types.
  params_dict->SetString("key", correction_params.api_key);
  params_dict->SetString("clientName", "chrome");
  params_dict->SetString("error", error_param);

  if (!correction_params.language.empty())
    params_dict->SetString("language", correction_params.language);

  if (!correction_params.country_code.empty())
    params_dict->SetString("originCountry", correction_params.country_code);

  base::DictionaryValue request_dict;
  request_dict.SetString("method", method);
  request_dict.SetString("apiVersion", "v1");
  request_dict.Set("params", params_dict.release());

  std::string request_body;
  bool success = base::JSONWriter::Write(&request_dict, &request_body);
  DCHECK(success);
  return request_body;
}

// If URL correction information should be retrieved remotely for a main frame
// load that failed with |error|, returns true and sets
// |correction_request_body| to be the body for the correction request.
std::string CreateFixUrlRequestBody(
    const blink::WebURLError& error,
    const NetErrorHelperCore::NavigationCorrectionParams& correction_params) {
  std::string error_param;
  bool result = ShouldUseFixUrlServiceForError(error, &error_param);
  DCHECK(result);

  // TODO(mmenke):  Investigate open sourcing the relevant protocol buffers and
  //                using those directly instead.
  scoped_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetString("urlQuery", PrepareUrlForUpload(error.unreachableURL));
  return CreateRequestBody("linkdoctor.fixurl.fixurl", error_param,
                           correction_params, params.Pass());
}

std::string CreateClickTrackingUrlRequestBody(
    const blink::WebURLError& error,
    const NetErrorHelperCore::NavigationCorrectionParams& correction_params,
    const NavigationCorrectionResponse& response,
    const NavigationCorrection& correction) {
  std::string error_param;
  bool result = ShouldUseFixUrlServiceForError(error, &error_param);
  DCHECK(result);

  scoped_ptr<base::DictionaryValue> params(new base::DictionaryValue());

  params->SetString("originalUrlQuery",
                    PrepareUrlForUpload(error.unreachableURL));

  params->SetString("clickedUrlCorrection", correction.url_correction);
  params->SetString("clickType", correction.click_type);
  params->SetString("clickData", correction.click_data);

  params->SetString("eventId", response.event_id);
  params->SetString("fingerprint", response.fingerprint);

  return CreateRequestBody("linkdoctor.fixurl.clicktracking", error_param,
                           correction_params, params.Pass());
}

base::string16 FormatURLForDisplay(const GURL& url, bool is_rtl,
                                   const std::string accept_languages) {
  // Translate punycode into UTF8, unescape UTF8 URLs.
  base::string16 url_for_display(net::FormatUrl(
      url, accept_languages, net::kFormatUrlOmitNothing,
      net::UnescapeRule::NORMAL, NULL, NULL, NULL));
  // URLs are always LTR.
  if (is_rtl)
    base::i18n::WrapStringWithLTRFormatting(&url_for_display);
  return url_for_display;
}

scoped_ptr<NavigationCorrectionResponse> ParseNavigationCorrectionResponse(
    const std::string raw_response) {
  // TODO(mmenke):  Open source related protocol buffers and use them directly.
  scoped_ptr<base::Value> parsed(base::JSONReader::Read(raw_response));
  scoped_ptr<NavigationCorrectionResponse> response(
      new NavigationCorrectionResponse());
  base::JSONValueConverter<NavigationCorrectionResponse> converter;
  if (!parsed || !converter.Convert(*parsed, response.get()))
    response.reset();
  return response.Pass();
}

scoped_ptr<LocalizedError::ErrorPageParams> CreateErrorPageParams(
    const NavigationCorrectionResponse& response,
    const blink::WebURLError& error,
    const NetErrorHelperCore::NavigationCorrectionParams& correction_params,
    const std::string& accept_languages,
    bool is_rtl) {
  // Version of URL for display in suggestions.  It has to be sanitized first
  // because any received suggestions will be relative to the sanitized URL.
  base::string16 original_url_for_display =
      FormatURLForDisplay(SanitizeURL(GURL(error.unreachableURL)), is_rtl,
                          accept_languages);

  scoped_ptr<LocalizedError::ErrorPageParams> params(
      new LocalizedError::ErrorPageParams());
  params->override_suggestions.reset(new base::ListValue());
  scoped_ptr<base::ListValue> parsed_corrections(new base::ListValue());
  for (ScopedVector<NavigationCorrection>::const_iterator it =
           response.corrections.begin();
       it != response.corrections.end(); ++it) {
    // Doesn't seem like a good idea to show these.
    if ((*it)->is_porn || (*it)->is_soft_porn)
      continue;

    int tracking_id = it - response.corrections.begin();

    if ((*it)->correction_type == "reloadPage") {
      params->suggest_reload = true;
      params->reload_tracking_id = tracking_id;
      continue;
    }

    if ((*it)->correction_type == "webSearchQuery") {
      // If there are mutliple searches suggested, use the first suggestion.
      if (params->search_terms.empty()) {
        params->search_url = correction_params.search_url;
        params->search_terms = (*it)->url_correction;
        params->search_tracking_id = tracking_id;
      }
      continue;
    }

    // Allow reload page and web search query to be empty strings, but not
    // links.
    if ((*it)->url_correction.empty())
      continue;
    size_t correction_index;
    for (correction_index = 0;
         correction_index < arraysize(kCorrectionResourceTable);
         ++correction_index) {
      if ((*it)->correction_type !=
              kCorrectionResourceTable[correction_index].correction_type) {
        continue;
      }
      base::DictionaryValue* suggest = new base::DictionaryValue();
      suggest->SetString("header",
          l10n_util::GetStringUTF16(
              kCorrectionResourceTable[correction_index].resource_id));
      suggest->SetString("urlCorrection", (*it)->url_correction);
      suggest->SetString(
          "urlCorrectionForDisplay",
          FormatURLForDisplay(GURL((*it)->url_correction), is_rtl,
                              accept_languages));
      suggest->SetString("originalUrlForDisplay", original_url_for_display);
      suggest->SetInteger("trackingId", tracking_id);
      params->override_suggestions->Append(suggest);
      break;
    }
  }

  if (params->override_suggestions->empty() && !params->search_url.is_valid())
    params.reset();
  return params.Pass();
}

void ReportAutoReloadSuccess(const blink::WebURLError& error, size_t count) {
  if (error.domain.utf8() != net::kErrorDomain)
    return;
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("Net.AutoReload.ErrorAtSuccess",
                                   -error.reason,
                                   net::GetAllErrorCodesForUma());
  UMA_HISTOGRAM_COUNTS("Net.AutoReload.CountAtSuccess", count);
  if (count == 1) {
    UMA_HISTOGRAM_CUSTOM_ENUMERATION("Net.AutoReload.ErrorAtFirstSuccess",
                                     -error.reason,
                                     net::GetAllErrorCodesForUma());
  }
}

void ReportAutoReloadFailure(const blink::WebURLError& error, size_t count) {
  if (error.domain.utf8() != net::kErrorDomain)
    return;
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("Net.AutoReload.ErrorAtStop",
                                   -error.reason,
                                   net::GetAllErrorCodesForUma());
  UMA_HISTOGRAM_COUNTS("Net.AutoReload.CountAtStop", count);
}

}  // namespace

struct NetErrorHelperCore::ErrorPageInfo {
  ErrorPageInfo(blink::WebURLError error, bool was_failed_post)
      : error(error),
        was_failed_post(was_failed_post),
        needs_dns_updates(false),
        needs_load_navigation_corrections(false),
        reload_button_in_page(false),
        load_stale_button_in_page(false),
        is_finished_loading(false),
        auto_reload_triggered(false) {
  }

  // Information about the failed page load.
  blink::WebURLError error;
  bool was_failed_post;

  // Information about the status of the error page.

  // True if a page is a DNS error page and has not yet received a final DNS
  // probe status.
  bool needs_dns_updates;

  // True if a blank page was loaded, and navigation corrections need to be
  // loaded to generate the real error page.
  bool needs_load_navigation_corrections;

  // Navigation correction service paramers, which will be used in response to
  // certain types of network errors.  They are all stored here in case they
  // change over the course of displaying the error page.
  scoped_ptr<NetErrorHelperCore::NavigationCorrectionParams>
      navigation_correction_params;

  scoped_ptr<NavigationCorrectionResponse> navigation_correction_response;

  // All the navigation corrections that have been clicked, for tracking
  // purposes.
  std::set<int> clicked_corrections;

  // Track if specific buttons are included in an error page, for statistics.
  bool reload_button_in_page;
  bool load_stale_button_in_page;

  // True if a page has completed loading, at which point it can receive
  // updates.
  bool is_finished_loading;

  // True if the auto-reload timer has fired and a reload is or has been in
  // flight.
  bool auto_reload_triggered;
};

NetErrorHelperCore::NavigationCorrectionParams::NavigationCorrectionParams() {
}

NetErrorHelperCore::NavigationCorrectionParams::~NavigationCorrectionParams() {
}

bool NetErrorHelperCore::IsReloadableError(
    const NetErrorHelperCore::ErrorPageInfo& info) {
  return info.error.domain.utf8() == net::kErrorDomain &&
         info.error.reason != net::ERR_ABORTED &&
         !info.was_failed_post;
}

NetErrorHelperCore::NetErrorHelperCore(Delegate* delegate,
                                       bool auto_reload_enabled,
                                       bool auto_reload_visible_only,
                                       bool is_visible)
    : delegate_(delegate),
      last_probe_status_(chrome_common_net::DNS_PROBE_POSSIBLE),
      auto_reload_enabled_(auto_reload_enabled),
      auto_reload_visible_only_(auto_reload_visible_only),
      auto_reload_timer_(new base::Timer(false, false)),
      auto_reload_paused_(false),
      uncommitted_load_started_(false),
      // TODO(ellyjones): Make online_ accurate at object creation.
      online_(true),
      visible_(is_visible),
      auto_reload_count_(0),
      navigation_from_button_(NO_BUTTON) {
}

NetErrorHelperCore::~NetErrorHelperCore() {
  if (committed_error_page_info_ &&
      committed_error_page_info_->auto_reload_triggered) {
    ReportAutoReloadFailure(committed_error_page_info_->error,
                            auto_reload_count_);
  }
}

void NetErrorHelperCore::CancelPendingFetches() {
  // Cancel loading the alternate error page, and prevent any pending error page
  // load from starting a new error page load.  Swapping in the error page when
  // it's finished loading could abort the navigation, otherwise.
  if (committed_error_page_info_)
    committed_error_page_info_->needs_load_navigation_corrections = false;
  if (pending_error_page_info_)
    pending_error_page_info_->needs_load_navigation_corrections = false;
  delegate_->CancelFetchNavigationCorrections();
  auto_reload_timer_->Stop();
  auto_reload_paused_ = false;
}

void NetErrorHelperCore::OnStop() {
  if (committed_error_page_info_ &&
      committed_error_page_info_->auto_reload_triggered) {
    ReportAutoReloadFailure(committed_error_page_info_->error,
                            auto_reload_count_);
  }
  CancelPendingFetches();
  uncommitted_load_started_ = false;
  auto_reload_count_ = 0;
}

void NetErrorHelperCore::OnWasShown() {
  visible_ = true;
  if (!auto_reload_visible_only_)
    return;
  if (auto_reload_paused_)
    MaybeStartAutoReloadTimer();
}

void NetErrorHelperCore::OnWasHidden() {
  visible_ = false;
  if (!auto_reload_visible_only_)
    return;
  PauseAutoReloadTimer();
}

void NetErrorHelperCore::OnStartLoad(FrameType frame_type, PageType page_type) {
  if (frame_type != MAIN_FRAME)
    return;

  uncommitted_load_started_ = true;

  // If there's no pending error page information associated with the page load,
  // or the new page is not an error page, then reset pending error page state.
  if (!pending_error_page_info_ || page_type != ERROR_PAGE)
    CancelPendingFetches();
}

void NetErrorHelperCore::OnCommitLoad(FrameType frame_type, const GURL& url) {
  if (frame_type != MAIN_FRAME)
    return;

  // uncommitted_load_started_ could already be false, since RenderFrameImpl
  // calls OnCommitLoad once for each in-page navigation (like a fragment
  // change) with no corresponding OnStartLoad.
  uncommitted_load_started_ = false;

  // Track if an error occurred due to a page button press.
  // This isn't perfect; if (for instance), the server is slow responding
  // to a request generated from the page reload button, and the user hits
  // the browser reload button, this code will still believe the
  // result is from the page reload button.
  if (committed_error_page_info_ && pending_error_page_info_ &&
      navigation_from_button_ != NO_BUTTON &&
      committed_error_page_info_->error.unreachableURL ==
          pending_error_page_info_->error.unreachableURL) {
    DCHECK(navigation_from_button_ == RELOAD_BUTTON ||
           navigation_from_button_ == LOAD_STALE_BUTTON);
    chrome_common_net::RecordEvent(
        navigation_from_button_ == RELOAD_BUTTON ?
            chrome_common_net::NETWORK_ERROR_PAGE_RELOAD_BUTTON_ERROR :
            chrome_common_net::NETWORK_ERROR_PAGE_LOAD_STALE_BUTTON_ERROR);
  }
  navigation_from_button_ = NO_BUTTON;

  if (committed_error_page_info_ && !pending_error_page_info_ &&
      committed_error_page_info_->auto_reload_triggered) {
    const blink::WebURLError& error = committed_error_page_info_->error;
    const GURL& error_url = error.unreachableURL;
    if (url == error_url)
      ReportAutoReloadSuccess(error, auto_reload_count_);
    else if (url != GURL(content::kUnreachableWebDataURL))
      ReportAutoReloadFailure(error, auto_reload_count_);
  }

  committed_error_page_info_.reset(pending_error_page_info_.release());
}

void NetErrorHelperCore::OnFinishLoad(FrameType frame_type) {
  if (frame_type != MAIN_FRAME)
    return;

  if (!committed_error_page_info_) {
    auto_reload_count_ = 0;
    return;
  }

  committed_error_page_info_->is_finished_loading = true;

  chrome_common_net::RecordEvent(chrome_common_net::NETWORK_ERROR_PAGE_SHOWN);
  if (committed_error_page_info_->reload_button_in_page) {
    chrome_common_net::RecordEvent(
        chrome_common_net::NETWORK_ERROR_PAGE_RELOAD_BUTTON_SHOWN);
  }
  if (committed_error_page_info_->load_stale_button_in_page) {
    chrome_common_net::RecordEvent(
        chrome_common_net::NETWORK_ERROR_PAGE_LOAD_STALE_BUTTON_SHOWN);
  }

  delegate_->EnablePageHelperFunctions();

  if (committed_error_page_info_->needs_load_navigation_corrections) {
    // If there is another pending error page load, |fix_url| should have been
    // cleared.
    DCHECK(!pending_error_page_info_);
    DCHECK(!committed_error_page_info_->needs_dns_updates);
    delegate_->FetchNavigationCorrections(
        committed_error_page_info_->navigation_correction_params->url,
        CreateFixUrlRequestBody(
            committed_error_page_info_->error,
            *committed_error_page_info_->navigation_correction_params));
  } else if (auto_reload_enabled_ &&
             IsReloadableError(*committed_error_page_info_)) {
    MaybeStartAutoReloadTimer();
  }

  if (!committed_error_page_info_->needs_dns_updates ||
      last_probe_status_ == chrome_common_net::DNS_PROBE_POSSIBLE) {
    return;
  }
  DVLOG(1) << "Error page finished loading; sending saved status.";
  UpdateErrorPage();
}

void NetErrorHelperCore::GetErrorHTML(
    FrameType frame_type,
    const blink::WebURLError& error,
    bool is_failed_post,
    std::string* error_html) {
  if (frame_type == MAIN_FRAME) {
    // If navigation corrections were needed before, that should have been
    // cancelled earlier by starting a new page load (Which has now failed).
    DCHECK(!committed_error_page_info_ ||
           !committed_error_page_info_->needs_load_navigation_corrections);

    pending_error_page_info_.reset(new ErrorPageInfo(error, is_failed_post));
    pending_error_page_info_->navigation_correction_params.reset(
        new NavigationCorrectionParams(navigation_correction_params_));
    GetErrorHtmlForMainFrame(pending_error_page_info_.get(), error_html);
  } else {
    // These values do not matter, as error pages in iframes hide the buttons.
    bool reload_button_in_page;
    bool load_stale_button_in_page;

    delegate_->GenerateLocalizedErrorPage(
        error, is_failed_post, scoped_ptr<LocalizedError::ErrorPageParams>(),
        &reload_button_in_page, &load_stale_button_in_page,
        error_html);
  }
}

void NetErrorHelperCore::OnNetErrorInfo(
    chrome_common_net::DnsProbeStatus status) {
  DCHECK_NE(chrome_common_net::DNS_PROBE_POSSIBLE, status);

  last_probe_status_ = status;

  if (!committed_error_page_info_ ||
      !committed_error_page_info_->needs_dns_updates ||
      !committed_error_page_info_->is_finished_loading) {
    return;
  }

  UpdateErrorPage();
}

void NetErrorHelperCore::OnSetNavigationCorrectionInfo(
    const GURL& navigation_correction_url,
    const std::string& language,
    const std::string& country_code,
    const std::string& api_key,
    const GURL& search_url) {
  navigation_correction_params_.url = navigation_correction_url;
  navigation_correction_params_.language = language;
  navigation_correction_params_.country_code = country_code;
  navigation_correction_params_.api_key = api_key;
  navigation_correction_params_.search_url = search_url;
}

void NetErrorHelperCore::GetErrorHtmlForMainFrame(
    ErrorPageInfo* pending_error_page_info,
    std::string* error_html) {
  std::string error_param;
  blink::WebURLError error = pending_error_page_info->error;

  if (pending_error_page_info->navigation_correction_params &&
      pending_error_page_info->navigation_correction_params->url.is_valid() &&
      ShouldUseFixUrlServiceForError(error, &error_param)) {
    pending_error_page_info->needs_load_navigation_corrections = true;
    return;
  }

  if (IsDnsError(pending_error_page_info->error)) {
    // The last probe status needs to be reset if this is a DNS error.  This
    // means that if a DNS error page is committed but has not yet finished
    // loading, a DNS probe status scheduled to be sent to it may be thrown
    // out, but since the new error page should trigger a new DNS probe, it
    // will just get the results for the next page load.
    last_probe_status_ = chrome_common_net::DNS_PROBE_POSSIBLE;
    pending_error_page_info->needs_dns_updates = true;
    error = GetUpdatedError(error);
  }

  delegate_->GenerateLocalizedErrorPage(
      error, pending_error_page_info->was_failed_post,
      scoped_ptr<LocalizedError::ErrorPageParams>(),
      &pending_error_page_info->reload_button_in_page,
      &pending_error_page_info->load_stale_button_in_page,
      error_html);
}

void NetErrorHelperCore::UpdateErrorPage() {
  DCHECK(committed_error_page_info_->needs_dns_updates);
  DCHECK(committed_error_page_info_->is_finished_loading);
  DCHECK_NE(chrome_common_net::DNS_PROBE_POSSIBLE, last_probe_status_);

  UMA_HISTOGRAM_ENUMERATION("DnsProbe.ErrorPageUpdateStatus",
                            last_probe_status_,
                            chrome_common_net::DNS_PROBE_MAX);
  // Every status other than DNS_PROBE_POSSIBLE and DNS_PROBE_STARTED is a
  // final status code.  Once one is reached, the page does not need further
  // updates.
  if (last_probe_status_ != chrome_common_net::DNS_PROBE_STARTED)
    committed_error_page_info_->needs_dns_updates = false;

  // There is no need to worry about the button display statistics here because
  // the presentation of the reload and load stale buttons can't be changed
  // by a DNS error update.
  delegate_->UpdateErrorPage(
      GetUpdatedError(committed_error_page_info_->error),
      committed_error_page_info_->was_failed_post);
}

void NetErrorHelperCore::OnNavigationCorrectionsFetched(
    const std::string& corrections,
    const std::string& accept_languages,
    bool is_rtl) {
  // Loading suggestions only starts when a blank error page finishes loading,
  // and is cancelled with a new load.
  DCHECK(!pending_error_page_info_);
  DCHECK(committed_error_page_info_->is_finished_loading);
  DCHECK(committed_error_page_info_->needs_load_navigation_corrections);
  DCHECK(committed_error_page_info_->navigation_correction_params);

  pending_error_page_info_.reset(
      new ErrorPageInfo(committed_error_page_info_->error,
                        committed_error_page_info_->was_failed_post));
  pending_error_page_info_->navigation_correction_response =
      ParseNavigationCorrectionResponse(corrections);

  std::string error_html;
  scoped_ptr<LocalizedError::ErrorPageParams> params;
  if (pending_error_page_info_->navigation_correction_response) {
    // Copy navigation correction parameters used for the request, so tracking
    // requests can still be sent if the configuration changes.
    pending_error_page_info_->navigation_correction_params.reset(
        new NavigationCorrectionParams(
            *committed_error_page_info_->navigation_correction_params));
    params = CreateErrorPageParams(
          *pending_error_page_info_->navigation_correction_response,
          pending_error_page_info_->error,
          *pending_error_page_info_->navigation_correction_params,
          accept_languages, is_rtl);
    delegate_->GenerateLocalizedErrorPage(
        pending_error_page_info_->error,
        pending_error_page_info_->was_failed_post,
        params.Pass(),
        &pending_error_page_info_->reload_button_in_page,
        &pending_error_page_info_->load_stale_button_in_page,
        &error_html);
  } else {
    // Since |navigation_correction_params| in |pending_error_page_info_| is
    // NULL, this won't trigger another attempt to load corrections.
    GetErrorHtmlForMainFrame(pending_error_page_info_.get(), &error_html);
  }

  // TODO(mmenke):  Once the new API is in place, look into replacing this
  //                double page load by just updating the error page, like DNS
  //                probes do.
  delegate_->LoadErrorPageInMainFrame(
      error_html,
      pending_error_page_info_->error.unreachableURL);
}

blink::WebURLError NetErrorHelperCore::GetUpdatedError(
    const blink::WebURLError& error) const {
  // If a probe didn't run or wasn't conclusive, restore the original error.
  if (last_probe_status_ == chrome_common_net::DNS_PROBE_NOT_RUN ||
      last_probe_status_ ==
          chrome_common_net::DNS_PROBE_FINISHED_INCONCLUSIVE) {
    return error;
  }

  blink::WebURLError updated_error;
  updated_error.domain = blink::WebString::fromUTF8(
      chrome_common_net::kDnsProbeErrorDomain);
  updated_error.reason = last_probe_status_;
  updated_error.unreachableURL = error.unreachableURL;
  updated_error.staleCopyInCache = error.staleCopyInCache;

  return updated_error;
}

void NetErrorHelperCore::Reload() {
  if (!committed_error_page_info_) {
    return;
  }
  delegate_->ReloadPage();
}

bool NetErrorHelperCore::MaybeStartAutoReloadTimer() {
  if (!committed_error_page_info_ ||
      !committed_error_page_info_->is_finished_loading ||
      pending_error_page_info_ ||
      uncommitted_load_started_) {
    return false;
  }

  StartAutoReloadTimer();
  return true;
}

void NetErrorHelperCore::StartAutoReloadTimer() {
  DCHECK(committed_error_page_info_);
  DCHECK(IsReloadableError(*committed_error_page_info_));

  committed_error_page_info_->auto_reload_triggered = true;

  if (!online_ || (!visible_ && auto_reload_visible_only_)) {
    auto_reload_paused_ = true;
    return;
  }

  auto_reload_paused_ = false;
  base::TimeDelta delay = GetAutoReloadTime(auto_reload_count_);
  auto_reload_timer_->Stop();
  auto_reload_timer_->Start(FROM_HERE, delay,
      base::Bind(&NetErrorHelperCore::AutoReloadTimerFired,
                 base::Unretained(this)));
}

void NetErrorHelperCore::AutoReloadTimerFired() {
  auto_reload_count_++;
  Reload();
}

void NetErrorHelperCore::PauseAutoReloadTimer() {
  if (!auto_reload_timer_->IsRunning())
    return;
  DCHECK(committed_error_page_info_);
  DCHECK(!auto_reload_paused_);
  DCHECK(committed_error_page_info_->auto_reload_triggered);
  auto_reload_timer_->Stop();
  auto_reload_paused_ = true;
}

void NetErrorHelperCore::NetworkStateChanged(bool online) {
  bool was_online = online_;
  online_ = online;
  if (!was_online && online) {
    // Transitioning offline -> online
    if (auto_reload_paused_)
      MaybeStartAutoReloadTimer();
  } else if (was_online && !online) {
    // Transitioning online -> offline
    if (auto_reload_timer_->IsRunning())
      auto_reload_count_ = 0;
    PauseAutoReloadTimer();
  }
}

bool NetErrorHelperCore::ShouldSuppressErrorPage(FrameType frame_type,
                                                 const GURL& url) {
  // Don't suppress child frame errors.
  if (frame_type != MAIN_FRAME)
    return false;

  if (!auto_reload_enabled_)
    return false;

  // If there's no committed error page, this error page wasn't from an auto
  // reload.
  if (!committed_error_page_info_)
    return false;

  // If the error page wasn't reloadable, display it.
  if (!IsReloadableError(*committed_error_page_info_))
    return false;

  // If |auto_reload_timer_| is still running or is paused, this error page
  // isn't from an auto reload.
  if (auto_reload_timer_->IsRunning() || auto_reload_paused_)
    return false;

  // If the error page was reloadable, and the timer isn't running or paused, an
  // auto-reload has already been triggered.
  DCHECK(committed_error_page_info_->auto_reload_triggered);

  GURL error_url = committed_error_page_info_->error.unreachableURL;
  // TODO(ellyjones): also plumb the error code down to CCRC and check that
  if (error_url != url)
    return false;

  // Suppressed an error-page load; the previous uncommitted load was the error
  // page load starting, so forget about it.
  uncommitted_load_started_ = false;

  // The first iteration of the timer is started by OnFinishLoad calling
  // MaybeStartAutoReloadTimer, but since error pages for subsequent loads are
  // suppressed in this function, subsequent iterations of the timer have to be
  // started here.
  MaybeStartAutoReloadTimer();
  return true;
}

void NetErrorHelperCore::ExecuteButtonPress(Button button) {
  switch (button) {
    case RELOAD_BUTTON:
      chrome_common_net::RecordEvent(
          chrome_common_net::NETWORK_ERROR_PAGE_RELOAD_BUTTON_CLICKED);
      navigation_from_button_ = RELOAD_BUTTON;
      Reload();
      return;
    case LOAD_STALE_BUTTON:
      chrome_common_net::RecordEvent(
          chrome_common_net::NETWORK_ERROR_PAGE_LOAD_STALE_BUTTON_CLICKED);
      navigation_from_button_ = LOAD_STALE_BUTTON;
      delegate_->LoadPageFromCache(
          committed_error_page_info_->error.unreachableURL);
      return;
    case MORE_BUTTON:
      // Visual effects on page are handled in Javascript code.
      chrome_common_net::RecordEvent(
          chrome_common_net::NETWORK_ERROR_PAGE_MORE_BUTTON_CLICKED);
      return;
    case NO_BUTTON:
      NOTREACHED();
      return;
  }
}

void NetErrorHelperCore::TrackClick(int tracking_id) {
  // It's technically possible for |navigation_correction_params| to be NULL but
  // for |navigation_correction_response| not to be NULL, if the paramters
  // changed between loading the original error page and loading the error page
  if (!committed_error_page_info_ ||
      !committed_error_page_info_->navigation_correction_response) {
    return;
  }

  NavigationCorrectionResponse* response =
      committed_error_page_info_->navigation_correction_response.get();

  // |tracking_id| is less than 0 when the error page was not generated by the
  // navigation correction service.  |tracking_id| should never be greater than
  // the array size, but best to be safe, since it contains data from a remote
  // site, though none of that data should make it into Javascript callbacks.
  if (tracking_id < 0 ||
      static_cast<size_t>(tracking_id) >= response->corrections.size()) {
    return;
  }

  // Only report a clicked link once.
  if (committed_error_page_info_->clicked_corrections.count(tracking_id))
    return;

  committed_error_page_info_->clicked_corrections.insert(tracking_id);
  std::string request_body = CreateClickTrackingUrlRequestBody(
      committed_error_page_info_->error,
      *committed_error_page_info_->navigation_correction_params,
      *response,
      *response->corrections[tracking_id]);
  delegate_->SendTrackingRequest(
      committed_error_page_info_->navigation_correction_params->url,
      request_body);
}

