// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_LOCAL_NTP_SOURCE_H_
#define CHROME_BROWSER_SEARCH_LOCAL_NTP_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_observer.h"
#include "content/public/browser/url_data_source.h"

struct OneGoogleBarData;
class OneGoogleBarService;
class Profile;

// Serves HTML and resources for the local new tab page i.e.
// chrome-search://local-ntp/local-ntp.html
class LocalNtpSource : public content::URLDataSource,
                       public OneGoogleBarServiceObserver {
 public:
  explicit LocalNtpSource(Profile* profile);

 private:
  class GoogleSearchProviderTracker;

  struct OneGoogleBarRequest {
    OneGoogleBarRequest(
        base::TimeTicks start_time,
        const content::URLDataSource::GotDataCallback& callback);
    OneGoogleBarRequest(const OneGoogleBarRequest&);
    ~OneGoogleBarRequest();

    base::TimeTicks start_time;
    content::URLDataSource::GotDataCallback callback;
  };

  ~LocalNtpSource() override;

  // Overridden from content::URLDataSource:
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string& path) const override;
  bool AllowCaching() const override;
  bool ShouldServiceRequest(const GURL& url,
                            content::ResourceContext* resource_context,
                            int render_process_id) const override;
  std::string GetContentSecurityPolicyScriptSrc() const override;
  std::string GetContentSecurityPolicyChildSrc() const override;

  // Overridden from OneGoogleBarServiceObserver:
  void OnOneGoogleBarDataUpdated() override;
  void OnOneGoogleBarServiceShuttingDown() override;

  void ServeOneGoogleBar(const base::Optional<OneGoogleBarData>& data);

  void DefaultSearchProviderIsGoogleChanged(bool is_google);

  void SetDefaultSearchProviderIsGoogleOnIOThread(bool is_google);

  Profile* const profile_;

  OneGoogleBarService* one_google_bar_service_;

  ScopedObserver<OneGoogleBarService, OneGoogleBarServiceObserver>
      one_google_bar_service_observer_;

  std::vector<OneGoogleBarRequest> one_google_bar_requests_;

  std::unique_ptr<GoogleSearchProviderTracker> google_tracker_;
  bool default_search_provider_is_google_;
  // A copy of |default_search_provider_is_google_| for use on the IO thread.
  bool default_search_provider_is_google_io_thread_;

  base::WeakPtrFactory<LocalNtpSource> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalNtpSource);
};

#endif  // CHROME_BROWSER_SEARCH_LOCAL_NTP_SOURCE_H_
