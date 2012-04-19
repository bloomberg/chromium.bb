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
      int session_id,
      int render_process_id,
      int render_view_id,
      const gfx::Rect& element_rect,
      const std::string& language,
      const std::string& grammar,
      const std::string& origin_url,
      net::URLRequestContextGetter* context_getter,
      content::SpeechRecognitionPreferences* recognition_prefs)
      : delegate(delegate),
        session_id(session_id),
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
  int session_id;
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
      recording_session_id_(0) {
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

bool SpeechRecognitionManagerImpl::HasPendingRequest(int session_id) const {
  return requests_.find(session_id) != requests_.end();
}

InputTagSpeechDispatcherHost* SpeechRecognitionManagerImpl::GetDelegate(
    int session_id) const {
  return requests_.find(session_id)->second.delegate;
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
    int session_id,
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
              delegate, session_id, render_process_id, render_view_id,
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

  // For host delegates other than VIEW_TYPE_WEB_CONTENTS we can't reliably show
  // a popup, including the speech input bubble. In these cases for privacy
  // reasons we don't want to start recording if the user can't be properly
  // notified. An example of this is trying to show the speech input bubble
  // within an extension popup: http://crbug.com/92083. In these situations the
  // speech input extension API should be used instead.
  if (render_view_host->GetDelegate()->GetRenderViewType() ==
      content::VIEW_TYPE_WEB_CONTENTS) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SpeechRecognitionManagerImpl::ProceedStartingRecognition,
        base::Unretained(this), params));
  }
}

void SpeechRecognitionManagerImpl::ProceedStartingRecognition(
    const SpeechRecognitionParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!HasPendingRequest(params.session_id));

  if (delegate_.get()) {
    delegate_->ShowRecognitionRequested(
        params.session_id, params.render_process_id, params.render_view_id,
        params.element_rect);
    delegate_->GetRequestInfo(&can_report_metrics_, &request_info_);
  }

  Request* request = &requests_[params.session_id];
  request->delegate = params.delegate;
  request->recognizer = content::SpeechRecognizer::Create(
      this, params.session_id, params.language, params.grammar,
      params.context_getter, params.recognition_prefs->FilterProfanities(),
      request_info_, can_report_metrics_ ? params.origin_url : "");
  request->is_active = false;

  StartRecognitionForRequest(params.session_id);
}

void SpeechRecognitionManagerImpl::StartRecognitionForRequest(int session_id) {
  SpeechRecognizerMap::iterator request = requests_.find(session_id);
  if (request == requests_.end()) {
    NOTREACHED();
    return;
  }

  // We should not currently be recording for the session.
  CHECK(recording_session_id_ != session_id);

  // If we are currently recording audio for another session, abort it cleanly.
  if (recording_session_id_)
    CancelRecognitionAndInformDelegate(recording_session_id_);
  recording_session_id_ = session_id;
  requests_[session_id].is_active = true;
  requests_[session_id].recognizer->StartRecognition();
  if (delegate_.get())
    delegate_->ShowWarmUp(session_id);
}

void SpeechRecognitionManagerImpl::CancelRecognitionForRequest(int session_id) {
  // Ignore if the session id was not in our active recognizers list because the
  // user might have clicked more than once, or recognition could have been
  // ended due to other reasons before the user click was processed.
  if (!HasPendingRequest(session_id))
    return;

  CancelRecognitionAndInformDelegate(session_id);
}

void SpeechRecognitionManagerImpl::FocusLostForRequest(int session_id) {
  // See above comment.
  if (!HasPendingRequest(session_id))
    return;

  // If this is an ongoing recording or if we were displaying an error message
  // to the user, abort it since user has switched focus. Otherwise
  // recognition has started and keep that going so user can start speaking to
  // another element while this gets the results in parallel.
  if (recording_session_id_ == session_id || !requests_[session_id].is_active)
    CancelRecognitionAndInformDelegate(session_id);
}

