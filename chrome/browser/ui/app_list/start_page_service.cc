// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/start_page_service.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/singleton.h"
#include "base/metrics/user_metrics.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/app_list/speech_auth_helper.h"
#include "chrome/browser/ui/app_list/speech_recognizer.h"
#include "chrome/browser/ui/app_list/start_page_observer.h"
#include "chrome/browser/ui/app_list/start_page_service_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/ui/zoom/zoom_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/speech_recognition_session_preamble.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_fetcher.h"
#include "ui/app_list/app_list_switches.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#endif

using base::RecordAction;
using base::UserMetricsAction;

namespace app_list {

namespace {

// Path to google.com's doodle JSON.
const char kDoodleJsonPath[] = "async/ddljson";

// Delay between checking for a new doodle when no doodle is found.
const int kDefaultDoodleRecheckDelayMinutes = 30;

// Delay before loading the start page WebContents on initialization.
const int kLoadContentsDelaySeconds = 5;

bool InSpeechRecognition(SpeechRecognitionState state) {
  return state == SPEECH_RECOGNITION_RECOGNIZING ||
      state == SPEECH_RECOGNITION_IN_SPEECH;
}

}  // namespace

class StartPageService::ProfileDestroyObserver
    : public content::NotificationObserver {
 public:
  explicit ProfileDestroyObserver(StartPageService* service)
      : service_(service) {
    if (service_->profile()->IsOffTheRecord()) {
      // We need to be notified when the original profile gets destroyed as well
      // as the OTR profile, because the original profile will be destroyed
      // first, and a DCHECK at that time ensures that the OTR profile has 0
      // hosts. See http://crbug.com/463419.
      registrar_.Add(
          this, chrome::NOTIFICATION_PROFILE_DESTROYED,
          content::Source<Profile>(service_->profile()->GetOriginalProfile()));
    }
    registrar_.Add(this,
                   chrome::NOTIFICATION_PROFILE_DESTROYED,
                   content::Source<Profile>(service_->profile()));
  }
  ~ProfileDestroyObserver() override {}

 private:
  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);
    DCHECK(service_->profile()->IsSameProfile(
        content::Source<Profile>(source).ptr()));
    registrar_.RemoveAll();
    service_->Shutdown();
  }

  StartPageService* service_;  // Owner of this class.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDestroyObserver);
};

class StartPageService::StartPageWebContentsDelegate
    : public content::WebContentsDelegate {
 public:
  explicit StartPageWebContentsDelegate(Profile* profile) : profile_(profile) {}
  ~StartPageWebContentsDelegate() override {}

  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override {
    if (MediaStreamInfoBarDelegate::Create(web_contents, request, callback))
      NOTREACHED() << "Media stream not allowed for WebUI";
  }

  bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override {
    return MediaCaptureDevicesDispatcher::GetInstance()
        ->CheckMediaAccessPermission(web_contents, security_origin, type);
  }

  void AddNewContents(content::WebContents* source,
                      content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture,
                      bool* was_blocked) override {
    chrome::ScopedTabbedBrowserDisplayer displayer(
        profile_, chrome::GetActiveDesktop());
    // Force all links to open in a new tab, even if they were trying to open a
    // new window.
    disposition =
        disposition == NEW_BACKGROUND_TAB ? disposition : NEW_FOREGROUND_TAB;
    chrome::AddWebContents(displayer.browser(),
                           nullptr,
                           new_contents,
                           disposition,
                           initial_pos,
                           user_gesture,
                           was_blocked);
  }

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override {
    // Force all links to open in a new tab, even if they were trying to open a
    // window.
    chrome::NavigateParams new_tab_params(
        static_cast<Browser*>(nullptr), params.url, params.transition);
    if (params.disposition == NEW_BACKGROUND_TAB) {
      new_tab_params.disposition = NEW_BACKGROUND_TAB;
    } else {
      new_tab_params.disposition = NEW_FOREGROUND_TAB;
      new_tab_params.window_action = chrome::NavigateParams::SHOW_WINDOW;
    }

    new_tab_params.initiating_profile = profile_;
    chrome::Navigate(&new_tab_params);

    return new_tab_params.target_contents;
  }

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(StartPageWebContentsDelegate);
};

#if defined(OS_CHROMEOS)

