// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_START_PAGE_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_START_PAGE_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace content {
struct SpeechRecognitionSessionPreamble;
}

namespace extensions {
class Extension;
}

namespace net {
class URLFetcher;
}

class Profile;
class SpeechRecognizer;

namespace app_list {

// StartPageService collects data to be displayed in app list's start page
// and hosts the start page contents.
class StartPageService : public KeyedService,
                         public content::WebContentsObserver,
                         public net::URLFetcherDelegate {
 public:
  typedef std::vector<scoped_refptr<const extensions::Extension> >
      ExtensionList;
  // Gets the instance for the given profile. May return nullptr.
  static StartPageService* Get(Profile* profile);

  void Init();

  // Loads the start page WebContents if it hasn't already been loaded.
  void LoadContentsIfNeeded();

  void AppListShown();
  void AppListHidden();

  // Called when the WebUI has finished loading.
  void WebUILoaded();

  // They return essentially the same web contents but might return NULL when
  // some flag disables the feature.
  content::WebContents* GetStartPageContents();

  void set_search_engine_is_google(bool search_engine_is_google) {
    search_engine_is_google_ = search_engine_is_google;
  }
  Profile* profile() { return profile_; }

 protected:
  // Protected for testing.
  explicit StartPageService(Profile* profile);
  ~StartPageService() override;

 private:
  friend class StartPageServiceFactory;

  // ProfileDestroyObserver to shutdown the service on exiting. WebContents
  // depends on the profile and needs to be closed before the profile and its
  // keyed service shutdown.
  class ProfileDestroyObserver;

  // The WebContentsDelegate implementation for the start page. This allows
  // getUserMedia() request from the web contents.
  class StartPageWebContentsDelegate;

  // This class observes network change events and disables/enables voice search
  // based on network connectivity.
  class NetworkChangeObserver;

  void LoadContents();
  void UnloadContents();

  // Loads the start page URL for |contents_|.
  void LoadStartPageURL();

  // Fetch the Google Doodle JSON data and update the app list start page.
  void FetchDoodleJson();

  // net::URLFetcherDelegate overrides:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // KeyedService overrides:
  void Shutdown() override;

  // contents::WebContentsObserver overrides;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Change the known network connectivity state. |available| should be true if
  // at least one network is connected to.
  void OnNetworkChanged(bool available);

  Profile* profile_;
  std::unique_ptr<content::WebContents> contents_;
  std::unique_ptr<StartPageWebContentsDelegate> contents_delegate_;
  std::unique_ptr<ProfileDestroyObserver> profile_destroy_observer_;

  bool webui_finished_loading_;
  std::vector<base::Closure> pending_webui_callbacks_;

  base::DefaultClock clock_;

  bool network_available_;
  std::unique_ptr<NetworkChangeObserver> network_change_observer_;

  bool search_engine_is_google_;
  std::unique_ptr<net::URLFetcher> doodle_fetcher_;
  net::BackoffEntry backoff_entry_;

  base::WeakPtrFactory<StartPageService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StartPageService);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_START_PAGE_SERVICE_H_
