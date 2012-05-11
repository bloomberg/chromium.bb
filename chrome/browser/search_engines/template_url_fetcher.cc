// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/search_engines/template_url_fetcher.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_fetcher_callbacks.h"
#include "chrome/browser/search_engines/template_url_parser.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

// RequestDelegate ------------------------------------------------------------
class TemplateURLFetcher::RequestDelegate
    : public content::URLFetcherDelegate,
      public content::NotificationObserver {
 public:
  // Takes ownership of |callbacks|.
  RequestDelegate(TemplateURLFetcher* fetcher,
                  const string16& keyword,
                  const GURL& osdd_url,
                  const GURL& favicon_url,
                  content::WebContents* web_contents,
                  TemplateURLFetcherCallbacks* callbacks,
                  ProviderType provider_type);

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // content::URLFetcherDelegate:
  // If data contains a valid OSDD, a TemplateURL is created and added to
  // the TemplateURLService.
  virtual void OnURLFetchComplete(const net::URLFetcher* source);

  // URL of the OSDD.
  GURL url() const { return osdd_url_; }

  // Keyword to use.
  string16 keyword() const { return keyword_; }

  // The type of search provider being fetched.
  ProviderType provider_type() const { return provider_type_; }

 private:
  void AddSearchProvider();

  scoped_ptr<content::URLFetcher> url_fetcher_;
  TemplateURLFetcher* fetcher_;
  scoped_ptr<TemplateURL> template_url_;
  string16 keyword_;
  const GURL osdd_url_;
  const GURL favicon_url_;
  const ProviderType provider_type_;
  scoped_ptr<TemplateURLFetcherCallbacks> callbacks_;

  // Handles registering for our notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(RequestDelegate);
};

TemplateURLFetcher::RequestDelegate::RequestDelegate(
    TemplateURLFetcher* fetcher,
    const string16& keyword,
    const GURL& osdd_url,
    const GURL& favicon_url,
    content::WebContents* web_contents,
    TemplateURLFetcherCallbacks* callbacks,
    ProviderType provider_type)
    : ALLOW_THIS_IN_INITIALIZER_LIST(url_fetcher_(content::URLFetcher::Create(
          osdd_url, content::URLFetcher::GET, this))),
      fetcher_(fetcher),
      keyword_(keyword),
      osdd_url_(osdd_url),
      favicon_url_(favicon_url),
      provider_type_(provider_type),
      callbacks_(callbacks) {
  TemplateURLService* model = TemplateURLServiceFactory::GetForProfile(
      fetcher_->profile());
  DCHECK(model);  // TemplateURLFetcher::ScheduleDownload verifies this.

  if (!model->loaded()) {
    // Start the model load and set-up waiting for it.
    registrar_.Add(this,
                   chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
                   content::Source<TemplateURLService>(model));
    model->Load();
  }

  url_fetcher_->SetRequestContext(fetcher->profile()->GetRequestContext());
  // Can be NULL during tests.
  if (web_contents) {
    url_fetcher_->AssociateWithRenderView(
        web_contents->GetURL(),
        web_contents->GetRenderProcessHost()->GetID(),
        web_contents->GetRenderViewHost()->GetRoutingID());
  }

  url_fetcher_->Start();
}

void TemplateURLFetcher::RequestDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED);

  if (!template_url_.get())
    return;
  AddSearchProvider();
  // WARNING: AddSearchProvider deletes us.
}

void TemplateURLFetcher::RequestDelegate::OnURLFetchComplete(
    const net::URLFetcher* source) {
  // Validation checks.
  // Make sure we can still replace the keyword, i.e. the fetch was successful.
  // If the OSDD file was loaded HTTP, we also have to check the response_code.
  // For other schemes, e.g. when the OSDD file is bundled with an extension,
  // the response_code is not applicable and should be -1. Also, ensure that
  // the returned information results in a valid search URL.
  std::string data;
  if (!source->GetStatus().is_success() ||
      ((source->GetResponseCode() != -1) &&
        (source->GetResponseCode() != 200)) ||
      !source->GetResponseAsString(&data)) {
    fetcher_->RequestCompleted(this);
    // WARNING: RequestCompleted deletes us.
    return;
  }

  template_url_.reset(TemplateURLParser::Parse(fetcher_->profile(), false,
      data.data(), data.length(), NULL));
  if (!template_url_.get() || !template_url_->url_ref().SupportsReplacement()) {
    fetcher_->RequestCompleted(this);
    // WARNING: RequestCompleted deletes us.
    return;
  }

  if (provider_type_ != AUTODETECTED_PROVIDER || keyword_.empty()) {
    // Use the parser-generated new keyword from the URL in the OSDD for the
    // non-autodetected case.  The existing |keyword_| was generated from the
    // URL that hosted the OSDD, which results in the wrong keyword when the
    // OSDD was located on a third-party site that has nothing in common with
    // search engine described by OSDD.
    keyword_ = template_url_->keyword();
    DCHECK(!keyword_.empty());
  }

  // Wait for the model to be loaded before adding the provider.
  TemplateURLService* model = TemplateURLServiceFactory::GetForProfile(
      fetcher_->profile());
  if (!model->loaded())
    return;
  AddSearchProvider();
  // WARNING: AddSearchProvider deletes us.
}

