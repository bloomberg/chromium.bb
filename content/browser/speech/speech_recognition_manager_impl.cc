// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_manager_impl.h"

#include "base/bind.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/speech/input_tag_speech_dispatcher_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/speech_recognizer.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_preferences.h"
#include "content/public/common/view_type.h"
#include "media/audio/audio_manager.h"

using content::BrowserMainLoop;
using content::BrowserThread;
using content::RenderViewHostImpl;
using content::SpeechRecognitionManager;
using content::SpeechRecognitionManagerDelegate;

SpeechRecognitionManager* SpeechRecognitionManager::GetInstance() {
  return speech::SpeechRecognitionManagerImpl::GetInstance();
}

namespace speech {

struct SpeechRecognitionManagerImpl::SpeechRecognitionParams {
  SpeechRecognitionParams(
      InputTagSpeechDispatcherHost* delegate,
      int caller_id,
      int render_process_id,
      int render_view_id,
      const gfx::Rect& element_rect,
      const std::string& language,
      const std::string& grammar,
      const std::string& origin_url,
      net::URLRequestContextGetter* context_getter,
      content::SpeechRecognitionPreferences* recognition_prefs)
      : delegate(delegate),
        caller_id(caller_id),
        render_process_id(render_process_id),
        render_view_id(render_view_id),
        element_rect(element_rect),
        language(language),
        grammar(grammar),
        origin_url(origin_url),
        context_getter(context_getter),
        recognition_prefs(recognition_prefs) {
  }

  InputTagSpeechDispatcherHost* delegate;
  int caller_id;
  int render_process_id;
  int render_view_id;
  gfx::Rect element_rect;
  std::string language;
  std::string grammar;
  std::string origin_url;
  net::URLRequestContextGetter* context_getter;
  content::SpeechRecognitionPreferences* recognition_prefs;
};

SpeechRecognitionManagerImpl* SpeechRecognitionManagerImpl::GetInstance() {
  return Singleton<SpeechRecognitionManagerImpl>::get();
}

SpeechRecognitionManagerImpl::SpeechRecognitionManagerImpl()
    : can_report_metrics_(false),
      recording_caller_id_(0) {
  delegate_.reset(content::GetContentClient()->browser()->
                  GetSpeechRecognitionManagerDelegate());
}

SpeechRecognitionManagerImpl::~SpeechRecognitionManagerImpl() {
  while (requests_.begin() != requests_.end())
    CancelRecognition(requests_.begin()->first);
}

bool SpeechRecognitionManagerImpl::HasAudioInputDevices() {
  return BrowserMainLoop::GetAudioManager()->HasAudioInputDevices();
}

bool SpeechRecognitionManagerImpl::IsCapturingAudio() {
  return BrowserMainLoop::GetAudioManager()->IsRecordingInProcess();
}

string16 SpeechRecognitionManagerImpl::GetAudioInputDeviceModel() {
  return BrowserMainLoop::GetAudioManager()->GetAudioInputDeviceModel();
}

bool SpeechRecognitionManagerImpl::HasPendingRequest(int caller_id) const {
  return requests_.find(caller_id) != requests_.end();
}

InputTagSpeechDispatcherHost* SpeechRecognitionManagerImpl::GetDelegate(
    int caller_id) const {
  return requests_.find(caller_id)->second.delegate;
}

void SpeechRecognitionManagerImpl::ShowAudioInputSettings() {
  // Since AudioManager::ShowAudioInputSettings can potentially launch external
  // processes, do that in the FILE thread to not block the calling threads.
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&SpeechRecognitionManagerImpl::ShowAudioInputSettings,
                   base::Unretained(this)));
    return;
  }

  media::AudioManager* audio_manager = BrowserMainLoop::GetAudioManager();
  DCHECK(audio_manager->CanShowAudioInputSettings());
  if (audio_manager->CanShowAudioInputSettings())
    audio_manager->ShowAudioInputSettings();
}

void SpeechRecognitionManagerImpl::StartRecognition(
    InputTagSpeechDispatcherHost* delegate,
    int caller_id,
    int render_process_id,
    int render_view_id,
    const gfx::Rect& element_rect,
    const std::string& language,
    const std::string& grammar,
    const std::string& origin_url,
    net::URLRequestContextGetter* context_getter,
    content::SpeechRecognitionPreferences* recognition_prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &SpeechRecognitionManagerImpl::CheckRenderViewTypeAndStartRecognition,
          base::Unretained(this),
          SpeechRecognitionParams(
              delegate, caller_id, render_process_id, render_view_id,
              element_rect, language, grammar, origin_url, context_getter,
              recognition_prefs)));
}

