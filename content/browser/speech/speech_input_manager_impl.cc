// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_input_manager_impl.h"

#include "base/bind.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/speech/speech_input_dispatcher_host.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/speech_input_manager_delegate.h"
#include "content/public/browser/speech_input_preferences.h"
#include "content/public/common/view_type.h"

using content::BrowserMainLoop;
using content::BrowserThread;
using content::SpeechInputManagerDelegate;

content::SpeechInputManager* content::SpeechInputManager::GetInstance() {
  return speech_input::SpeechInputManagerImpl::GetInstance();
}

namespace speech_input {

struct SpeechInputManagerImpl::SpeechInputParams {
  SpeechInputParams(SpeechInputDispatcherHost* delegate,
                    int caller_id,
                    int render_process_id,
                    int render_view_id,
                    const gfx::Rect& element_rect,
                    const std::string& language,
                    const std::string& grammar,
                    const std::string& origin_url,
                    net::URLRequestContextGetter* context_getter,
                    content::SpeechInputPreferences* speech_input_prefs)
      : delegate(delegate),
        caller_id(caller_id),
        render_process_id(render_process_id),
        render_view_id(render_view_id),
        element_rect(element_rect),
        language(language),
        grammar(grammar),
        origin_url(origin_url),
        context_getter(context_getter),
        speech_input_prefs(speech_input_prefs) {
  }

  SpeechInputDispatcherHost* delegate;
  int caller_id;
  int render_process_id;
  int render_view_id;
  gfx::Rect element_rect;
  std::string language;
  std::string grammar;
  std::string origin_url;
  net::URLRequestContextGetter* context_getter;
  content::SpeechInputPreferences* speech_input_prefs;
};

SpeechInputManagerImpl* SpeechInputManagerImpl::GetInstance() {
  return Singleton<SpeechInputManagerImpl>::get();
}

SpeechInputManagerImpl::SpeechInputManagerImpl()
    : can_report_metrics_(false),
      recording_caller_id_(0) {
  delegate_ =
      content::GetContentClient()->browser()->GetSpeechInputManagerDelegate();
}

SpeechInputManagerImpl::~SpeechInputManagerImpl() {
  while (requests_.begin() != requests_.end())
    CancelRecognition(requests_.begin()->first);
}

bool SpeechInputManagerImpl::HasAudioInputDevices() {
  return BrowserMainLoop::GetAudioManager()->HasAudioInputDevices();
}

bool SpeechInputManagerImpl::IsRecordingInProcess() {
  return BrowserMainLoop::GetAudioManager()->IsRecordingInProcess();
}

string16 SpeechInputManagerImpl::GetAudioInputDeviceModel() {
  return BrowserMainLoop::GetAudioManager()->GetAudioInputDeviceModel();
}

bool SpeechInputManagerImpl::HasPendingRequest(int caller_id) const {
  return requests_.find(caller_id) != requests_.end();
}

SpeechInputDispatcherHost* SpeechInputManagerImpl::GetDelegate(
    int caller_id) const {
  return requests_.find(caller_id)->second.delegate;
}

void SpeechInputManagerImpl::ShowAudioInputSettings() {
  // Since AudioManager::ShowAudioInputSettings can potentially launch external
  // processes, do that in the FILE thread to not block the calling threads.
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&SpeechInputManagerImpl::ShowAudioInputSettings,
                   base::Unretained(this)));
    return;
  }

  AudioManager* audio_manager = BrowserMainLoop::GetAudioManager();
  DCHECK(audio_manager->CanShowAudioInputSettings());
  if (audio_manager->CanShowAudioInputSettings())
    audio_manager->ShowAudioInputSettings();
}

void SpeechInputManagerImpl::StartRecognition(
    SpeechInputDispatcherHost* delegate,
    int caller_id,
    int render_process_id,
    int render_view_id,
    const gfx::Rect& element_rect,
    const std::string& language,
    const std::string& grammar,
    const std::string& origin_url,
    net::URLRequestContextGetter* context_getter,
    content::SpeechInputPreferences* speech_input_prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &SpeechInputManagerImpl::CheckRenderViewTypeAndStartRecognition,
          base::Unretained(this),
          SpeechInputParams(
              delegate, caller_id, render_process_id, render_view_id,
              element_rect, language, grammar, origin_url, context_getter,
              speech_input_prefs)));
}

void SpeechInputManagerImpl::CheckRenderViewTypeAndStartRecognition(
    const SpeechInputParams& params) {
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
        base::Bind(&SpeechInputManagerImpl::ProceedStartingRecognition,
        base::Unretained(this), params));
  }
}

void SpeechInputManagerImpl::ProceedStartingRecognition(
    const SpeechInputParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!HasPendingRequest(params.caller_id));

  if (delegate_) {
    delegate_->ShowRecognitionRequested(
        params.caller_id, params.render_process_id, params.render_view_id,
        params.element_rect);
    delegate_->GetRequestInfo(&can_report_metrics_, &request_info_);
  }

  SpeechInputRequest* request = &requests_[params.caller_id];
  request->delegate = params.delegate;
  request->recognizer = new SpeechRecognizerImpl(
      this, params.caller_id, params.language, params.grammar,
      params.context_getter, params.speech_input_prefs->FilterProfanities(),
      request_info_, can_report_metrics_ ? params.origin_url : "");
  request->is_active = false;

  StartRecognitionForRequest(params.caller_id);
}

