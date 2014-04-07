// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper_core.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/common/localized_error.h"
#include "grit/generated_resources.h"
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

// If URL correction information should be retrieved remotely for a main frame
// load that failed with |error|, returns true and sets
// |correction_request_body| to be the body for the correction request.
bool GetFixUrlRequestBody(const blink::WebURLError& error,
                          const std::string& language,
                          const std::string& country_code,
                          const std::string& api_key,
                          std::string* correction_request_body) {
  // Parameter to send to the correction service indicating the error type.
  std::string error_param;

  std::string domain = error.domain.utf8();
  if (domain == "http" && error.reason == 404) {
    error_param = "http404";
  } else if (IsDnsError(error)) {
    error_param = "dnserror";
  } else if (domain == net::kErrorDomain &&
             (error.reason == net::ERR_CONNECTION_FAILED ||
              error.reason == net::ERR_CONNECTION_REFUSED ||
              error.reason == net::ERR_ADDRESS_UNREACHABLE ||
              error.reason == net::ERR_CONNECTION_TIMED_OUT)) {
    error_param = "connectionFailure";
  } else {
    return false;
  }

  // Don't use the correction service for HTTPS (for privacy reasons).
  GURL unreachable_url(error.unreachableURL);
  if (unreachable_url.SchemeIsSecure())
    return false;

  // TODO(yuusuke): Change to net::FormatUrl when Link Doctor becomes
  // unicode-capable.
  std::string spec_to_send = SanitizeURL(unreachable_url).spec();

  // Notify navigation correction service of the url truncation by sending of
  // "?" at the end.
  if (unreachable_url.has_query())
    spec_to_send.append("?");

  // Assemble request body, which is a JSON string.
  // TODO(mmenke):  Investigate open sourcing the relevant protocol buffers and
  //                using those directly instead.

  base::DictionaryValue request_dict;
  request_dict.SetString("method", "linkdoctor.fixurl.fixurl");
  request_dict.SetString("apiVersion", "v1");

  base::DictionaryValue* params_dict = new base::DictionaryValue();
  request_dict.Set("params", params_dict);

  params_dict->SetString("key", api_key);
  params_dict->SetString("urlQuery", spec_to_send);
  params_dict->SetString("clientName", "chrome");
  params_dict->SetString("error", error_param);

  if (!language.empty())
    params_dict->SetString("language", language);

  if (!country_code.empty())
    params_dict->SetString("originCountry", country_code);

  base::JSONWriter::Write(&request_dict, correction_request_body);
  return true;
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

LocalizedError::ErrorPageParams* ParseAdditionalSuggestions(
    const std::string& data,
    const GURL& original_url,
    const GURL& search_url,
    const std::string& accept_languages,
    bool is_rtl) {
  scoped_ptr<base::Value> parsed(base::JSONReader::Read(data));
  if (!parsed)
    return NULL;
  // TODO(mmenke):  Open source related protocol buffers and use them directly.
  base::DictionaryValue* parsed_dict;
  base::ListValue* corrections;
  if (!parsed->GetAsDictionary(&parsed_dict))
    return NULL;
  if (!parsed_dict->GetList("result.UrlCorrections", &corrections))
    return NULL;

  // Version of URL for display in suggestions.  It has to be sanitized first
  // because any received suggestions will be relative to the sanitized URL.
  base::string16 original_url_for_display =
      FormatURLForDisplay(SanitizeURL(original_url), is_rtl, accept_languages);

  scoped_ptr<LocalizedError::ErrorPageParams> params(
      new LocalizedError::ErrorPageParams());
  params->override_suggestions.reset(new base::ListValue());
  scoped_ptr<base::ListValue> parsed_corrections(new base::ListValue());
  for (base::ListValue::iterator it = corrections->begin();
       it != corrections->end(); ++it) {
    base::DictionaryValue* correction;
    if (!(*it)->GetAsDictionary(&correction))
      continue;

    // Doesn't seem like a good idea to show these.
    bool is_porn;
    if (correction->GetBoolean("isPorn", &is_porn) && is_porn)
      continue;
    if (correction->GetBoolean("isSoftPorn", &is_porn) && is_porn)
      continue;

    std::string correction_type;
    std::string url_correction;
    if (!correction->GetString("correctionType", &correction_type) ||
        !correction->GetString("urlCorrection", &url_correction)) {
      continue;
    }

    std::string click_tracking_url;
    correction->GetString("clickTrackingUrl", &click_tracking_url);

    if (correction_type == "reloadPage") {
      params->suggest_reload = true;
      continue;
    }

    if (correction_type == "webSearchQuery") {
      // If there are mutliple searches suggested, use the first suggestion.
      if (params->search_terms.empty()) {
        params->search_url = search_url;
        params->search_terms = url_correction;
      }
      continue;
    }

    size_t correction_index;
    for (correction_index = 0;
         correction_index < arraysize(kCorrectionResourceTable);
         ++correction_index) {
      if (correction_type !=
              kCorrectionResourceTable[correction_index].correction_type) {
        continue;
      }
      base::DictionaryValue* suggest = new base::DictionaryValue();
      suggest->SetString("header",
          l10n_util::GetStringUTF16(
              kCorrectionResourceTable[correction_index].resource_id));
      suggest->SetString("urlCorrection",
          !click_tracking_url.empty() ? click_tracking_url :
                                        url_correction);
      suggest->SetString(
          "urlCorrectionForDisplay",
          FormatURLForDisplay(GURL(url_correction), is_rtl, accept_languages));
      suggest->SetString("originalUrlForDisplay", original_url_for_display);
      params->override_suggestions->Append(suggest);
      break;
    }
  }

  if (params->override_suggestions->empty() &&
      !params->search_url.is_valid()) {
    return NULL;
  }
  return params.release();
}

}  // namespace