class StartPageService::AudioStatus
    : public chromeos::CrasAudioHandler::AudioObserver {
 public:
  explicit AudioStatus(StartPageService* start_page_service)
      : start_page_service_(start_page_service) {
    chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
    CheckAndUpdate();
  }

  ~AudioStatus() override {
    chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
  }

  bool CanListen() {
    chromeos::CrasAudioHandler* audio_handler =
        chromeos::CrasAudioHandler::Get();
    return (audio_handler->GetPrimaryActiveInputNode() != 0) &&
           !audio_handler->IsInputMuted();
  }

 private:
  void CheckAndUpdate() {
    // TODO(mukai): If the system can listen, this should also restart the
    // hotword recognition.
    start_page_service_->OnMicrophoneChanged(CanListen());
  }

  // chromeos::CrasAudioHandler::AudioObserver:
  void OnInputMuteChanged() override { CheckAndUpdate(); }

  void OnActiveInputNodeChanged() override { CheckAndUpdate(); }

  StartPageService* start_page_service_;

  DISALLOW_COPY_AND_ASSIGN(AudioStatus);
};

#endif  // OS_CHROMEOS

class StartPageService::NetworkChangeObserver
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  explicit NetworkChangeObserver(StartPageService* start_page_service)
      : start_page_service_(start_page_service) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // NOTE: This is used to detect network connectivity changes. However, what
    // we really want is internet connectivity changes because voice recognition
    // needs to talk to a web service. However, this information isn't
    // available, so network changes are the best we can do.
    net::NetworkChangeNotifier::AddNetworkChangeObserver(this);

    last_type_ = net::NetworkChangeNotifier::GetConnectionType();
    // Handle the case where we're started with no network available.
    if (last_type_ == net::NetworkChangeNotifier::CONNECTION_NONE)
      start_page_service_->OnNetworkChanged(false);
  }

  ~NetworkChangeObserver() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

 private:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    // Threading note: NetworkChangeNotifier's contract is that observers are
    // called on the same thread that they're registered. In this case, it
    // should always be the UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (type == net::NetworkChangeNotifier::CONNECTION_NONE) {
      start_page_service_->OnNetworkChanged(false);
    } else if (last_type_ == net::NetworkChangeNotifier::CONNECTION_NONE &&
               type != net::NetworkChangeNotifier::CONNECTION_NONE) {
      start_page_service_->OnNetworkChanged(true);
    }
    last_type_ = type;
  }

  StartPageService* start_page_service_;
  net::NetworkChangeNotifier::ConnectionType last_type_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeObserver);
};

// static
StartPageService* StartPageService::Get(Profile* profile) {
  return StartPageServiceFactory::GetForProfile(profile);
}

StartPageService::StartPageService(Profile* profile)
    : profile_(profile),
      profile_destroy_observer_(new ProfileDestroyObserver(this)),
      state_(app_list::SPEECH_RECOGNITION_OFF),
      speech_button_toggled_manually_(false),
      speech_result_obtained_(false),
      webui_finished_loading_(false),
      speech_auth_helper_(new SpeechAuthHelper(profile, &clock_)),
      network_available_(true),
      microphone_available_(true),
      search_engine_is_google_(false),
      weak_factory_(this) {
  // If experimental hotwording is enabled, then we're always "ready".
  // Transitioning into the "hotword recognizing" state is handled by the
  // hotword extension.
  if (HotwordService::IsExperimentalHotwordingEnabled()) {
    state_ = app_list::SPEECH_RECOGNITION_READY;
  }
  if (switches::IsExperimentalAppListEnabled()) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile_);
    const TemplateURL* default_provider =
        template_url_service->GetDefaultSearchProvider();
    search_engine_is_google_ =
        TemplateURLPrepopulateData::GetEngineType(
            *default_provider, template_url_service->search_terms_data()) ==
        SEARCH_ENGINE_GOOGLE;
  }

  network_change_observer_.reset(new NetworkChangeObserver(this));
}

StartPageService::~StartPageService() {
}

void StartPageService::AddObserver(StartPageObserver* observer) {
  observers_.AddObserver(observer);
}

void StartPageService::RemoveObserver(StartPageObserver* observer) {
  observers_.RemoveObserver(observer);
}

void StartPageService::OnMicrophoneChanged(bool available) {
  microphone_available_ = available;
  UpdateRecognitionState();
}

