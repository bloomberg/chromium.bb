// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_LOCAL_NTP_SOURCE_H_
#define CHROME_BROWSER_SEARCH_LOCAL_NTP_SOURCE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/url_data_source.h"

class Profile;

// Serves HTML and resources for the local new tab page i.e.
// chrome-search://local-ntp/local-ntp.html
class LocalNtpSource : public content::URLDataSource {
 public:
  explicit LocalNtpSource(Profile* profile);

 private:
  class GoogleSearchProviderTracker;

  ~LocalNtpSource() override;

  // Overridden from content::URLDataSource:
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string& path) const override;
  bool AllowCaching() const override;
  bool ShouldServiceRequest(const net::URLRequest* request) const override;
  std::string GetContentSecurityPolicyScriptSrc() const override;
  std::string GetContentSecurityPolicyChildSrc() const override;

  void DefaultSearchProviderIsGoogleChanged(bool is_google);

  void SetDefaultSearchProviderIsGoogleOnIOThread(bool is_google);

  Profile* profile_;

  std::unique_ptr<GoogleSearchProviderTracker> google_tracker_;
  bool default_search_provider_is_google_;
  // A copy of |default_search_provider_is_google_| for use on the IO thread.
  bool default_search_provider_is_google_io_thread_;

  base::WeakPtrFactory<LocalNtpSource> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalNtpSource);
};

#endif  // CHROME_BROWSER_SEARCH_LOCAL_NTP_SOURCE_H_
