// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/search_provider_install_data.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/search_engines/search_host_to_urls_map.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"

using content::BrowserThread;

typedef SearchHostToURLsMap::TemplateURLSet TemplateURLSet;

namespace {

void LoadDataOnUIThread(TemplateURLService* template_url_service,
                        const base::Callback<void(ScopedVector<TemplateURL>,
                                                  TemplateURL*)>& callback) {
  ScopedVector<TemplateURL> template_url_copies;
  TemplateURL* default_provider_copy = NULL;
  TemplateURLService::TemplateURLVector original_template_urls =
      template_url_service->GetTemplateURLs();
  TemplateURL* original_default_provider =
      template_url_service->GetDefaultSearchProvider();
  for (TemplateURLService::TemplateURLVector::const_iterator it =
           original_template_urls.begin();
       it != original_template_urls.end();
       ++it) {
    template_url_copies.push_back(new TemplateURL((*it)->data()));
    if (*it == original_default_provider)
      default_provider_copy = template_url_copies.back();
  }
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(callback,
                                     base::Passed(template_url_copies.Pass()),
                                     base::Unretained(default_provider_copy)));
}

// Implementation of SearchTermsData that may be used on the I/O thread.
class IOThreadSearchTermsData : public SearchTermsData {
 public:
  explicit IOThreadSearchTermsData(const std::string& google_base_url);

  // Implementation of SearchTermsData.
  virtual std::string GoogleBaseURLValue() const OVERRIDE;

 private:
  std::string google_base_url_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadSearchTermsData);
};

IOThreadSearchTermsData::IOThreadSearchTermsData(
    const std::string& google_base_url) : google_base_url_(google_base_url) {
}

std::string IOThreadSearchTermsData::GoogleBaseURLValue() const {
  return google_base_url_;
}

// Handles telling SearchProviderInstallData about changes to the google base
// url. (Ensure that this is deleted on the I/O thread so that the WeakPtr is
// deleted on the correct thread.)
class GoogleURLChangeNotifier
    : public base::RefCountedThreadSafe<GoogleURLChangeNotifier,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  explicit GoogleURLChangeNotifier(
      const base::WeakPtr<SearchProviderInstallData>& install_data);

  // Called on the I/O thread with the Google base URL whenever the value
  // changes.
  void OnChange(const std::string& google_base_url);

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<GoogleURLChangeNotifier>;

  ~GoogleURLChangeNotifier() {}

  base::WeakPtr<SearchProviderInstallData> install_data_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLChangeNotifier);
};

GoogleURLChangeNotifier::GoogleURLChangeNotifier(
    const base::WeakPtr<SearchProviderInstallData>& install_data)
    : install_data_(install_data) {
}

void GoogleURLChangeNotifier::OnChange(const std::string& google_base_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (install_data_.get())
    install_data_->OnGoogleURLChange(google_base_url);
}

// Notices changes in the Google base URL and sends them along
// to the SearchProviderInstallData on the I/O thread.
class GoogleURLObserver : public content::RenderProcessHostObserver {
 public:
  GoogleURLObserver(GoogleURLTracker* google_url_tracker,
                    GoogleURLChangeNotifier* change_notifier,
                    content::RenderProcessHost* host);

  // Implementation of content::RenderProcessHostObserver.
  virtual void RenderProcessHostDestroyed(
        content::RenderProcessHost* host) OVERRIDE;

 private:
  virtual ~GoogleURLObserver() {}

  // Callback that is called when the Google URL is updated.
  void OnGoogleURLUpdated();

  GoogleURLTracker* google_url_tracker_;
  scoped_refptr<GoogleURLChangeNotifier> change_notifier_;

  scoped_ptr<GoogleURLTracker::Subscription> google_url_updated_subscription_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLObserver);
};

GoogleURLObserver::GoogleURLObserver(
    GoogleURLTracker* google_url_tracker,
    GoogleURLChangeNotifier* change_notifier,
    content::RenderProcessHost* host)
    : google_url_tracker_(google_url_tracker),
      change_notifier_(change_notifier) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  google_url_updated_subscription_ =
      google_url_tracker_->RegisterCallback(base::Bind(
          &GoogleURLObserver::OnGoogleURLUpdated, base::Unretained(this)));
  host->AddObserver(this);
}

void GoogleURLObserver::OnGoogleURLUpdated() {
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&GoogleURLChangeNotifier::OnChange,
                                     change_notifier_.get(),
                                     google_url_tracker_->google_url().spec()));
}

void GoogleURLObserver::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  delete this;
}

