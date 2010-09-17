// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/search_provider_install_data.h"

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/task.h"
#include "chrome/browser/search_engines/search_host_to_urls_map.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/util.h"
#include "chrome/browser/webdata/web_data_service.h"

typedef SearchHostToURLsMap::TemplateURLSet TemplateURLSet;

namespace {

// Implementation of SearchTermsData that may be used on the I/O thread.
class IOThreadSearchTermsData : public SearchTermsData {
 public:
  IOThreadSearchTermsData() {}

  // Implementation of SearchTermsData.
  virtual std::string GoogleBaseURLValue() const;
  virtual std::string GetApplicationLocale() const;
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  virtual std::wstring GetRlzParameterValue() const {
    // This value doesn't matter for our purposes.
    return std::wstring();
  }
#endif

 private:

  DISALLOW_COPY_AND_ASSIGN(IOThreadSearchTermsData);
};

std::string IOThreadSearchTermsData::GoogleBaseURLValue() const {
  // TODO(levin): fix this.
  return "http://FIXME.FIXME/";
}

std::string IOThreadSearchTermsData::GetApplicationLocale() const {
  // This value doesn't matter for our purposes.
  return "yy";
}

// Indicates if the two inputs have the same security origin.
// |requested_origin| should only be a security origin (no path, etc.).
// It is ok if |template_url| is NULL.
static bool IsSameOrigin(const GURL& requested_origin,
                         const TemplateURL* template_url,
                         const SearchTermsData& search_terms_data) {
  DCHECK(requested_origin == requested_origin.GetOrigin());
  return template_url && requested_origin ==
      TemplateURLModel::GenerateSearchURLUsingTermsData(
          template_url,
          search_terms_data).GetOrigin();
}

}  // namespace

SearchProviderInstallData::SearchProviderInstallData(
    WebDataService* web_service)
    : web_service_(web_service),
      load_handle_(0) {
}

SearchProviderInstallData::~SearchProviderInstallData() {
  if (load_handle_) {
    DCHECK(web_service_.get());
    web_service_->CancelRequest(load_handle_);
  }
}

void SearchProviderInstallData::CallWhenLoaded(Task* task) {
  if (provider_map_.get()) {
    task->Run();
    delete task;
    return;
  }

  task_queue_.Push(task);
  if (load_handle_)
    return;

  if (web_service_.get())
    load_handle_ = web_service_->GetKeywords(this);
  else
    OnLoadFailed();
}

SearchProviderInstallData::State SearchProviderInstallData::GetInstallState(
    const GURL& requested_origin) {
  DCHECK(provider_map_.get());

  // First check to see if the origin is the default search provider.
  if (requested_origin.spec() == default_search_origin_)
    return INSTALLED_AS_DEFAULT;

  // Is the url any search provider?
  const TemplateURLSet* urls = provider_map_->GetURLsForHost(
      requested_origin.host());
  if (!urls)
    return NOT_INSTALLED;

  IOThreadSearchTermsData search_terms_data;
  for (TemplateURLSet::const_iterator i = urls->begin();
       i != urls->end(); ++i) {
    const TemplateURL* template_url = *i;
    if (IsSameOrigin(requested_origin, template_url, search_terms_data))
      return INSTALLED_BUT_NOT_DEFAULT;
  }
  return NOT_INSTALLED;
}

void SearchProviderInstallData::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult* result) {
  // Reset the load_handle so that we don't try and cancel the load in
  // the destructor.
  load_handle_ = 0;

  if (!result) {
    // Results are null if the database went away or (most likely) wasn't
    // loaded.
    OnLoadFailed();
    return;
  }

  const TemplateURL* default_search_provider = NULL;
  int new_resource_keyword_version = 0;
  std::vector<TemplateURL*> extracted_template_urls;
  GetSearchProvidersUsingKeywordResult(*result,
                                       NULL,
                                       NULL,
                                       &extracted_template_urls,
                                       &default_search_provider,
                                       &new_resource_keyword_version);
  template_urls_.get().insert(template_urls_.get().begin(),
                              extracted_template_urls.begin(),
                              extracted_template_urls.end());
  IOThreadSearchTermsData search_terms_data;
  provider_map_.reset(new SearchHostToURLsMap());
  provider_map_->Init(template_urls_.get(), search_terms_data);
  SetDefault(default_search_provider);
  NotifyLoaded();
}

void SearchProviderInstallData::SetDefault(const TemplateURL* template_url) {
  if (!template_url) {
    default_search_origin_.clear();
    return;
  }

  IOThreadSearchTermsData search_terms_data;
  const GURL url(TemplateURLModel::GenerateSearchURLUsingTermsData(
      template_url, search_terms_data));
  if (!url.is_valid() || !url.has_host()) {
    default_search_origin_.clear();
    return;
  }
  default_search_origin_ = url.GetOrigin().spec();
}

void SearchProviderInstallData::OnLoadFailed() {
  provider_map_.reset(new SearchHostToURLsMap());
  IOThreadSearchTermsData search_terms_data;
  provider_map_->Init(template_urls_.get(), search_terms_data);
  SetDefault(NULL);
  NotifyLoaded();
}

void SearchProviderInstallData::NotifyLoaded() {
  task_queue_.Run();

  // Since we expect this request to be rare, clear out the information. This
  // also keeps the responses current as the search providers change.
  provider_map_.reset();
  SetDefault(NULL);
}