void TemplateURLFetcher::RequestDelegate::AddSearchProvider() {
  DCHECK(template_url_.get());
  DCHECK(!keyword_.empty());
  Profile* profile = fetcher_->profile();
  TemplateURLService* model = TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(model);
  DCHECK(model->loaded());

  TemplateURL* existing_url = NULL;
  if (model->CanReplaceKeyword(keyword_, GURL(template_url_->url()),
                               &existing_url)) {
    if (existing_url)
      model->Remove(existing_url);
  } else if (provider_type_ == AUTODETECTED_PROVIDER) {
    fetcher_->RequestCompleted(this);  // WARNING: Deletes us!
    return;
  }

  // The short name is what is shown to the user. We preserve original names
  // since it is better when generated keyword in many cases.
  TemplateURLData data(template_url_->data());
  data.SetKeyword(keyword_);
  data.originating_url = osdd_url_;

  // The page may have specified a URL to use for favicons, if not, set it.
  if (!data.favicon_url.is_valid())
    data.favicon_url = favicon_url_;

  switch (provider_type_) {
    case AUTODETECTED_PROVIDER:
      // Mark the keyword as replaceable so it can be removed if necessary.
      data.safe_for_autoreplace = true;
      model->Add(new TemplateURL(profile, data));
      break;

    case EXPLICIT_PROVIDER:
      // Confirm addition and allow user to edit default choices. It's ironic
      // that only *non*-autodetected additions get confirmed, but the user
      // expects feedback that his action did something.
      // The source WebContents' delegate takes care of adding the URL to the
      // model, which takes ownership, or of deleting it if the add is
      // cancelled.
      callbacks_->ConfirmAddSearchProvider(new TemplateURL(profile, data),
                                           profile);
      break;

    default:
      NOTREACHED();
      break;
  }

  fetcher_->RequestCompleted(this);
  // WARNING: RequestCompleted deletes us.
}

// TemplateURLFetcher ---------------------------------------------------------

TemplateURLFetcher::TemplateURLFetcher(Profile* profile) : profile_(profile) {
  DCHECK(profile_);
}

TemplateURLFetcher::~TemplateURLFetcher() {
}

void TemplateURLFetcher::ScheduleDownload(
    const string16& keyword,
    const GURL& osdd_url,
    const GURL& favicon_url,
    content::WebContents* web_contents,
    TemplateURLFetcherCallbacks* callbacks,
    ProviderType provider_type) {
  DCHECK(osdd_url.is_valid());
  scoped_ptr<TemplateURLFetcherCallbacks> owned_callbacks(callbacks);

  TemplateURLService* url_model =
      TemplateURLServiceFactory::GetForProfile(profile());
  if (!url_model)
    return;

  // For a JS-added OSDD, the provided keyword is irrelevant because we will
  // generate a keyword later from the OSDD content.  For the autodetected case,
  // we need a valid keyword up front.
  if (provider_type == TemplateURLFetcher::AUTODETECTED_PROVIDER) {
    DCHECK(!keyword.empty());

    if (!url_model->loaded()) {
      // We could try to set up a callback to this function again once the model
      // is loaded but since this is an auto-add case anyway, meh.
      url_model->Load();
      return;
    }

    const TemplateURL* template_url =
        url_model->GetTemplateURLForKeyword(keyword);
    if (template_url && (!template_url->safe_for_autoreplace() ||
                         template_url->originating_url() == osdd_url))
      return;
  }

  // Make sure we aren't already downloading this request.
  for (Requests::iterator i = requests_->begin(); i != requests_->end(); ++i) {
    if (((*i)->url() == osdd_url) ||
        ((provider_type == TemplateURLFetcher::AUTODETECTED_PROVIDER) &&
         ((*i)->keyword() == keyword)))
      return;
  }

  requests_->push_back(
      new RequestDelegate(this, keyword, osdd_url, favicon_url, web_contents,
                          owned_callbacks.release(), provider_type));
}

void TemplateURLFetcher::RequestCompleted(RequestDelegate* request) {
  Requests::iterator i =
      std::find(requests_->begin(), requests_->end(), request);
  DCHECK(i != requests_->end());
  requests_->erase(i);
  delete request;
}
