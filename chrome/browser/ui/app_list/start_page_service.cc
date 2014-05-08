// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/start_page_service.h"

#include <string>

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/metrics/user_metrics.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/ui/app_list/recommended_apps.h"
#include "chrome/browser/ui/app_list/start_page_observer.h"
#include "chrome/browser/ui/app_list/start_page_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "ui/app_list/app_list_switches.h"

using base::RecordAction;
using base::UserMetricsAction;

namespace app_list {

namespace {

bool InSpeechRecognition(SpeechRecognitionState state) {
  return state == SPEECH_RECOGNITION_RECOGNIZING ||
      state == SPEECH_RECOGNITION_IN_SPEECH;
}

}

class StartPageService::ProfileDestroyObserver
    : public content::NotificationObserver {
 public:
  explicit ProfileDestroyObserver(StartPageService* service)
      : service_(service) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_PROFILE_DESTROYED,
                   content::Source<Profile>(service_->profile()));
  }
  virtual ~ProfileDestroyObserver() {}

 private:
  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);
    DCHECK_EQ(service_->profile(), content::Source<Profile>(source).ptr());
    service_->Shutdown();
  }

  StartPageService* service_;  // Owner of this class.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDestroyObserver);
};

class StartPageService::StartPageWebContentsDelegate
    : public content::WebContentsDelegate {
 public:
  StartPageWebContentsDelegate() {}
  virtual ~StartPageWebContentsDelegate() {}

  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE {
    if (MediaStreamInfoBarDelegate::Create(web_contents, request, callback))
      NOTREACHED() << "Media stream not allowed for WebUI";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StartPageWebContentsDelegate);
};

// static
StartPageService* StartPageService::Get(Profile* profile) {
  return StartPageServiceFactory::GetForProfile(profile);
}

StartPageService::StartPageService(Profile* profile)
    : profile_(profile),
      profile_destroy_observer_(new ProfileDestroyObserver(this)),
      recommended_apps_(new RecommendedApps(profile)),
      state_(app_list::SPEECH_RECOGNITION_OFF),
      speech_button_toggled_manually_(false),
      speech_result_obtained_(false) {
  if (app_list::switches::IsExperimentalAppListEnabled())
    LoadContents();
}

StartPageService::~StartPageService() {}

void StartPageService::AddObserver(StartPageObserver* observer) {
  observers_.AddObserver(observer);
}

void StartPageService::RemoveObserver(StartPageObserver* observer) {
  observers_.RemoveObserver(observer);
}

void StartPageService::AppListShown() {
  if (!contents_) {
    LoadContents();
  } else {
    contents_->GetWebUI()->CallJavascriptFunction(
        "appList.startPage.onAppListShown",
        base::FundamentalValue(HotwordEnabled()));
  }
}

void StartPageService::AppListHidden() {
  contents_->GetWebUI()->CallJavascriptFunction(
      "appList.startPage.onAppListHidden");
  if (!app_list::switches::IsExperimentalAppListEnabled())
    UnloadContents();
}

void StartPageService::ToggleSpeechRecognition() {
  speech_button_toggled_manually_ = true;
  contents_->GetWebUI()->CallJavascriptFunction(
      "appList.startPage.toggleSpeechRecognition");
}

bool StartPageService::HotwordEnabled() {
#if defined(OS_CHROMEOS)
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

void StartPageService::OnSpeechSoundLevelChanged(int16 level) {
  FOR_EACH_OBSERVER(StartPageObserver,
                    observers_,
                    OnSpeechSoundLevelChanged(level));
}

void StartPageService::OnSpeechRecognitionStateChanged(
    SpeechRecognitionState new_state) {
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

void StartPageService::Shutdown() {
  UnloadContents();
}

void StartPageService::LoadContents() {
  contents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(profile_)));
  contents_delegate_.reset(new StartPageWebContentsDelegate());
  contents_->SetDelegate(contents_delegate_.get());

  GURL url(chrome::kChromeUIAppListStartPageURL);
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kAppListStartPageURL)) {
    url = GURL(
        command_line->GetSwitchValueASCII(::switches::kAppListStartPageURL));
  }

  contents_->GetController().LoadURL(
      url,
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

void StartPageService::UnloadContents() {
  contents_.reset();
}

}  // namespace app_list
