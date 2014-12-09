// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_LOCAL_NTP_SOURCE_H_
#define CHROME_BROWSER_SEARCH_LOCAL_NTP_SOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/url_data_source.h"

class Profile;

// Serves HTML and resources for the local new tab page i.e.
// chrome-search://local-ntp/local-ntp.html
class LocalNtpSource : public content::URLDataSource {
 public:
  explicit LocalNtpSource(Profile* profile);

 private:
  ~LocalNtpSource() override;

  // Overridden from content::URLDataSource:
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string& path) const override;
  bool ShouldServiceRequest(const net::URLRequest* request) const override;
  std::string GetContentSecurityPolicyFrameSrc() const override;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(LocalNtpSource);
};

#endif  // CHROME_BROWSER_SEARCH_LOCAL_NTP_SOURCE_H_
