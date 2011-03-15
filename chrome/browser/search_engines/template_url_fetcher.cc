// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/search_engines/template_url_fetcher.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_fetcher_callbacks.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_parser.h"
#include "chrome/common/net/url_fetcher.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "net/url_request/url_request_status.h"

// RequestDelegate ------------------------------------------------------------
class TemplateURLFetcher::RequestDelegate : public URLFetcher::Delegate,
                                            public NotificationObserver {
 public:
  // Takes ownership of |callbacks|.
  RequestDelegate(TemplateURLFetcher* fetcher,
                  const string16& keyword,
                  const GURL& osdd_url,
                  const GURL& favicon_url,
                  TemplateURLFetcherCallbacks* callbacks,
                  ProviderType provider_type);

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // URLFetcher::Delegate:
  // If data contains a valid OSDD, a TemplateURL is created and added to
  // the TemplateURLModel.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // URL of the OSDD.
  GURL url() const { return osdd_url_; }

  // Keyword to use.
  string16 keyword() const { return keyword_; }

  // The type of search provider being fetched.
  ProviderType provider_type() const { return provider_type_; }

 private:
  void AddSearchProvider();

  URLFetcher url_fetcher_;
  TemplateURLFetcher* fetcher_;
  scoped_ptr<TemplateURL> template_url_;
  string16 keyword_;
  const GURL osdd_url_;
  const GURL favicon_url_;
  const ProviderType provider_type_;
  scoped_ptr<TemplateURLFetcherCallbacks> callbacks_;

  // Handles registering for our notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(RequestDelegate);
};

TemplateURLFetcher::RequestDelegate::RequestDelegate(
    TemplateURLFetcher* fetcher,
    const string16& keyword,
    const GURL& osdd_url,
    const GURL& favicon_url,
    TemplateURLFetcherCallbacks* callbacks,
    ProviderType provider_type)
    : ALLOW_THIS_IN_INITIALIZER_LIST(url_fetcher_(osdd_url,
                                                  URLFetcher::GET, this)),
      fetcher_(fetcher),
      keyword_(keyword),
      osdd_url_(osdd_url),
      favicon_url_(favicon_url),
      provider_type_(provider_type),
      callbacks_(callbacks) {
  TemplateURLModel* model = fetcher_->profile()->GetTemplateURLModel();
  DCHECK(model);  // TemplateURLFetcher::ScheduleDownload verifies this.

  if (!model->loaded()) {
    // Start the model load and set-up waiting for it.
    registrar_.Add(this,
                   NotificationType::TEMPLATE_URL_MODEL_LOADED,
                   Source<TemplateURLModel>(model));
    model->Load();
  }

  url_fetcher_.set_request_context(fetcher->profile()->GetRequestContext());
  url_fetcher_.Start();
}

void TemplateURLFetcher::RequestDelegate::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NotificationType::TEMPLATE_URL_MODEL_LOADED);

  if (!template_url_.get())
    return;
  AddSearchProvider();
  // WARNING: AddSearchProvider deletes us.
}

void TemplateURLFetcher::RequestDelegate::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  template_url_.reset(new TemplateURL());

  // Validation checks.
  // Make sure we can still replace the keyword, i.e. the fetch was successful.
  // If the OSDD file was loaded HTTP, we also have to check the response_code.
  // For other schemes, e.g. when the OSDD file is bundled with an extension,
  // the response_code is not applicable and should be -1. Also, ensure that
  // the returned information results in a valid search URL.
  if (!status.is_success() ||
      ((response_code != -1) && (response_code != 200)) ||
      !TemplateURLParser::Parse(
          reinterpret_cast<const unsigned char*>(data.c_str()),
          data.length(),
          NULL,
          template_url_.get()) ||
      !template_url_->url() || !template_url_->url()->SupportsReplacement()) {
    fetcher_->RequestCompleted(this);
    // WARNING: RequestCompleted deletes us.
    return;
  }

  // Wait for the model to be loaded before adding the provider.
  TemplateURLModel* model = fetcher_->profile()->GetTemplateURLModel();
  if (!model->loaded())
    return;
  AddSearchProvider();
  // WARNING: AddSearchProvider deletes us.
}

