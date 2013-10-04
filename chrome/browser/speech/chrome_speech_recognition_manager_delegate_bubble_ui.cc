// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate_bubble_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/common/speech_recognition_error.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::SpeechRecognitionManager;

namespace {

bool RequiresBubble(int session_id) {
  return SpeechRecognitionManager::GetInstance()->
      GetSessionContext(session_id).requested_by_page_element;
}

}  // namespace

namespace speech {

ChromeSpeechRecognitionManagerDelegateBubbleUI
::ChromeSpeechRecognitionManagerDelegateBubbleUI() {
}

ChromeSpeechRecognitionManagerDelegateBubbleUI
::~ChromeSpeechRecognitionManagerDelegateBubbleUI() {
  if (bubble_controller_.get())
    bubble_controller_->CloseBubble();
}

void ChromeSpeechRecognitionManagerDelegateBubbleUI::InfoBubbleButtonClicked(
    int session_id, SpeechRecognitionBubble::Button button) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Note, the session might have been destroyed, therefore avoid calls to the
  // manager which imply its existance (e.g., GetSessionContext()).

  if (button == SpeechRecognitionBubble::BUTTON_CANCEL) {
    GetBubbleController()->CloseBubble();
    last_session_config_.reset();

    // We can safely call AbortSession even if the session has already ended,
    // the manager's public methods are reliable and will handle it properly.
    SpeechRecognitionManager::GetInstance()->AbortSession(session_id);
  } else if (button == SpeechRecognitionBubble::BUTTON_TRY_AGAIN) {
    GetBubbleController()->CloseBubble();
    RestartLastSession();
  } else {
    NOTREACHED();
  }
}

void ChromeSpeechRecognitionManagerDelegateBubbleUI::InfoBubbleFocusChanged(
    int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // This check is needed since on some systems (MacOS), in rare cases, if the
  // user clicks repeatedly and fast on the input element, the FocusChanged
  // event (corresponding to the old session that should be aborted) can be
  // received after a new session (corresponding to the 2nd click) is started.
  if (GetBubbleController()->GetActiveSessionID() != session_id)
    return;

  // Note, the session might have been destroyed, therefore avoid calls to the
  // manager which imply its existance (e.g., GetSessionContext()).
  GetBubbleController()->CloseBubble();
  last_session_config_.reset();

  // Clicking outside the bubble means we should abort.
  SpeechRecognitionManager::GetInstance()->AbortSession(session_id);
}

void ChromeSpeechRecognitionManagerDelegateBubbleUI::OnRecognitionStart(
    int session_id) {
  ChromeSpeechRecognitionManagerDelegate::OnRecognitionStart(session_id);

  const content::SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);

  if (RequiresBubble(session_id)) {
    // Copy the configuration of the session (for the "try again" button).
    last_session_config_.reset(new content::SpeechRecognitionSessionConfig(
        SpeechRecognitionManager::GetInstance()->GetSessionConfig(session_id)));

    // Create and show the bubble. It will be closed upon the OnEnd event.
    GetBubbleController()->CreateBubble(session_id,
                                        context.render_process_id,
                                        context.render_view_id,
                                        context.element_rect);
  }
}

void ChromeSpeechRecognitionManagerDelegateBubbleUI::OnAudioStart(
    int session_id) {
  ChromeSpeechRecognitionManagerDelegate::OnAudioStart(session_id);

  if (RequiresBubble(session_id)) {
    DCHECK_EQ(session_id, GetBubbleController()->GetActiveSessionID());
    GetBubbleController()->SetBubbleRecordingMode();
  }
}

void ChromeSpeechRecognitionManagerDelegateBubbleUI::OnAudioEnd(
    int session_id) {
  ChromeSpeechRecognitionManagerDelegate::OnAudioEnd(session_id);

  // OnAudioEnd can be also raised after an abort, when the bubble has already
  // been closed.
  if (GetBubbleController()->GetActiveSessionID() == session_id) {
    DCHECK(RequiresBubble(session_id));
    GetBubbleController()->SetBubbleRecognizingMode();
  }
}

