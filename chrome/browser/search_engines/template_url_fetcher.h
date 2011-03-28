// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_H_
#pragma once

#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
class Profile;
class TemplateURL;
class TemplateURLFetcherCallbacks;

// TemplateURLFetcher is responsible for downloading OpenSearch description
// documents, creating a TemplateURL from the OSDD, and adding the TemplateURL
// to the TemplateURLModel. Downloading is done in the background.
//
class TemplateURLFetcher {
 public:
  enum ProviderType {
    AUTODETECTED_PROVIDER,
    EXPLICIT_PROVIDER,  // Supplied by Javascript.
    EXPLICIT_DEFAULT_PROVIDER  // Supplied by Javascript as default provider.
  };

  // Creates a TemplateURLFetcher with the specified Profile.
  explicit TemplateURLFetcher(Profile* profile);
  ~TemplateURLFetcher();

  // If TemplateURLFetcher is not already downloading the OSDD for osdd_url,
  // it is downloaded. If successful and the result can be parsed, a TemplateURL
  // is added to the TemplateURLModel. Takes ownership of |callbacks|.
  void ScheduleDownload(const string16& keyword,
                        const GURL& osdd_url,
                        const GURL& favicon_url,
                        TemplateURLFetcherCallbacks* callbacks,
                        ProviderType provider_type);

  // The current number of outstanding requests.
  int requests_count() const { return requests_->size(); }

 private:
  friend class RequestDelegate;

  // A RequestDelegate is created to download each OSDD. When done downloading
  // RequestCompleted is invoked back on the TemplateURLFetcher.
  class RequestDelegate;

  Profile* profile() const { return profile_; }

  // Invoked from the RequestDelegate when done downloading.
  void RequestCompleted(RequestDelegate* request);

  Profile* profile_;

  // In progress requests.
  ScopedVector<RequestDelegate> requests_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLFetcher);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_H_