void TemplateURLFetcher::RequestDelegate::AddSearchProvider() {
  DCHECK(template_url_.get());
  if (provider_type_ != AUTODETECTED_PROVIDER || keyword_.empty()) {
    // Generate new keyword from URL in OSDD for none autodetected case.
    // Previous keyword was generated from URL where OSDD was placed and
    // it gives wrong result when OSDD is located on third party site that
    // has nothing in common with search engine in OSDD.
    GURL keyword_url(template_url_->url()->url());
    string16 new_keyword = TemplateURLModel::GenerateKeyword(
        keyword_url, false);
    if (!new_keyword.empty())
      keyword_ = new_keyword;
  }
  TemplateURLModel* model = fetcher_->profile()->GetTemplateURLModel();
  const TemplateURL* existing_url;
  if (keyword_.empty() ||
      !model || !model->loaded() ||
      !model->CanReplaceKeyword(keyword_, GURL(template_url_->url()->url()),
                                &existing_url)) {
    if (provider_type_ == AUTODETECTED_PROVIDER || !model || !model->loaded()) {
      fetcher_->RequestCompleted(this);
      // WARNING: RequestCompleted deletes us.
      return;
    }

    existing_url = NULL;

    // Try to generate a keyword automatically when we are setting the default
    // provider. The keyword isn't as important in this case.
    if (provider_type_ == EXPLICIT_DEFAULT_PROVIDER) {
      // The loop numbers are arbitrary and are simply a strong effort.
      string16 new_keyword;
      for (int i = 0; i < 100; ++i) {
        // Concatenate a number at end of the keyword and try that.
        new_keyword = keyword_;
        // Try the keyword alone the first time
        if (i > 0)
          new_keyword.append(base::IntToString16(i));
        if (!model->GetTemplateURLForKeyword(new_keyword) ||
            model->CanReplaceKeyword(new_keyword,
                                     GURL(template_url_->url()->url()),
                                     &existing_url)) {
          break;
        }
        new_keyword.clear();
        existing_url = NULL;
      }

      if (new_keyword.empty()) {
        // A keyword could not be found. This user must have a lot of numerical
        // keywords built up.
        fetcher_->RequestCompleted(this);
        // WARNING: RequestCompleted deletes us.
        return;
      }
      keyword_ = new_keyword;
    } else {
      // If we're coming from JS (neither autodetected nor failure to load the
      // template URL model) and this URL already exists in the model, we bring
      // up the EditKeywordController to edit it.  This is helpful feedback in
      // the case of clicking a button twice, and annoying in the case of a
      // page that calls AddSearchProvider() in JS without a user action.
      keyword_.clear();
    }
  }

  if (existing_url)
    model->Remove(existing_url);

  // The short name is what is shown to the user. We preserve original names
  // since it is better when generated keyword in many cases.
  template_url_->set_keyword(keyword_);
  template_url_->set_originating_url(osdd_url_);

  // The page may have specified a URL to use for favicons, if not, set it.
  if (!template_url_->GetFavIconURL().is_valid())
    template_url_->SetFaviconURL(favicon_url_);

  switch (provider_type_) {
    case AUTODETECTED_PROVIDER:
      // Mark the keyword as replaceable so it can be removed if necessary.
      template_url_->set_safe_for_autoreplace(true);
      model->Add(template_url_.release());
      break;

    case EXPLICIT_PROVIDER:
      // Confirm addition and allow user to edit default choices. It's ironic
      // that only *non*-autodetected additions get confirmed, but the user
      // expects feedback that his action did something.
      // The source TabContents' delegate takes care of adding the URL to the
      // model, which takes ownership, or of deleting it if the add is
      // cancelled.
      callbacks_->ConfirmAddSearchProvider(template_url_.release(),
                                           fetcher_->profile());
      break;

    case EXPLICIT_DEFAULT_PROVIDER:
      callbacks_->ConfirmSetDefaultSearchProvider(template_url_.release(),
                                                  model);
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
    TemplateURLFetcherCallbacks* callbacks,
    ProviderType provider_type) {
  DCHECK(osdd_url.is_valid());
  scoped_ptr<TemplateURLFetcherCallbacks> owned_callbacks(callbacks);

  // For JS added OSDD empty keyword is OK because we will generate keyword
  // later from OSDD content.
  if (provider_type == TemplateURLFetcher::AUTODETECTED_PROVIDER &&
      keyword.empty())
    return;
  TemplateURLModel* url_model = profile()->GetTemplateURLModel();
  if (!url_model)
    return;

  // Avoid certain checks for the default provider because we'll do the load
  // and try to brute force a unique keyword for it.
  if (provider_type != TemplateURLFetcher::EXPLICIT_DEFAULT_PROVIDER) {
    if (!url_model->loaded()) {
      url_model->Load();
      return;
    }
    const TemplateURL* template_url =
        url_model->GetTemplateURLForKeyword(keyword);
    if (template_url && (!template_url->safe_for_autoreplace() ||
                         template_url->originating_url() == osdd_url)) {
      // Either there is a user created TemplateURL for this keyword, or the
      // keyword has the same OSDD url and we've parsed it.
      return;
    }
  }

  // Make sure we aren't already downloading this request.
  for (std::vector<RequestDelegate*>::iterator i = requests_->begin();
       i != requests_->end(); ++i) {
    bool keyword_or_osdd_match = (*i)->url() == osdd_url ||
        (*i)->keyword() == keyword;
    bool same_type_or_neither_is_default =
        (*i)->provider_type() == provider_type ||
        ((*i)->provider_type() != EXPLICIT_DEFAULT_PROVIDER &&
         provider_type != EXPLICIT_DEFAULT_PROVIDER);
    if (keyword_or_osdd_match && same_type_or_neither_is_default)
      return;
  }

  requests_->push_back(
      new RequestDelegate(this, keyword, osdd_url, favicon_url,
                          owned_callbacks.release(), provider_type));
}

void TemplateURLFetcher::RequestCompleted(RequestDelegate* request) {
  DCHECK(find(requests_->begin(), requests_->end(), request) !=
         requests_->end());
  requests_->erase(find(requests_->begin(), requests_->end(), request));
  delete request;
}
