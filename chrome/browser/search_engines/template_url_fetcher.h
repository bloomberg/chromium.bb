// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_H_
#pragma once

#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
class Profile;
class TemplateURL;
class TemplateURLFetcherCallbacks;

namespace content {
class WebContents;
}

// TemplateURLFetcher is responsible for downloading OpenSearch description
// documents, creating a TemplateURL from the OSDD, and adding the TemplateURL
// to the TemplateURLService. Downloading is done in the background.
//
class TemplateURLFetcher : public ProfileKeyedService {
 public:
  enum ProviderType {
    AUTODETECTED_PROVIDER,
    EXPLICIT_PROVIDER  // Supplied by Javascript.
  };

  // Creates a TemplateURLFetcher with the specified Profile.
  explicit TemplateURLFetcher(Profile* profile);
  virtual ~TemplateURLFetcher();

  // If TemplateURLFetcher is not already downloading the OSDD for osdd_url,
  // it is downloaded. If successful and the result can be parsed, a TemplateURL
  // is added to the TemplateURLService. Takes ownership of |callbacks|.
  //
  // If |provider_type| is AUTODETECTED_PROVIDER, |keyword| must be non-empty,
  // and if there's already a non-replaceable TemplateURL in the model for
  // |keyword|, or we're already downloading an OSDD for this keyword, no
  // download is started.  If |provider_type| is EXPLICIT_PROVIDER, |keyword| is
  // ignored.
  //
  // |web_contents| specifies which WebContents displays the page the OSDD is
  // downloaded for. |web_contents| must not be NULL, except during tests.
  void ScheduleDownload(const string16& keyword,
                        const GURL& osdd_url,
                        const GURL& favicon_url,
                        content::WebContents* web_contents,
                        TemplateURLFetcherCallbacks* callbacks,
                        ProviderType provider_type);

  // The current number of outstanding requests.
  int requests_count() const { return requests_.size(); }

 private:
  // A RequestDelegate is created to download each OSDD. When done downloading
  // RequestCompleted is invoked back on the TemplateURLFetcher.
  class RequestDelegate;
  friend class RequestDelegate;

  typedef ScopedVector<RequestDelegate> Requests;

  Profile* profile() const { return profile_; }

  // Invoked from the RequestDelegate when done downloading.
  void RequestCompleted(RequestDelegate* request);

  Profile* profile_;

  // In progress requests.
  Requests requests_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLFetcher);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_H_