void StartPageService::OnNetworkChanged(bool available) {
  network_available_ = available;
  UpdateRecognitionState();
}

void StartPageService::UpdateRecognitionState() {
  if (ShouldEnableSpeechRecognition()) {
    if (state_ == SPEECH_RECOGNITION_OFF)
      OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_READY);
  } else {
    OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_OFF);
  }
}

void StartPageService::Init() {
  // Do not load the start page web contents in tests because many tests assume
  // no WebContents exist except the ones they make.
  if (switches::IsExperimentalAppListEnabled() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType)) {
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&StartPageService::LoadContentsIfNeeded,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(kLoadContentsDelaySeconds));
  }
}

void StartPageService::LoadContentsIfNeeded() {
  if (!contents_)
    LoadContents();
}

bool StartPageService::ShouldEnableSpeechRecognition() const {
  return microphone_available_ && network_available_;
}

void StartPageService::AppListShown() {
  if (!contents_) {
    LoadContents();
  } else if (contents_->IsCrashed()) {
    LoadStartPageURL();
  } else if (contents_->GetWebUI()) {
    // If experimental hotwording is enabled, don't initialize the web speech
    // API, which is not used with
    // experimental hotwording.
    contents_->GetWebUI()->CallJavascriptFunction(
        "appList.startPage.onAppListShown",
        base::FundamentalValue(HotwordEnabled()),
        base::FundamentalValue(
            !HotwordService::IsExperimentalHotwordingEnabled()));
  }

#if defined(OS_CHROMEOS)
  audio_status_.reset(new AudioStatus(this));
#endif
}

void StartPageService::AppListHidden() {
  if (contents_->GetWebUI()) {
    contents_->GetWebUI()->CallJavascriptFunction(
        "appList.startPage.onAppListHidden");
  }
  if (!app_list::switches::IsExperimentalAppListEnabled())
    UnloadContents();

  if (HotwordService::IsExperimentalHotwordingEnabled() &&
      speech_recognizer_) {
    speech_recognizer_->Stop();
    speech_recognizer_.reset();

    // When the SpeechRecognizer is destroyed above, we get stuck in the current
    // speech state instead of being reset into the READY state. Reset the
    // speech state explicitly so that speech works when the launcher is opened
    // again.
    OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_READY);
  }

#if defined(OS_CHROMEOS)
  audio_status_.reset();
#endif
}

void StartPageService::ToggleSpeechRecognition(
    const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble) {
  DCHECK(contents_);
  speech_button_toggled_manually_ = true;

  // Speech recognition under V2 hotwording does not depend in any way on the
  // start page web contents. Do this code path first to make this explicit and
  // easier to identify what code needs to be deleted when V2 hotwording is
  // stable.
  if (HotwordService::IsExperimentalHotwordingEnabled()) {
    if (!speech_recognizer_) {
      std::string profile_locale;
#if defined(OS_CHROMEOS)
      profile_locale = profile_->GetPrefs()->GetString(
          prefs::kApplicationLocale);
#endif
      if (profile_locale.empty())
        profile_locale = g_browser_process->GetApplicationLocale();

      speech_recognizer_.reset(
          new SpeechRecognizer(weak_factory_.GetWeakPtr(),
                               profile_->GetRequestContext(),
                               profile_locale));
    }

    speech_recognizer_->Start(preamble);
    return;
  }

  if (!contents_->GetWebUI())
    return;

  if (!webui_finished_loading_) {
    pending_webui_callbacks_.push_back(
        base::Bind(&StartPageService::ToggleSpeechRecognition,
                   base::Unretained(this),
                   preamble));
    return;
  }

  contents_->GetWebUI()->CallJavascriptFunction(
      "appList.startPage.toggleSpeechRecognition");
}

bool StartPageService::HotwordEnabled() {
// Voice input for the launcher is unsupported on non-ChromeOS platforms.
// TODO(amistry): Make speech input, and hotwording, work on non-ChromeOS.
#if defined(OS_CHROMEOS)
  if (HotwordService::IsExperimentalHotwordingEnabled()) {
    HotwordService* service = HotwordServiceFactory::GetForProfile(profile_);
    return state_ != SPEECH_RECOGNITION_OFF &&
        service &&
        (service->IsSometimesOnEnabled() || service->IsAlwaysOnEnabled()) &&
        service->IsServiceAvailable();
  }
  return HotwordServiceFactory::IsServiceAvailable(profile_) &&
      profile_->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled);