// Indicates if the two inputs have the same security origin.
// |requested_origin| should only be a security origin (no path, etc.).
// It is ok if |template_url| is NULL.
static bool IsSameOrigin(const GURL& requested_origin,
                         TemplateURL* template_url,
                         const SearchTermsData& search_terms_data) {
  DCHECK(requested_origin == requested_origin.GetOrigin());
  DCHECK(template_url->GetType() != TemplateURL::OMNIBOX_API_EXTENSION);
  return requested_origin ==
      template_url->GenerateSearchURL(search_terms_data).GetOrigin();
}

}  // namespace

SearchProviderInstallData::SearchProviderInstallData(
    TemplateURLService* template_url_service,
    const std::string& google_base_url,
    GoogleURLTracker* google_url_tracker,
    content::RenderProcessHost* host)
    : template_url_service_(template_url_service),
      google_base_url_(google_base_url),
      weak_factory_(this) {
  // GoogleURLTracker is not created in tests.
  if (google_url_tracker) {
    // GoogleURLObserver is responsible for killing itself when
    // the given notification occurs.
    new GoogleURLObserver(
        google_url_tracker,
        new GoogleURLChangeNotifier(weak_factory_.GetWeakPtr()),
        host);
  }
}

SearchProviderInstallData::~SearchProviderInstallData() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void SearchProviderInstallData::CallWhenLoaded(const base::Closure& closure) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (provider_map_.get()) {
    closure.Run();
    return;
  }

  bool do_load = closure_queue_.empty();
  closure_queue_.push_back(closure);

  // If the queue wasn't empty, there was already a load in progress.
  if (!do_load)
    return;

  if (template_url_service_) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&LoadDataOnUIThread,
                   template_url_service_,
                   base::Bind(&SearchProviderInstallData::OnTemplateURLsLoaded,
                              weak_factory_.GetWeakPtr())));
  } else {
    OnLoadFailed();
  }
}

SearchProviderInstallData::State SearchProviderInstallData::GetInstallState(
    const GURL& requested_origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(provider_map_.get());

  // First check to see if the origin is the default search provider.
  if (requested_origin.spec() == default_search_origin_)
    return INSTALLED_AS_DEFAULT;

  // Is the url any search provider?
  const TemplateURLSet* urls = provider_map_->GetURLsForHost(
      requested_origin.host());
  if (!urls)
    return NOT_INSTALLED;

  IOThreadSearchTermsData search_terms_data(google_base_url_);
  for (TemplateURLSet::const_iterator i = urls->begin();
       i != urls->end(); ++i) {
    if (IsSameOrigin(requested_origin, *i, search_terms_data))
      return INSTALLED_BUT_NOT_DEFAULT;
  }
  return NOT_INSTALLED;
}

void SearchProviderInstallData::OnGoogleURLChange(
    const std::string& google_base_url) {
  google_base_url_ = google_base_url;
}

void SearchProviderInstallData::OnTemplateURLsLoaded(
    ScopedVector<TemplateURL> template_urls,
    TemplateURL* default_provider) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  template_urls_ = template_urls.Pass();

  IOThreadSearchTermsData search_terms_data(google_base_url_);
  provider_map_.reset(new SearchHostToURLsMap());
  provider_map_->Init(template_urls_.get(), search_terms_data);
  SetDefault(default_provider);
  NotifyLoaded();
}

void SearchProviderInstallData::SetDefault(const TemplateURL* template_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!template_url) {
    default_search_origin_.clear();
    return;
  }

  DCHECK(template_url->GetType() != TemplateURL::OMNIBOX_API_EXTENSION);

  IOThreadSearchTermsData search_terms_data(google_base_url_);
  const GURL url(template_url->GenerateSearchURL(search_terms_data));
  if (!url.is_valid() || !url.has_host()) {
    default_search_origin_.clear();
    return;
  }
  default_search_origin_ = url.GetOrigin().spec();
}

void SearchProviderInstallData::OnLoadFailed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  provider_map_.reset(new SearchHostToURLsMap());
  IOThreadSearchTermsData search_terms_data(google_base_url_);
  provider_map_->Init(template_urls_.get(), search_terms_data);
  SetDefault(NULL);
  NotifyLoaded();
}

void SearchProviderInstallData::NotifyLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  std::vector<base::Closure> closure_queue;
  closure_queue.swap(closure_queue_);

  std::for_each(closure_queue.begin(),
                closure_queue.end(),
                std::mem_fun_ref(&base::Closure::Run));

  // Since we expect this request to be rare, clear out the information. This
  // also keeps the responses current as the search providers change.
  provider_map_.reset();
  SetDefault(NULL);
}