struct NetErrorHelperCore::ErrorPageInfo {
  ErrorPageInfo(blink::WebURLError error, bool was_failed_post)
      : error(error),
        was_failed_post(was_failed_post),
        needs_dns_updates(false),
        is_finished_loading(false) {
  }

  // Information about the failed page load.
  blink::WebURLError error;
  bool was_failed_post;

  // Information about the status of the error page.

  // True if a page is a DNS error page and has not yet received a final DNS
  // probe status.
  bool needs_dns_updates;

  // Navigation correction service url, which will be used in response to
  // certain types of network errors.  This is also stored by the
  // NetErrorHelperCore itself, but it stored here as well in case its modified
  // in the middle of an error page load.  Empty when no error page should be
  // fetched, or if there's already a fetch in progress.
  GURL navigation_correction_url;

  // Request body to use when requesting corrections from a web service.
  // TODO(mmenke):  Investigate loading the error page at the same time as
  //                the blank page is loading, to get rid of these.
  std::string navigation_correction_request_body;

  // True if a page has completed loading, at which point it can receive
  // updates.
  bool is_finished_loading;
};

bool NetErrorHelperCore::IsReloadableError(
    const NetErrorHelperCore::ErrorPageInfo& info) {
  return info.error.domain.utf8() == net::kErrorDomain &&
         info.error.reason != net::ERR_ABORTED &&
         !info.was_failed_post;
}

NetErrorHelperCore::NetErrorHelperCore(Delegate* delegate)
    : delegate_(delegate),
      last_probe_status_(chrome_common_net::DNS_PROBE_POSSIBLE),
      auto_reload_enabled_(false),
      auto_reload_timer_(new base::Timer(false, false)),
      // TODO(ellyjones): Make online_ accurate at object creation.
      online_(true),
      auto_reload_count_(0),
      can_auto_reload_page_(false) {
}

NetErrorHelperCore::~NetErrorHelperCore() {
  if (committed_error_page_info_ && can_auto_reload_page_) {
    UMA_HISTOGRAM_CUSTOM_ENUMERATION("Net.AutoReload.ErrorAtStop",
                                     -committed_error_page_info_->error.reason,
                                     net::GetAllErrorCodesForUma());
    UMA_HISTOGRAM_COUNTS("Net.AutoReload.CountAtStop", auto_reload_count_);
  }
}

