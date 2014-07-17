// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_DATA_H_
#define CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_DATA_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"

class GURL;
class GoogleURLTracker;
class SearchHostToURLsMap;
class TemplateURL;
class TemplateURLService;

namespace content {
class RenderProcessHost;
}

// Provides the search provider install state for the I/O thread. It works by
// loading the data on demand (when CallWhenLoaded is called) and then throwing
// away the results after the callbacks are done, so the results are always up
// to date with what is in the database.
class SearchProviderInstallData {
 public:
  enum State {
    // The search provider is not installed.
    NOT_INSTALLED = 0,

    // The search provider is in the user's set but is not
    INSTALLED_BUT_NOT_DEFAULT = 1,

    // The search provider is set as the user's default.
    INSTALLED_AS_DEFAULT = 2
  };

  // |host| is a RenderProcessHost that is observed, and whose destruction is a
  // signal to this class that it no longer needs to be kept up to date. (Note
  // that this class may be deleted before or after that death occurs. It
  // doesn't matter.)
  SearchProviderInstallData(TemplateURLService* template_url_service,
                            const std::string& google_base_url,
                            GoogleURLTracker* google_url_tracker,
                            content::RenderProcessHost* host);
  virtual ~SearchProviderInstallData();

  // Use to determine when the search provider information is loaded. The
  // callback may happen synchronously or asynchronously. There is no need to do
  // anything special to make it function (as it just relies on the normal I/O
  // thread message loop).
  void CallWhenLoaded(const base::Closure& closure);

  // Returns the search provider install state for the given origin.
  // This should only be called while a task is called back from CallWhenLoaded.
  State GetInstallState(const GURL& requested_origin);

  // Called when the google base url has changed.
  void OnGoogleURLChange(const std::string& google_base_url);

 private:
  // Receives a copy of the TemplateURLService's keywords on the IO thread.
  void OnTemplateURLsLoaded(ScopedVector<TemplateURL> template_urls,
                            TemplateURL* default_provider);

  // Stores information about the default search provider.
  void SetDefault(const TemplateURL* template_url);

  // Sets up the loaded state and then lets clients know that the search
  // provider install state has been loaded.
  void OnLoadFailed();

  // Does notifications to let clients know that the search provider
  // install state has been loaded.
  void NotifyLoaded();

  // The original data source. Only accessed on the UI thread.
  TemplateURLService* template_url_service_;

  // The list of closures to call after the load has finished. If empty, there
  // is no pending load.
  std::vector<base::Closure> closure_queue_;

  // Holds results of a load that was done using this class.
  scoped_ptr<SearchHostToURLsMap> provider_map_;

  // The list of template urls that are owned by the class.
  ScopedVector<TemplateURL> template_urls_;

  // The security origin for the default search provider.
  std::string default_search_origin_;

  // The google base url.
  std::string google_base_url_;

  base::WeakPtrFactory<SearchProviderInstallData> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchProviderInstallData);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_DATA_H_