void SpeechRecognitionManagerImpl::CheckRenderViewTypeAndStartRecognition(
    const SpeechRecognitionParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderViewHostImpl* render_view_host = RenderViewHostImpl::FromID(
      params.render_process_id, params.render_view_id);
  if (!render_view_host || !render_view_host->GetDelegate())
    return;

  // For host delegates other than TabContents we can't reliably show a popup,
  // including the speech input bubble. In these cases for privacy reasons we
  // don't want to start recording if the user can't be properly notified.
  // An example of this is trying to show the speech input bubble within an
  // extension popup: http://crbug.com/92083. In these situations the speech
  // input extension API should be used instead.
  if (render_view_host->GetDelegate()->GetRenderViewType() ==
      content::VIEW_TYPE_TAB_CONTENTS) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SpeechRecognitionManagerImpl::ProceedStartingRecognition,
        base::Unretained(this), params));
  }
}

void SpeechRecognitionManagerImpl::ProceedStartingRecognition(
    const SpeechRecognitionParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!HasPendingRequest(params.caller_id));

  if (delegate_.get()) {
    delegate_->ShowRecognitionRequested(
        params.caller_id, params.render_process_id, params.render_view_id,
        params.element_rect);
    delegate_->GetRequestInfo(&can_report_metrics_, &request_info_);
  }

  Request* request = &requests_[params.caller_id];
  request->delegate = params.delegate;
  request->recognizer = content::SpeechRecognizer::Create(
      this, params.caller_id, params.language, params.grammar,
      params.context_getter, params.recognition_prefs->FilterProfanities(),
      request_info_, can_report_metrics_ ? params.origin_url : "");
  request->is_active = false;

  StartRecognitionForRequest(params.caller_id);
}

void SpeechRecognitionManagerImpl::StartRecognitionForRequest(int caller_id) {
  SpeechRecognizerMap::iterator request = requests_.find(caller_id);
  if (request == requests_.end()) {
    NOTREACHED();
    return;
  }

  // We should not currently be recording for the caller.
  CHECK(recording_caller_id_ != caller_id);

  // If we are currently recording audio for another caller, abort that cleanly.
  if (recording_caller_id_)
    CancelRecognitionAndInformDelegate(recording_caller_id_);
  recording_caller_id_ = caller_id;
  requests_[caller_id].is_active = true;
  requests_[caller_id].recognizer->StartRecognition();
  if (delegate_.get())
    delegate_->ShowWarmUp(caller_id);
}

void SpeechRecognitionManagerImpl::CancelRecognitionForRequest(int caller_id) {
  // Ignore if the caller id was not in our active recognizers list because the
  // user might have clicked more than once, or recognition could have been
  // ended due to other reasons before the user click was processed.
  if (!HasPendingRequest(caller_id))
    return;

  CancelRecognitionAndInformDelegate(caller_id);
}

void SpeechRecognitionManagerImpl::FocusLostForRequest(int caller_id) {
  // See above comment.
  if (!HasPendingRequest(caller_id))
    return;

  // If this is an ongoing recording or if we were displaying an error message
  // to the user, abort it since user has switched focus. Otherwise
  // recognition has started and keep that going so user can start speaking to
  // another element while this gets the results in parallel.
  if (recording_caller_id_ == caller_id || !requests_[caller_id].is_active)
    CancelRecognitionAndInformDelegate(caller_id);
}

void SpeechRecognitionManagerImpl::CancelRecognition(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  if (requests_[caller_id].is_active)
    requests_[caller_id].recognizer->AbortRecognition();
  requests_.erase(caller_id);
  if (recording_caller_id_ == caller_id)
    recording_caller_id_ = 0;
  if (delegate_.get())
    delegate_->DoClose(caller_id);
}

void SpeechRecognitionManagerImpl::CancelAllRequestsWithDelegate(
    InputTagSpeechDispatcherHost* delegate) {
  SpeechRecognizerMap::iterator it = requests_.begin();
  while (it != requests_.end()) {
    if (it->second.delegate == delegate) {
      CancelRecognition(it->first);
      // This map will have very few elements so it is simpler to restart.
      it = requests_.begin();
    } else {
      ++it;
    }
  }
}

