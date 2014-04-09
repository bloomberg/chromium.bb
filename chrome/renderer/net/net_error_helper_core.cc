// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper_core.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "chrome/common/localized_error.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "url/gurl.h"

namespace {

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

// If an alternate error page should be retrieved remotely for a main frame load
// that failed with |error|, returns true and sets |error_page_url| to the URL
// of the remote error page.
bool GetErrorPageURL(const blink::WebURLError& error,
                     const GURL& alt_error_page_url,
                     GURL* error_page_url) {
  if (!alt_error_page_url.is_valid())
    return false;

  // Parameter to send to the error page indicating the error type.
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
    error_param = "connectionfailure";
  } else {
    return false;
  }

  // Don't use the Link Doctor for HTTPS (for privacy reasons).
  GURL unreachable_url(error.unreachableURL);
  if (unreachable_url.SchemeIsSecure())
    return false;

  // Sanitize the unreachable URL.
  GURL::Replacements remove_params;
  remove_params.ClearUsername();
  remove_params.ClearPassword();
  remove_params.ClearQuery();
  remove_params.ClearRef();
  // TODO(yuusuke): change to net::FormatUrl when Link Doctor becomes
  // unicode-capable.
  std::string spec_to_send =
      unreachable_url.ReplaceComponents(remove_params).spec();

  // Notify Link Doctor of the url truncation by sending of "?" at the end.
  if (unreachable_url.has_query())
    spec_to_send.append("?");

  std::string params(alt_error_page_url.query());
  params.append("&url=");
  params.append(net::EscapeQueryParamValue(spec_to_send, true));
  params.append("&sourceid=chrome");
  params.append("&error=");
  params.append(error_param);

  // Build the final url to request.
  GURL::Replacements link_doctor_params;
  link_doctor_params.SetQueryStr(params);
  *error_page_url = alt_error_page_url.ReplaceComponents(link_doctor_params);
  return true;
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

  // URL of an alternate error page to repace this error page with, if it's a
  // valid URL.  Request will be issued when the error page finishes loading.
  // This is done on load complete to ensure that there are two complete loads
  // for tests to wait for.
  GURL alternate_error_page_url;

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
  if (committed_error_page_info_)
    committed_error_page_info_->alternate_error_page_url = GURL();
  if (pending_error_page_info_)
    pending_error_page_info_->alternate_error_page_url = GURL();
  delegate_->CancelFetchErrorPage();
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

  if (committed_error_page_info_->alternate_error_page_url.is_valid()) {
    // If there is another pending error page load,
    // |replace_with_alternate_error_page| should have been set to false.
    DCHECK(!pending_error_page_info_);
    DCHECK(!committed_error_page_info_->needs_dns_updates);
    GURL error_page_url;
    delegate_->FetchErrorPage(
        committed_error_page_info_->alternate_error_page_url);
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
    // If an alternate error page was going to be fetched, that should have been
    // cancelled by loading a new page load (Which has now failed to load).
    DCHECK(!committed_error_page_info_ ||
           !committed_error_page_info_->alternate_error_page_url.is_valid());

    // The last probe status needs to be reset if this is a DNS error.  This
    // means that if a DNS error page is committed but has not yet finished
    // loading, a DNS probe status scheduled to be sent to it may be thrown
    // out, but since the new error page should trigger a new DNS probe, it
    // will just get the results for the next page load.
    if (IsDnsError(error))
      last_probe_status_ = chrome_common_net::DNS_PROBE_POSSIBLE;

    GURL error_page_url;
    if (GetErrorPageURL(error, alt_error_page_url_, &error_page_url)) {
      pending_error_page_info_.reset(new ErrorPageInfo(error, is_failed_post));
      pending_error_page_info_->alternate_error_page_url = error_page_url;
      return;
    }
  }

  GenerateLocalErrorPage(frame_type, error, is_failed_post, error_html);
}

void NetErrorHelperCore::GenerateLocalErrorPage(
    FrameType frame_type,
    const blink::WebURLError& error,
    bool is_failed_post,
    std::string* error_html) {
  if (frame_type == MAIN_FRAME) {
    pending_error_page_info_.reset(new ErrorPageInfo(error, is_failed_post));
    if (IsDnsError(error)) {
      // This is not strictly necessary, but waiting for a new status to be
      // sent as a result of the DidFinishLoading call keeps the histograms
      // consistent with older versions of the code, at no real cost.
      last_probe_status_ = chrome_common_net::DNS_PROBE_POSSIBLE;

      delegate_->GenerateLocalizedErrorPage(
          GetUpdatedError(error), is_failed_post, error_html);
      pending_error_page_info_->needs_dns_updates = true;
      return;
    }
  }
  delegate_->GenerateLocalizedErrorPage(error, is_failed_post, error_html);
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

void NetErrorHelperCore::OnAlternateErrorPageFetched(const std::string& data) {
  // Alternate error page load only starts when an error page finishes loading,
  // and is cancelled with a new load
  DCHECK(!pending_error_page_info_);
  DCHECK(committed_error_page_info_->is_finished_loading);

  const std::string* error_html = NULL;
  std::string generated_html;
  if (!data.empty()) {
    // If the request succeeded, use the response in place of a generated error
    // page.
    pending_error_page_info_.reset(
        new ErrorPageInfo(committed_error_page_info_->error,
                          committed_error_page_info_->was_failed_post));
    error_html = &data;
  } else {
    // Otherwise, generate a local error page.  |pending_error_page_info_| will
    // be set by GenerateLocalErrorPage.
    GenerateLocalErrorPage(MAIN_FRAME,
                           committed_error_page_info_->error,
                           committed_error_page_info_->was_failed_post,
                           &generated_html);
    error_html = &generated_html;
  }

  // |error_page_info| may have been destroyed by this point, since
  // |pending_error_page_info_| was set to a new ErrorPageInfo.

  // TODO(mmenke):  Once the new API is in place, look into replacing this
  //                double page load by just updating the error page, like DNS
  //                probes do.
  delegate_->LoadErrorPageInMainFrame(
      *error_html,
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