void NetErrorHelperCore::CancelPendingFetches() {
  // Cancel loading the alternate error page, and prevent any pending error page
  // load from starting a new error page load.  Swapping in the error page when
  // it's finished loading could abort the navigation, otherwise.
  if (committed_error_page_info_ && can_auto_reload_page_) {
    UMA_HISTOGRAM_CUSTOM_ENUMERATION("Net.AutoReload.ErrorAtStop",
                                     -committed_error_page_info_->error.reason,
                                     net::GetAllErrorCodesForUma());
    UMA_HISTOGRAM_COUNTS("Net.AutoReload.CountAtStop", auto_reload_count_);
  }
  if (committed_error_page_info_) {
    committed_error_page_info_->navigation_correction_url = GURL();
    committed_error_page_info_->navigation_correction_request_body.clear();
  }
  if (pending_error_page_info_) {
    pending_error_page_info_->navigation_correction_url = GURL();
    pending_error_page_info_->navigation_correction_request_body.clear();
  }
  delegate_->CancelFetchNavigationCorrections();
  auto_reload_timer_->Stop();
  can_auto_reload_page_ = false;
}

void NetErrorHelperCore::OnStop() {
  CancelPendingFetches();
  auto_reload_count_ = 0;
}

void NetErrorHelperCore::OnStartLoad(FrameType frame_type, PageType page_type) {
  if (frame_type != MAIN_FRAME)
    return;

  // If there's no pending error page information associated with the page load,
  // or the new page is not an error page, then reset pending error page state.
  if (!pending_error_page_info_ || page_type != ERROR_PAGE) {
    CancelPendingFetches();
  } else if (auto_reload_enabled_) {
    // If an error load is starting, the resulting error page is autoreloadable.
    can_auto_reload_page_ = IsReloadableError(*pending_error_page_info_);
  }
}

void NetErrorHelperCore::OnCommitLoad(FrameType frame_type) {
  if (frame_type != MAIN_FRAME)
    return;

  if (committed_error_page_info_ && !pending_error_page_info_ &&
      can_auto_reload_page_) {
    int reason = committed_error_page_info_->error.reason;
    UMA_HISTOGRAM_CUSTOM_ENUMERATION("Net.AutoReload.ErrorAtSuccess",
                                     -reason,
                                     net::GetAllErrorCodesForUma());
    UMA_HISTOGRAM_COUNTS("Net.AutoReload.CountAtSuccess", auto_reload_count_);
    if (auto_reload_count_ == 1) {
      UMA_HISTOGRAM_CUSTOM_ENUMERATION("Net.AutoReload.ErrorAtFirstSuccess",
                                       -reason,
                                       net::GetAllErrorCodesForUma());
    }
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

  // Only enable stale cache JS bindings if this wasn't a post.
  if (!committed_error_page_info_->was_failed_post) {
    delegate_->EnableStaleLoadBindings(
        committed_error_page_info_->error.unreachableURL);
  }

  if (committed_error_page_info_->navigation_correction_url.is_valid()) {
    // If there is another pending error page load, |fix_url| should have been
    // cleared.
    DCHECK(!pending_error_page_info_);
    DCHECK(!committed_error_page_info_->needs_dns_updates);
    delegate_->FetchNavigationCorrections(
        committed_error_page_info_->navigation_correction_url,
        committed_error_page_info_->navigation_correction_request_body);
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
           !committed_error_page_info_->navigation_correction_url.is_valid());

    std::string navigation_correction_request_body;

    if (navigation_correction_url_.is_valid() &&
        GetFixUrlRequestBody(error, language_, country_code_, api_key_,
                             &navigation_correction_request_body)) {
      pending_error_page_info_.reset(new ErrorPageInfo(error, is_failed_post));
      pending_error_page_info_->navigation_correction_url =
          navigation_correction_url_;
      pending_error_page_info_->navigation_correction_request_body =
          navigation_correction_request_body;
      return;
    }

    // The last probe status needs to be reset if this is a DNS error.  This
    // means that if a DNS error page is committed but has not yet finished
    // loading, a DNS probe status scheduled to be sent to it may be thrown
    // out, but since the new error page should trigger a new DNS probe, it
    // will just get the results for the next page load.
    if (IsDnsError(error))
      last_probe_status_ = chrome_common_net::DNS_PROBE_POSSIBLE;
  }

  GenerateLocalErrorPage(frame_type, error, is_failed_post,
                         scoped_ptr<LocalizedError::ErrorPageParams>(),
                         error_html);
}

