// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/new_tab_page_interceptor_service.h"

#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_redirect_job.h"
#include "url/gurl.h"

// Implementation of the URLRequestInterceptor for the New Tab Page. Will look
// at incoming response from the server and possibly divert to the local NTP.
class NewTabPageInterceptor : public net::URLRequestInterceptor {
 public:
  explicit NewTabPageInterceptor(const GURL& new_tab_url)
      : new_tab_url_(new_tab_url), weak_factory_(this) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  ~NewTabPageInterceptor() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  }

  base::WeakPtr<NewTabPageInterceptor> GetWeakPtr() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return weak_factory_.GetWeakPtr();
  }

  void SetNewTabPageURL(const GURL& new_tab_page_url) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    new_tab_url_ = new_tab_page_url;
  }

 private:
  // Overrides from net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    return nullptr;
  }

  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if ((request->url() != new_tab_url_) ||
        (new_tab_url_ == chrome::kChromeSearchLocalNtpUrl)) {
      return nullptr;
    }
    // User has canceled this navigation so it shouldn't be redirected.
    // TODO(maksims): Remove request->status() and use int net_error
    // once MaybeInterceptResponse() starts to pass that.
    if (request->status().status() == net::URLRequestStatus::CANCELED ||
        (request->status().status() == net::URLRequestStatus::FAILED &&
         request->status().error() == net::ERR_ABORTED)) {
      return nullptr;
    }

    // Request to NTP was successful.
    // TODO(maksims): Remove request->status() and use int net_error
    // once MaybeInterceptResponse() starts to pass that.
    if (request->status().is_success() &&
        request->GetResponseCode() != net::HTTP_NO_CONTENT &&
        request->GetResponseCode() < 400) {
      return nullptr;
    }

    // Failure to load the NTP correctly; redirect to Local NTP.
    UMA_HISTOGRAM_ENUMERATION("InstantExtended.CacheableNTPLoad",
                              search::CACHEABLE_NTP_LOAD_FAILED,
                              search::CACHEABLE_NTP_LOAD_MAX);
    return new net::URLRequestRedirectJob(
        request, network_delegate, GURL(chrome::kChromeSearchLocalNtpUrl),
        net::URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT,
        "NTP Request Interceptor");
  }

  GURL new_tab_url_;
  base::WeakPtrFactory<NewTabPageInterceptor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NewTabPageInterceptor);
};

NewTabPageInterceptorService::NewTabPageInterceptorService(Profile* profile)
    : profile_(profile),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile)) {
  DCHECK(profile_);
  if (template_url_service_)
    template_url_service_->AddObserver(this);
}

NewTabPageInterceptorService::~NewTabPageInterceptorService() {
  if (template_url_service_)
    template_url_service_->RemoveObserver(this);
}

void NewTabPageInterceptorService::OnTemplateURLServiceChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL new_tab_page_url(search::GetNewTabPageURL(profile_));
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&NewTabPageInterceptor::SetNewTabPageURL, interceptor_,
                 new_tab_page_url));
}

std::unique_ptr<net::URLRequestInterceptor>
NewTabPageInterceptorService::CreateInterceptor() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<NewTabPageInterceptor> interceptor(
      new NewTabPageInterceptor(search::GetNewTabPageURL(profile_)));
  interceptor_ = interceptor->GetWeakPtr();
  return std::move(interceptor);
}