void ChromeSpeechRecognitionManagerDelegateBubbleUI::OnRecognitionError(
    int session_id, const content::SpeechRecognitionError& error) {
  ChromeSpeechRecognitionManagerDelegate::OnRecognitionError(session_id, error);

  // An error can be dispatched when the bubble is not visible anymore.
  if (GetBubbleController()->GetActiveSessionID() != session_id)
    return;
  DCHECK(RequiresBubble(session_id));

  int error_message_id = 0;
  switch (error.code) {
    case content::SPEECH_RECOGNITION_ERROR_AUDIO:
      switch (error.details) {
        case content::SPEECH_AUDIO_ERROR_DETAILS_NO_MIC:
          error_message_id = IDS_SPEECH_INPUT_NO_MIC;
          break;
        default:
          error_message_id = IDS_SPEECH_INPUT_MIC_ERROR;
          break;
      }
      break;
    case content::SPEECH_RECOGNITION_ERROR_ABORTED:
      error_message_id = IDS_SPEECH_INPUT_ABORTED;
      break;
    case content::SPEECH_RECOGNITION_ERROR_NO_SPEECH:
      error_message_id = IDS_SPEECH_INPUT_NO_SPEECH;
      break;
    case content::SPEECH_RECOGNITION_ERROR_NO_MATCH:
      error_message_id = IDS_SPEECH_INPUT_NO_RESULTS;
      break;
    case content::SPEECH_RECOGNITION_ERROR_NETWORK:
      error_message_id = IDS_SPEECH_INPUT_NET_ERROR;
      break;
    default:
      NOTREACHED() << "unknown error " << error.code;
      return;
  }
  GetBubbleController()->SetBubbleMessage(
      l10n_util::GetStringUTF16(error_message_id));

}

void ChromeSpeechRecognitionManagerDelegateBubbleUI::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {
  ChromeSpeechRecognitionManagerDelegate::OnAudioLevelsChange(
      session_id, volume, noise_volume);

  if (GetBubbleController()->GetActiveSessionID() == session_id) {
    DCHECK(RequiresBubble(session_id));
    GetBubbleController()->SetBubbleInputVolume(volume, noise_volume);
  }
}

void ChromeSpeechRecognitionManagerDelegateBubbleUI::OnRecognitionEnd(
    int session_id) {
  ChromeSpeechRecognitionManagerDelegate::OnRecognitionEnd(session_id);

  // The only case in which the OnRecognitionEnd should not close the bubble is
  // when we are showing an error. In this case the bubble will be closed by
  // the |InfoBubbleFocusChanged| method, when the users clicks either the
  // "Cancel" button or outside of the bubble.
  if (GetBubbleController()->GetActiveSessionID() == session_id &&
      !GetBubbleController()->IsShowingMessage()) {
    DCHECK(RequiresBubble(session_id));
    GetBubbleController()->CloseBubble();
  }
}

void ChromeSpeechRecognitionManagerDelegateBubbleUI::TabClosedCallback(
    int render_process_id, int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ChromeSpeechRecognitionManagerDelegate::TabClosedCallback(
      render_process_id, render_view_id);

  // Avoid instantiating a bubble_controller_ if not needed. Thus, prefer a
  // checked access to bubble_controller_ to GetBubbleController().
  if (bubble_controller_.get())
    bubble_controller_->CloseBubbleForRenderViewOnUIThread(render_process_id,
                                                           render_view_id);
}

SpeechRecognitionBubbleController*
ChromeSpeechRecognitionManagerDelegateBubbleUI::GetBubbleController() {
  if (!bubble_controller_.get())
    bubble_controller_ = new SpeechRecognitionBubbleController(this);
  return bubble_controller_.get();
}

void ChromeSpeechRecognitionManagerDelegateBubbleUI::RestartLastSession() {
  DCHECK(last_session_config_.get());
  SpeechRecognitionManager* manager = SpeechRecognitionManager::GetInstance();
  const int new_session_id = manager->CreateSession(*last_session_config_);
  DCHECK_NE(SpeechRecognitionManager::kSessionIDInvalid, new_session_id);
  last_session_config_.reset();
  manager->StartSession(new_session_id);
}

}  // namespace speech
