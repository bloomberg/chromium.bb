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
#include "chrome/browser/ui/app_list/speech_recognizer_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "ui/app_list/speech_ui_model_observer.h"

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

namespace app_list {

class SpeechAuthHelper;
class SpeechRecognizer;
class StartPageObserver;

// StartPageService collects data to be displayed in app list's start page
// and hosts the start page contents.
class StartPageService : public KeyedService,
                         public content::WebContentsObserver,
                         public net::URLFetcherDelegate,
                         public SpeechRecognizerDelegate {
 public:
  typedef std::vector<scoped_refptr<const extensions::Extension> >
      ExtensionList;
  // Gets the instance for the given profile. May return nullptr.
  static StartPageService* Get(Profile* profile);

  void AddObserver(StartPageObserver* observer);
  void RemoveObserver(StartPageObserver* observer);

  void Init();

  // Loads the start page WebContents if it hasn't already been loaded.
  void LoadContentsIfNeeded();

  void AppListShown();
  void AppListHidden();

  void StartSpeechRecognition(
      const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble);
  void StopSpeechRecognition();

  // Called when the WebUI has finished loading.
  void WebUILoaded();

  // Returns true if the hotword is enabled in the app-launcher.
  bool HotwordEnabled();

  // They return essentially the same web contents but might return NULL when
  // some flag disables the feature.
  content::WebContents* GetStartPageContents();
  content::WebContents* GetSpeechRecognitionContents();

  void set_search_engine_is_google(bool search_engine_is_google) {
    search_engine_is_google_ = search_engine_is_google;
  }
  Profile* profile() { return profile_; }
  SpeechRecognitionState state() { return state_; }

  // Overridden from app_list::SpeechRecognizerDelegate:
  void OnSpeechResult(const base::string16& query, bool is_final) override;
  void OnSpeechSoundLevelChanged(int16_t level) override;
  void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) override;
  void GetSpeechAuthParameters(std::string* auth_scope,
                               std::string* auth_token) override;

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

#if defined(OS_CHROMEOS)
  // This class observes the change of audio input device availability and
  // checks if currently the system has valid audio input.
  class AudioStatus;
#endif

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

  // Change the known microphone availability. |available| should be true if
  // the microphone exists and is available for use.
  void OnMicrophoneChanged(bool available);
  // Change the known network connectivity state. |available| should be true if
  // at least one network is connected to.
  void OnNetworkChanged(bool available);
  // Enables/disables voice recognition based on network and microphone state.
  void UpdateRecognitionState();
  // Determines whether speech recognition should be enabled, based on the
  // current state of the StartPageService.
  bool ShouldEnableSpeechRecognition() const;

  Profile* profile_;
  std::unique_ptr<content::WebContents> contents_;
  std::unique_ptr<StartPageWebContentsDelegate> contents_delegate_;
  std::unique_ptr<ProfileDestroyObserver> profile_destroy_observer_;
  SpeechRecognitionState state_;
  base::ObserverList<StartPageObserver> observers_;
  bool speech_button_toggled_manually_;
  bool speech_result_obtained_;

  bool webui_finished_loading_;
  std::vector<base::Closure> pending_webui_callbacks_;

  base::DefaultClock clock_;
  std::unique_ptr<SpeechRecognizer> speech_recognizer_;
  std::unique_ptr<SpeechAuthHelper> speech_auth_helper_;

  bool network_available_;
  bool microphone_available_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<AudioStatus> audio_status_;
#endif
  std::unique_ptr<NetworkChangeObserver> network_change_observer_;

  bool search_engine_is_google_;
  std::unique_ptr<net::URLFetcher> doodle_fetcher_;
  net::BackoffEntry backoff_entry_;

  base::WeakPtrFactory<StartPageService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StartPageService);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_START_PAGE_SERVICE_H_