void NetErrorHelperCore::GenerateLocalErrorPage(
    FrameType frame_type,
    const blink::WebURLError& error,
    bool is_failed_post,
    scoped_ptr<LocalizedError::ErrorPageParams> params,
    std::string* error_html) {
  if (frame_type == MAIN_FRAME) {
    pending_error_page_info_.reset(new ErrorPageInfo(error, is_failed_post));
    // Skip DNS logic if suggestions were received from a remote server.
    if (IsDnsError(error) && !params) {
      // This is not strictly necessary, but waiting for a new status to be
      // sent as a result of the DidFinishLoading call keeps the histograms
      // consistent with older versions of the code, at no real cost.
      last_probe_status_ = chrome_common_net::DNS_PROBE_POSSIBLE;

      delegate_->GenerateLocalizedErrorPage(
          GetUpdatedError(error), is_failed_post, params.Pass(),
          error_html);
      pending_error_page_info_->needs_dns_updates = true;
      return;
    }
  }

  delegate_->GenerateLocalizedErrorPage(error, is_failed_post,
                                        params.Pass(), error_html);
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
  navigation_correction_url_ = navigation_correction_url;
  language_ = language;
  country_code_ = country_code;
  api_key_ = api_key;
  search_url_ = search_url;
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

  scoped_ptr<LocalizedError::ErrorPageParams> params(
    ParseAdditionalSuggestions(
        corrections, GURL(committed_error_page_info_->error.unreachableURL),
        search_url_, accept_languages, is_rtl));
  std::string error_html;
  GenerateLocalErrorPage(MAIN_FRAME,
                         committed_error_page_info_->error,
                         committed_error_page_info_->was_failed_post,
                         params.Pass(),
                         &error_html);

  // |error_page_info| may have been destroyed by this point, since
  // |pending_error_page_info_| was set to a new ErrorPageInfo.

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
      !can_auto_reload_page_ ||
      pending_error_page_info_) {
    return false;
  }

  DCHECK(IsReloadableError(*committed_error_page_info_));

  if (!online_)
    return false;

  StartAutoReloadTimer();
  return true;
}

void NetErrorHelperCore::StartAutoReloadTimer() {
  DCHECK(committed_error_page_info_);
  DCHECK(can_auto_reload_page_);
  base::TimeDelta delay = GetAutoReloadTime(auto_reload_count_);
  auto_reload_count_++;
  auto_reload_timer_->Stop();
  auto_reload_timer_->Start(FROM_HERE, delay,
      base::Bind(&NetErrorHelperCore::Reload,
                 base::Unretained(this)));
}

void NetErrorHelperCore::NetworkStateChanged(bool online) {
  online_ = online;
  if (auto_reload_timer_->IsRunning()) {
    DCHECK(committed_error_page_info_);
    // If there's an existing timer running, stop it and reset the retry count.
    auto_reload_timer_->Stop();
    auto_reload_count_ = 0;
  }

  // If the network state changed to online, maybe start auto-reloading again.
  if (online)
    MaybeStartAutoReloadTimer();
}

bool NetErrorHelperCore::ShouldSuppressErrorPage(FrameType frame_type,
                                                 const GURL& url) {
  // Don't suppress child frame errors.
  if (frame_type != MAIN_FRAME)
    return false;

  // If |auto_reload_timer_| is still running, this error page isn't from an
  // auto reload.
  if (auto_reload_timer_->IsRunning())
    return false;

  // If there's no committed error page, this error page wasn't from an auto
  // reload.
  if (!committed_error_page_info_ || !can_auto_reload_page_)
    return false;

  GURL error_url = committed_error_page_info_->error.unreachableURL;
  // TODO(ellyjones): also plumb the error code down to CCRC and check that
  if (error_url != url)
    return false;

  // The first iteration of the timer is started by OnFinishLoad calling
  // MaybeStartAutoReloadTimer, but since error pages for subsequent loads are
  // suppressed in this function, subsequent iterations of the timer have to be
  // started here.
  MaybeStartAutoReloadTimer();
  return true;
}