void SpeechRecognitionManagerImpl::CancelRecognition(int session_id) {
  DCHECK(HasPendingRequest(session_id));
  if (requests_[session_id].is_active)
    requests_[session_id].recognizer->AbortRecognition();
  requests_.erase(session_id);
  if (recording_session_id_ == session_id)
    recording_session_id_ = 0;
  if (delegate_.get())
    delegate_->DoClose(session_id);
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

void SpeechRecognitionManagerImpl::StopRecording(int session_id) {
  // No pending requests on extension popups.
  if (!HasPendingRequest(session_id))
    return;

  requests_[session_id].recognizer->StopAudioCapture();
}

// -------- SpeechRecognitionEventListener interface implementation. ---------

void SpeechRecognitionManagerImpl::OnRecognitionResult(
    int session_id, const content::SpeechRecognitionResult& result) {
  DCHECK(HasPendingRequest(session_id));
  GetDelegate(session_id)->SetRecognitionResult(session_id, result);
}

void SpeechRecognitionManagerImpl::OnAudioEnd(int session_id) {
  if (recording_session_id_ != session_id)
    return;
  DCHECK_EQ(recording_session_id_, session_id);
  DCHECK(HasPendingRequest(session_id));
  if (!requests_[session_id].is_active)
    return;
  recording_session_id_ = 0;
  GetDelegate(session_id)->DidCompleteRecording(session_id);
  if (delegate_.get())
    delegate_->ShowRecognizing(session_id);
}

void SpeechRecognitionManagerImpl::OnRecognitionEnd(int session_id) {
  if (!HasPendingRequest(session_id) || !requests_[session_id].is_active)
    return;
  GetDelegate(session_id)->DidCompleteRecognition(session_id);
  requests_.erase(session_id);
  if (delegate_.get())
    delegate_->DoClose(session_id);
}

void SpeechRecognitionManagerImpl::OnSoundStart(int session_id) {
}

void SpeechRecognitionManagerImpl::OnSoundEnd(int session_id) {
}

void SpeechRecognitionManagerImpl::OnRecognitionError(
    int session_id, const content::SpeechRecognitionError& error) {
  DCHECK(HasPendingRequest(session_id));
  if (session_id == recording_session_id_)
    recording_session_id_ = 0;
  requests_[session_id].is_active = false;
  if (delegate_.get()) {
    if (error.code == content::SPEECH_RECOGNITION_ERROR_AUDIO &&
        error.details == content::SPEECH_AUDIO_ERROR_DETAILS_NO_MIC) {
      delegate_->ShowMicError(session_id,
          SpeechRecognitionManagerDelegate::MIC_ERROR_NO_DEVICE_AVAILABLE);
    } else if (error.code == content::SPEECH_RECOGNITION_ERROR_AUDIO &&
               error.details == content::SPEECH_AUDIO_ERROR_DETAILS_IN_USE) {
      delegate_->ShowMicError(session_id,
          SpeechRecognitionManagerDelegate::MIC_ERROR_DEVICE_IN_USE);
    } else {
      delegate_->ShowRecognizerError(session_id, error.code);
    }
  }
}

void SpeechRecognitionManagerImpl::OnAudioStart(int session_id) {
  DCHECK(HasPendingRequest(session_id));
  DCHECK_EQ(recording_session_id_, session_id);
  if (delegate_.get())
    delegate_->ShowRecording(session_id);
}

void SpeechRecognitionManagerImpl::OnRecognitionStart(int session_id) {
}

void SpeechRecognitionManagerImpl::OnEnvironmentEstimationComplete(
    int session_id) {
  DCHECK(HasPendingRequest(session_id));
  DCHECK_EQ(recording_session_id_, session_id);
}

void SpeechRecognitionManagerImpl::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {
  DCHECK(HasPendingRequest(session_id));
  DCHECK_EQ(recording_session_id_, session_id);
  if (delegate_.get())
    delegate_->ShowInputVolume(session_id, volume, noise_volume);
}

void SpeechRecognitionManagerImpl::CancelRecognitionAndInformDelegate(
    int session_id) {
  InputTagSpeechDispatcherHost* cur_delegate = GetDelegate(session_id);
  CancelRecognition(session_id);
  cur_delegate->DidCompleteRecording(session_id);
  cur_delegate->DidCompleteRecognition(session_id);
}

SpeechRecognitionManagerImpl::Request::Request()
    : is_active(false) {
}

SpeechRecognitionManagerImpl::Request::~Request() {
}

}  // namespace speech
