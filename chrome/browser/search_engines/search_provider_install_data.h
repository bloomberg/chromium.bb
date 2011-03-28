// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_DATA_H_
#define CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_DATA_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/task_queue.h"
#include "chrome/browser/webdata/web_data_service.h"

class GURL;
class NotificationSource;
class NotificationType;
class SearchHostToURLsMap;
class Task;
class TemplateURL;

// Provides the search provider install state for the I/O thread. It works by
// loading the data on demand (when CallWhenLoaded is called) and then throwing
// away the results after the callbacks are done, so the results are always up
// to date with what is in the database.
class SearchProviderInstallData : public WebDataServiceConsumer,
    public base::SupportsWeakPtr<SearchProviderInstallData> {
 public:
  enum State {
    // The search provider is not installed.
    NOT_INSTALLED = 0,

    // The search provider is in the user's set but is not
    INSTALLED_BUT_NOT_DEFAULT = 1,

    // The search provider is set as the user's default.
    INSTALLED_AS_DEFAULT = 2
  };

  // |ui_death_notification| and |ui_death_source| indentify a notification that
  // may be observed on the UI thread to know when this class no longer needs to
  // be kept up to date. (Note that this class may be deleted before or after
  // that notification occurs. It doesn't matter.)
  SearchProviderInstallData(WebDataService* web_service,
                            NotificationType ui_death_notification,
                            const NotificationSource& ui_death_source);
  virtual ~SearchProviderInstallData();

  // Use to determine when the search provider information is loaded. The
  // callback may happen synchronously or asynchronously. This takes ownership
  // of |task|. There is no need to do anything special to make it function
  // (as it just relies on the normal I/O thread message loop).
  void CallWhenLoaded(Task* task);

  // Returns the search provider install state for the given origin.
  // This should only be called while a task is called back from CallWhenLoaded.
  State GetInstallState(const GURL& requested_origin);

  // Called when the google base url has changed.
  void OnGoogleURLChange(const std::string& google_base_url);

 private:
  // WebDataServiceConsumer
  // Notification that the keywords have been loaded.
  // This is invoked from WebDataService, and should not be directly
  // invoked.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  // Stores information about the default search provider.
  void SetDefault(const TemplateURL* template_url);

  // Sets up the loaded state and then lets clients know that the search
  // provider install state has been loaded.
  void OnLoadFailed();

  // Does notifications to let clients know that the search provider
  // install state has been loaded.
  void NotifyLoaded();

  // The list of tasks to call after the load has finished.
  TaskQueue task_queue_;

  // Service used to store entries.
  scoped_refptr<WebDataService> web_service_;

  // If non-zero, we're waiting on a load.
  WebDataService::Handle load_handle_;

  // Holds results of a load that was done using this class.
  scoped_ptr<SearchHostToURLsMap> provider_map_;

  // The list of template urls that are owned by the class.
  ScopedVector<const TemplateURL> template_urls_;

  // The security origin for the default search provider.
  std::string default_search_origin_;

  // The google base url.
  std::string google_base_url_;

  DISALLOW_COPY_AND_ASSIGN(SearchProviderInstallData);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_DATA_H_