#else
  return false;
#endif
}

content::WebContents* StartPageService::GetStartPageContents() {
  return app_list::switches::IsExperimentalAppListEnabled() ? contents_.get()
                                                            : NULL;
}

content::WebContents* StartPageService::GetSpeechRecognitionContents() {
  if (app_list::switches::IsVoiceSearchEnabled()) {
    if (!contents_)
      LoadContents();
    return contents_.get();
  }
  return NULL;
}

void StartPageService::OnSpeechResult(
    const base::string16& query, bool is_final) {
  if (is_final) {
    speech_result_obtained_ = true;
    RecordAction(UserMetricsAction("AppList_SearchedBySpeech"));
  }
  FOR_EACH_OBSERVER(StartPageObserver,
                    observers_,
                    OnSpeechResult(query, is_final));
}

void StartPageService::OnSpeechSoundLevelChanged(int16_t level) {
  FOR_EACH_OBSERVER(StartPageObserver,
                    observers_,
                    OnSpeechSoundLevelChanged(level));
}

void StartPageService::OnSpeechRecognitionStateChanged(
    SpeechRecognitionState new_state) {
#if defined(OS_CHROMEOS)
  // Sometimes this can be called even though there are no audio input devices.
  if (audio_status_ && !audio_status_->CanListen())
    new_state = SPEECH_RECOGNITION_OFF;
#endif
  if (!ShouldEnableSpeechRecognition())
    new_state = SPEECH_RECOGNITION_OFF;

  if (state_ == new_state)
    return;

  if (HotwordService::IsExperimentalHotwordingEnabled() &&
      (new_state == SPEECH_RECOGNITION_READY ||
       new_state == SPEECH_RECOGNITION_OFF) &&
      speech_recognizer_) {
    speech_recognizer_->Stop();
  }

  if (!InSpeechRecognition(state_) && InSpeechRecognition(new_state)) {
    if (!speech_button_toggled_manually_ &&
        state_ == SPEECH_RECOGNITION_HOTWORD_LISTENING) {
      RecordAction(UserMetricsAction("AppList_HotwordRecognized"));
    } else {
      RecordAction(UserMetricsAction("AppList_VoiceSearchStartedManually"));
    }
  } else if (InSpeechRecognition(state_) && !InSpeechRecognition(new_state) &&
             !speech_result_obtained_) {
    RecordAction(UserMetricsAction("AppList_VoiceSearchCanceled"));
  }
  speech_button_toggled_manually_ = false;
  speech_result_obtained_ = false;
  state_ = new_state;
  FOR_EACH_OBSERVER(StartPageObserver,
                    observers_,
                    OnSpeechRecognitionStateChanged(new_state));
}

void StartPageService::GetSpeechAuthParameters(std::string* auth_scope,
                                               std::string* auth_token) {
  if (HotwordService::IsExperimentalHotwordingEnabled()) {
    HotwordService* service = HotwordServiceFactory::GetForProfile(profile_);
    if (service &&
        service->IsOptedIntoAudioLogging() &&
        service->IsAlwaysOnEnabled() &&
        !speech_auth_helper_->GetToken().empty()) {
      *auth_scope = speech_auth_helper_->GetScope();
      *auth_token = speech_auth_helper_->GetToken();
    }
  }
}

void StartPageService::Shutdown() {
  UnloadContents();
#if defined(OS_CHROMEOS)
  audio_status_.reset();
#endif

  speech_auth_helper_.reset();
  network_change_observer_.reset();
}

void StartPageService::DidNavigateMainFrame(
    const content::LoadCommittedDetails& /*details*/,
    const content::FrameNavigateParams& /*params*/) {
  // Set the zoom level in DidNavigateMainFrame, as this is the earliest point
  // at which it can be done and not be affected by the ZoomController's
  // DidNavigateMainFrame handler.
  //
  // Use a temporary zoom level for this web contents (aka isolated zoom
  // mode) so changes to its zoom aren't reflected in any preferences.
  ui_zoom::ZoomController::FromWebContents(contents_.get())
      ->SetZoomMode(ui_zoom::ZoomController::ZOOM_MODE_ISOLATED);
  // Set to have a zoom level of 0, which corresponds to 100%, so the
  // contents aren't affected by the browser's default zoom level.
  ui_zoom::ZoomController::FromWebContents(contents_.get())->SetZoomLevel(0);
}

