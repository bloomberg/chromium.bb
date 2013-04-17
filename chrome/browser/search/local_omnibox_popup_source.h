// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_LOCAL_OMNIBOX_POPUP_SOURCE_H_
#define CHROME_BROWSER_SEARCH_LOCAL_OMNIBOX_POPUP_SOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/url_data_source.h"

// Serves HTML and resources for the local omnibox popup i.e.
// chrome-search://local-omnibox-popup/local_omnibox_popup.html
class LocalOmniboxPopupSource : public content::URLDataSource {
 public:
  LocalOmniboxPopupSource();

 private:
  virtual ~LocalOmniboxPopupSource();

  // Overridden from content::URLDataSource:
  virtual std::string GetSource() const OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;
  virtual bool ShouldServiceRequest(
      const net::URLRequest* request) const OVERRIDE;
  virtual std::string GetContentSecurityPolicyFrameSrc() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(LocalOmniboxPopupSource);
};

#endif  // CHROME_BROWSER_SEARCH_LOCAL_OMNIBOX_POPUP_SOURCE_H_