void SpeechRecognitionManagerImpl::StopRecording(int caller_id) {
  // No pending requests on extension popups.
  if (!HasPendingRequest(caller_id))
    return;

  requests_[caller_id].recognizer->StopAudioCapture();
}

// -------- SpeechRecognitionEventListener interface implementation. ---------

void SpeechRecognitionManagerImpl::OnRecognitionResult(
    int caller_id, const content::SpeechRecognitionResult& result) {
  DCHECK(HasPendingRequest(caller_id));
  GetDelegate(caller_id)->SetRecognitionResult(caller_id, result);
}

void SpeechRecognitionManagerImpl::OnAudioEnd(int caller_id) {
  if (recording_caller_id_ != caller_id)
    return;
  DCHECK_EQ(recording_caller_id_, caller_id);
  DCHECK(HasPendingRequest(caller_id));
  if (!requests_[caller_id].is_active)
    return;
  recording_caller_id_ = 0;
  GetDelegate(caller_id)->DidCompleteRecording(caller_id);
  if (delegate_.get())
    delegate_->ShowRecognizing(caller_id);
}

void SpeechRecognitionManagerImpl::OnRecognitionEnd(int caller_id) {
  if (!HasPendingRequest(caller_id) || !requests_[caller_id].is_active)
    return;
  GetDelegate(caller_id)->DidCompleteRecognition(caller_id);
  requests_.erase(caller_id);
  if (delegate_.get())
    delegate_->DoClose(caller_id);
}

void SpeechRecognitionManagerImpl::OnSoundStart(int caller_id) {
}

void SpeechRecognitionManagerImpl::OnSoundEnd(int caller_id) {
}

void SpeechRecognitionManagerImpl::OnRecognitionError(
    int caller_id, const content::SpeechRecognitionError& error) {
  DCHECK(HasPendingRequest(caller_id));
  if (caller_id == recording_caller_id_)
    recording_caller_id_ = 0;
  requests_[caller_id].is_active = false;
  if (delegate_.get()) {
    if (error.code == content::SPEECH_RECOGNITION_ERROR_AUDIO &&
        error.details == content::SPEECH_AUDIO_ERROR_DETAILS_NO_MIC) {
      delegate_->ShowMicError(caller_id,
          SpeechRecognitionManagerDelegate::MIC_ERROR_NO_DEVICE_AVAILABLE);
    } else if (error.code == content::SPEECH_RECOGNITION_ERROR_AUDIO &&
               error.details == content::SPEECH_AUDIO_ERROR_DETAILS_IN_USE) {
      delegate_->ShowMicError(
          caller_id, SpeechRecognitionManagerDelegate::MIC_ERROR_DEVICE_IN_USE);
    } else {
      delegate_->ShowRecognizerError(caller_id, error.code);
    }
  }
}

void SpeechRecognitionManagerImpl::OnAudioStart(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  DCHECK_EQ(recording_caller_id_, caller_id);
  if (delegate_.get())
    delegate_->ShowRecording(caller_id);
}

void SpeechRecognitionManagerImpl::OnRecognitionStart(int caller_id) {
}

void SpeechRecognitionManagerImpl::OnEnvironmentEstimationComplete(
    int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  DCHECK_EQ(recording_caller_id_, caller_id);
}

void SpeechRecognitionManagerImpl::OnAudioLevelsChange(
    int caller_id, float volume, float noise_volume) {
  DCHECK(HasPendingRequest(caller_id));
  DCHECK_EQ(recording_caller_id_, caller_id);
  if (delegate_.get())
    delegate_->ShowInputVolume(caller_id, volume, noise_volume);
}

void SpeechRecognitionManagerImpl::CancelRecognitionAndInformDelegate(
    int caller_id) {
  InputTagSpeechDispatcherHost* cur_delegate = GetDelegate(caller_id);
  CancelRecognition(caller_id);
  cur_delegate->DidCompleteRecording(caller_id);
  cur_delegate->DidCompleteRecognition(caller_id);
}

SpeechRecognitionManagerImpl::Request::Request()
    : is_active(false) {
}

SpeechRecognitionManagerImpl::Request::~Request() {
}

}  // namespace speech