void StartPageService::RenderProcessGone(base::TerminationStatus status) {
  // TODO(calamity): Remove this log after http://crbug.com/462082 is diagnosed.
  if (status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ||
      status == base::TERMINATION_STATUS_PROCESS_CRASHED) {
    LOG(WARNING) << "App list doodle contents crashed with status " << status;
  }
}

void StartPageService::WebUILoaded() {
  // There's a race condition between the WebUI loading, and calling its JS
  // functions. Specifically, calling LoadContents() doesn't mean that the page
  // has loaded, but several code paths make this assumption. This function
  // allows us to defer calling JS functions until after the page has finished
  // loading.
  webui_finished_loading_ = true;
  for (const auto& cb : pending_webui_callbacks_)
    cb.Run();
  pending_webui_callbacks_.clear();

  FetchDoodleJson();
}

void StartPageService::LoadContents() {
  contents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(profile_)));
  contents_delegate_.reset(new StartPageWebContentsDelegate(profile_));
  contents_->SetDelegate(contents_delegate_.get());

  // The ZoomController needs to be created before the web contents is observed
  // by this object. Otherwise it will react to DidNavigateMainFrame after this
  // object does, resetting the zoom mode in the process.
  ui_zoom::ZoomController::CreateForWebContents(contents_.get());
  Observe(contents_.get());

  LoadStartPageURL();
}

void StartPageService::UnloadContents() {
  contents_.reset();
  webui_finished_loading_ = false;
}

void StartPageService::LoadStartPageURL() {
  contents_->GetController().LoadURL(
      GURL(chrome::kChromeUIAppListStartPageURL),
      content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());

  contents_->GetRenderViewHost()->GetView()->SetBackgroundColor(
        SK_ColorTRANSPARENT);
}

void StartPageService::FetchDoodleJson() {
  if (!search_engine_is_google_)
    return;

  GURL::Replacements replacements;
  replacements.SetPathStr(kDoodleJsonPath);

  GURL google_base_url(UIThreadSearchTermsData(profile_).GoogleBaseURLValue());
  GURL doodle_url = google_base_url.ReplaceComponents(replacements);
  doodle_fetcher_.reset(
      net::URLFetcher::Create(0, doodle_url, net::URLFetcher::GET, this));
  doodle_fetcher_->SetRequestContext(profile_->GetRequestContext());
  doodle_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  doodle_fetcher_->Start();
}

void StartPageService::OnURLFetchComplete(const net::URLFetcher* source) {
  std::string json_data;
  source->GetResponseAsString(&json_data);

  // Remove XSSI guard for JSON parsing.
  size_t json_start_index = json_data.find("{");
  base::StringPiece json_data_substr(json_data);
  if (json_start_index != std::string::npos)
    json_data_substr.remove_prefix(json_start_index);

  JSONStringValueSerializer deserializer(json_data_substr);
  deserializer.set_allow_trailing_comma(true);
  int error_code = 0;
  scoped_ptr<base::Value> doodle_json(
      deserializer.Deserialize(&error_code, nullptr));

  base::TimeDelta recheck_delay =
      base::TimeDelta::FromMinutes(kDefaultDoodleRecheckDelayMinutes);

  if (error_code == 0) {
    base::DictionaryValue* doodle_dictionary = nullptr;
    // Use the supplied TTL as the recheck delay if available.
    if (doodle_json->GetAsDictionary(&doodle_dictionary)) {
      int time_to_live = 0;
      if (doodle_dictionary->GetInteger("ddljson.time_to_live_ms",
                                        &time_to_live)) {
        recheck_delay = base::TimeDelta::FromMilliseconds(time_to_live);
      }
    }

    if (contents_ && contents_->GetWebUI()) {
      contents_->GetWebUI()->CallJavascriptFunction(
          "appList.startPage.onAppListDoodleUpdated", *doodle_json,
          base::StringValue(
              UIThreadSearchTermsData(profile_).GoogleBaseURLValue()));
    }
  }

  // Check for a new doodle.
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&StartPageService::FetchDoodleJson,
                 weak_factory_.GetWeakPtr()),
      recheck_delay);
}

}  // namespace app_list
