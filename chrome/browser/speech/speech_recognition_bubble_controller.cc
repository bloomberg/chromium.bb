// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_bubble_controller.h"

#include "base/bind.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/rect.h"

using content::BrowserThread;
using content::WebContents;

namespace speech {

SpeechRecognitionBubbleController::SpeechRecognitionBubbleController(
    Delegate* delegate)
    : delegate_(delegate),
      current_bubble_session_id_(0),
      registrar_(new content::NotificationRegistrar) {
}

void SpeechRecognitionBubbleController::CreateBubble(
    int session_id,
    int render_process_id,
    int render_view_id,
    const gfx::Rect& element_rect) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SpeechRecognitionBubbleController::CreateBubble, this,
                   session_id, render_process_id, render_view_id,
                   element_rect));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebContents* web_contents = tab_util::GetWebContentsByID(render_process_id,
                                                           render_view_id);

  DCHECK_EQ(0u, bubbles_.count(session_id));
  SpeechRecognitionBubble* bubble = SpeechRecognitionBubble::Create(
      web_contents, this, element_rect);
  if (!bubble) {
    // Could be null if tab or display rect were invalid.
    // Simulate the cancel button being clicked to inform the delegate.
    BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &SpeechRecognitionBubbleController::InvokeDelegateButtonClicked,
              this, session_id, SpeechRecognitionBubble::BUTTON_CANCEL));
    return;
  }

  bubbles_[session_id] = bubble;

  UpdateTabContentsSubscription(session_id, BUBBLE_ADDED);
}

void SpeechRecognitionBubbleController::SetBubbleWarmUpMode(int session_id) {
  ProcessRequestInUiThread(session_id, REQUEST_SET_WARM_UP_MODE,
                           string16(), 0, 0);
}

void SpeechRecognitionBubbleController::SetBubbleRecordingMode(int session_id) {
  ProcessRequestInUiThread(session_id, REQUEST_SET_RECORDING_MODE,
                           string16(), 0, 0);
}

void SpeechRecognitionBubbleController::SetBubbleRecognizingMode(
    int session_id) {
  ProcessRequestInUiThread(session_id, REQUEST_SET_RECOGNIZING_MODE,
                           string16(), 0, 0);
}

void SpeechRecognitionBubbleController::SetBubbleMessage(int session_id,
                                                         const string16& text) {
  ProcessRequestInUiThread(session_id, REQUEST_SET_MESSAGE, text, 0, 0);
}

void SpeechRecognitionBubbleController::SetBubbleInputVolume(
    int session_id, float volume, float noise_volume) {
  ProcessRequestInUiThread(session_id, REQUEST_SET_INPUT_VOLUME, string16(),
                           volume, noise_volume);
}

void SpeechRecognitionBubbleController::CloseBubble(int session_id) {
  ProcessRequestInUiThread(session_id, REQUEST_CLOSE, string16(), 0, 0);
}

void SpeechRecognitionBubbleController::InfoBubbleButtonClicked(
    SpeechRecognitionBubble::Button button) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(current_bubble_session_id_);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &SpeechRecognitionBubbleController::InvokeDelegateButtonClicked,
          this, current_bubble_session_id_, button));
}

void SpeechRecognitionBubbleController::InfoBubbleFocusChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(current_bubble_session_id_);

  int old_bubble_session_id = current_bubble_session_id_;
  current_bubble_session_id_ = 0;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &SpeechRecognitionBubbleController::InvokeDelegateFocusChanged,
          this, old_bubble_session_id));
}

void SpeechRecognitionBubbleController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED) {
    // Cancel all bubbles and active recognition sessions for this tab.
    WebContents* web_contents = content::Source<WebContents>(source).ptr();
    BubbleSessionIdMap::iterator iter = bubbles_.begin();
    while (iter != bubbles_.end()) {
      if (iter->second->GetWebContents() == web_contents) {
        BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
            base::Bind(
                &SpeechRecognitionBubbleController::InvokeDelegateButtonClicked,
                this, iter->first, SpeechRecognitionBubble::BUTTON_CANCEL));
        CloseBubble(iter->first);
        // We expect to have a very small number of items in this map so
        // redo-ing from start is ok.
        iter = bubbles_.begin();
      } else {
        ++iter;
      }
    }
  } else {
    NOTREACHED() << "Unknown notification";
  }
}

SpeechRecognitionBubbleController::~SpeechRecognitionBubbleController() {
  DCHECK(bubbles_.empty());
}

void SpeechRecognitionBubbleController::InvokeDelegateButtonClicked(
    int session_id, SpeechRecognitionBubble::Button button) {
  delegate_->InfoBubbleButtonClicked(session_id, button);
}

void SpeechRecognitionBubbleController::InvokeDelegateFocusChanged(
    int session_id) {
  delegate_->InfoBubbleFocusChanged(session_id);
}

void SpeechRecognitionBubbleController::ProcessRequestInUiThread(
    int session_id, RequestType type, const string16& text, float volume,
    float noise_volume) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &SpeechRecognitionBubbleController::ProcessRequestInUiThread, this,
        session_id, type, text, volume, noise_volume));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // The bubble may have been closed before we got a chance to process this
  // request. So check before proceeding.
  if (!bubbles_.count(session_id))
    return;

  bool change_active_bubble = (type == REQUEST_SET_WARM_UP_MODE ||
                               type == REQUEST_SET_MESSAGE);
  if (change_active_bubble) {
    if (current_bubble_session_id_ && current_bubble_session_id_ != session_id)
      bubbles_[current_bubble_session_id_]->Hide();
    current_bubble_session_id_ = session_id;
  }

  SpeechRecognitionBubble* bubble = bubbles_[session_id];
  switch (type) {
    case REQUEST_SET_WARM_UP_MODE:
      bubble->SetWarmUpMode();
      break;
    case REQUEST_SET_RECORDING_MODE:
      bubble->SetRecordingMode();
      break;
    case REQUEST_SET_RECOGNIZING_MODE:
      bubble->SetRecognizingMode();
      break;
    case REQUEST_SET_MESSAGE:
      bubble->SetMessage(text);
      break;
    case REQUEST_SET_INPUT_VOLUME:
      bubble->SetInputVolume(volume, noise_volume);
      break;
    case REQUEST_CLOSE:
      if (current_bubble_session_id_ == session_id)
        current_bubble_session_id_ = 0;
      UpdateTabContentsSubscription(session_id, BUBBLE_REMOVED);
      delete bubble;
      bubbles_.erase(session_id);
      break;
    default:
      NOTREACHED();
      break;
  }

  if (change_active_bubble)
    bubble->Show();
}

void SpeechRecognitionBubbleController::UpdateTabContentsSubscription(
    int session_id, ManageSubscriptionAction action) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If there are any other bubbles existing for the same WebContents, we would
  // have subscribed to tab close notifications on their behalf and we need to
  // stay registered. So we don't change the subscription in such cases.
  WebContents* web_contents = bubbles_[session_id]->GetWebContents();
  for (BubbleSessionIdMap::iterator iter = bubbles_.begin();
       iter != bubbles_.end(); ++iter) {
    if (iter->second->GetWebContents() == web_contents &&
        iter->first != session_id) {
      // At least one other bubble exists for the same WebContents. So don't
      // make any change to the subscription.
      return;
    }
  }

  if (action == BUBBLE_ADDED) {
    registrar_->Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    content::Source<WebContents>(web_contents));
  } else {
    registrar_->Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                       content::Source<WebContents>(web_contents));
  }
}

}  // namespace speech