void SpeechInputManagerImpl::StartRecognitionForRequest(int caller_id) {
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

  if (!HasAudioInputDevices()) {
    if (delegate_) {
      delegate_->ShowMicError(
          caller_id, SpeechInputManagerDelegate::MIC_ERROR_NO_DEVICE_AVAILABLE);
    }
  } else if (IsRecordingInProcess()) {
    if (delegate_) {
      delegate_->ShowMicError(
          caller_id, SpeechInputManagerDelegate::MIC_ERROR_DEVICE_IN_USE);
    }
  } else {
    recording_caller_id_ = caller_id;
    requests_[caller_id].is_active = true;
    requests_[caller_id].recognizer->StartRecording();
    if (delegate_)
      delegate_->ShowWarmUp(caller_id);
  }
}

void SpeechInputManagerImpl::CancelRecognitionForRequest(int caller_id) {
  // Ignore if the caller id was not in our active recognizers list because the
  // user might have clicked more than once, or recognition could have been
  // ended due to other reasons before the user click was processed.
  if (!HasPendingRequest(caller_id))
    return;

  CancelRecognitionAndInformDelegate(caller_id);
}

void SpeechInputManagerImpl::FocusLostForRequest(int caller_id) {
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

void SpeechInputManagerImpl::CancelRecognition(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  if (requests_[caller_id].is_active)
    requests_[caller_id].recognizer->CancelRecognition();
  requests_.erase(caller_id);
  if (recording_caller_id_ == caller_id)
    recording_caller_id_ = 0;
  if (delegate_)
    delegate_->DoClose(caller_id);
}

void SpeechInputManagerImpl::CancelAllRequestsWithDelegate(
    SpeechInputDispatcherHost* delegate) {
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

void SpeechInputManagerImpl::StopRecording(int caller_id) {
  // No pending requests on extension popups.
  if (!HasPendingRequest(caller_id))
    return;

  requests_[caller_id].recognizer->StopRecording();
}

void SpeechInputManagerImpl::SetRecognitionResult(
    int caller_id, const content::SpeechInputResult& result) {
  DCHECK(HasPendingRequest(caller_id));
  GetDelegate(caller_id)->SetRecognitionResult(caller_id, result);
}

void SpeechInputManagerImpl::DidCompleteRecording(int caller_id) {
  DCHECK(recording_caller_id_ == caller_id);
  DCHECK(HasPendingRequest(caller_id));
  recording_caller_id_ = 0;
  GetDelegate(caller_id)->DidCompleteRecording(caller_id);
  if (delegate_)
    delegate_->ShowRecognizing(caller_id);
}

void SpeechInputManagerImpl::DidCompleteRecognition(int caller_id) {
  GetDelegate(caller_id)->DidCompleteRecognition(caller_id);
  requests_.erase(caller_id);
  if (delegate_)
    delegate_->DoClose(caller_id);
}

void SpeechInputManagerImpl::DidStartReceivingSpeech(int caller_id) {
}

void SpeechInputManagerImpl::DidStopReceivingSpeech(int caller_id) {
}

void SpeechInputManagerImpl::OnRecognizerError(
    int caller_id, content::SpeechInputError error) {
  if (caller_id == recording_caller_id_)
    recording_caller_id_ = 0;
  requests_[caller_id].is_active = false;
  if (delegate_)
    delegate_->ShowRecognizerError(caller_id, error);
}

void SpeechInputManagerImpl::DidStartReceivingAudio(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  DCHECK(recording_caller_id_ == caller_id);
  if (delegate_)
    delegate_->ShowRecording(caller_id);
}

void SpeechInputManagerImpl::DidCompleteEnvironmentEstimation(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  DCHECK(recording_caller_id_ == caller_id);
}

void SpeechInputManagerImpl::SetInputVolume(int caller_id, float volume,
                                            float noise_volume) {
  DCHECK(HasPendingRequest(caller_id));
  DCHECK_EQ(recording_caller_id_, caller_id);
  if (delegate_)
    delegate_->ShowInputVolume(caller_id, volume, noise_volume);
}

void SpeechInputManagerImpl::CancelRecognitionAndInformDelegate(
    int caller_id) {
  SpeechInputDispatcherHost* cur_delegate = GetDelegate(caller_id);
  CancelRecognition(caller_id);
  cur_delegate->DidCompleteRecording(caller_id);
  cur_delegate->DidCompleteRecognition(caller_id);
}

SpeechInputManagerImpl::SpeechInputRequest::SpeechInputRequest()
    : delegate(NULL),
      is_active(false) {
}

SpeechInputManagerImpl::SpeechInputRequest::~SpeechInputRequest() {
}

}  // namespace speech_input
