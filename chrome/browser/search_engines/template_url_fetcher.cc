// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/search_engines/template_url_fetcher.h"

#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_parser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "net/url_request/url_request_status.h"

// RequestDelegate ------------------------------------------------------------
class TemplateURLFetcher::RequestDelegate : public URLFetcher::Delegate,
                                            public NotificationObserver {
 public:
  RequestDelegate(TemplateURLFetcher* fetcher,
                  const std::wstring& keyword,
                  const GURL& osdd_url,
                  const GURL& favicon_url,
                  TabContents* source,
                  ProviderType provider_type)
      : ALLOW_THIS_IN_INITIALIZER_LIST(url_fetcher_(osdd_url,
                                                    URLFetcher::GET, this)),
        fetcher_(fetcher),
        keyword_(keyword),
        osdd_url_(osdd_url),
        favicon_url_(favicon_url),
        provider_type_(provider_type),
        source_(source) {
    url_fetcher_.set_request_context(fetcher->profile()->GetRequestContext());
    url_fetcher_.Start();
    registrar_.Add(this,
                   NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(source_));
  }

  // URLFetcher::Delegate:
  // If data contains a valid OSDD, a TemplateURL is created and added to
  // the TemplateURLModel.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
    DCHECK(source == Source<TabContents>(source_));
    source_ = NULL;
  }

  // URL of the OSDD.
  const GURL& url() const { return osdd_url_; }

  // Keyword to use.
  const std::wstring keyword() const { return keyword_; }

  // The type of search provider being fetched.
  ProviderType provider_type() const { return provider_type_; }

 private:
  void AddSearchProvider();

  URLFetcher url_fetcher_;
  TemplateURLFetcher* fetcher_;
  scoped_ptr<TemplateURL> template_url_;
  std::wstring keyword_;
  const GURL osdd_url_;
  const GURL favicon_url_;
  const ProviderType provider_type_;

  // The TabContents where this request originated. Can be NULL if the
  // originating tab is closed. If NULL, the engine is not added.
  TabContents* source_;

  // Handles registering for our notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(RequestDelegate);
};


void TemplateURLFetcher::RequestDelegate::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
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
    std::wstring new_keyword = TemplateURLModel::GenerateKeyword(
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
    // If we're coming from JS (neither autodetected nor failure to load the
    // template URL model) and this URL already exists in the model, we bring
    // up the EditKeywordController to edit it.  This is helpful feedback in
    // the case of clicking a button twice, and annoying in the case of a
    // page that calls AddSearchProvider() in JS without a user action.
    keyword_.clear();
    existing_url = NULL;
  }

  if (existing_url)
    model->Remove(existing_url);

  // The short name is what is shown to the user. We preserve original names
  // since it is better when generated keyword in many cases.
  template_url_->set_keyword(keyword_);
  template_url_->set_originating_url(osdd_url_);

  // The page may have specified a URL to use for favicons, if not, set it.
  if (!template_url_->GetFavIconURL().is_valid())
    template_url_->SetFavIconURL(favicon_url_);

  switch (provider_type_) {
    case AUTODETECTED_PROVIDER:
      // Mark the keyword as replaceable so it can be removed if necessary.
      template_url_->set_safe_for_autoreplace(true);
      model->Add(template_url_.release());
      break;

    case EXPLICIT_PROVIDER:
      if (source_ && source_->delegate()) {
        // Confirm addition and allow user to edit default choices. It's ironic
        // that only *non*-autodetected additions get confirmed, but the user
        // expects feedback that his action did something.
        // The source TabContents' delegate takes care of adding the URL to the
        // model, which takes ownership, or of deleting it if the add is
        // cancelled.
        source_->delegate()->ConfirmAddSearchProvider(template_url_.release(),
                                                      fetcher_->profile());
      }
      break;

    case EXPLICIT_DEFAULT_PROVIDER:
      source_->delegate()->ConfirmSetDefaultSearchProvider(
          source_,
          template_url_.release(),
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

void TemplateURLFetcher::ScheduleDownload(const std::wstring& keyword,
                                          const GURL& osdd_url,
                                          const GURL& favicon_url,
                                          TabContents* source,
                                          ProviderType provider_type) {
  DCHECK(!keyword.empty() && osdd_url.is_valid());

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
      new RequestDelegate(this, keyword, osdd_url, favicon_url, source,
                          provider_type));
}

void TemplateURLFetcher::RequestCompleted(RequestDelegate* request) {
  DCHECK(find(requests_->begin(), requests_->end(), request) !=
         requests_->end());
  requests_->erase(find(requests_->begin(), requests_->end(), request));
  delete request;
}
