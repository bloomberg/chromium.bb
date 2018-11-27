// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TTS_CONTROLLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_TTS_CONTROLLER_DELEGATE_H_

#include "content/public/browser/tts_controller.h"

namespace content {

// Allows embedders to access the current state of text-to-speech.
// TODO(katie): This currently matches tts_controller.h but we want to move
// functionality one at a time into tts_controller_impl from
// tts_controller_delegate_impl, and remove most of these functions.
class TtsControllerDelegate {
 public:
  // Returns true if we're currently speaking an utterance.
  virtual bool IsSpeaking() = 0;

  // Speak the given utterance. If the utterance's can_enqueue flag is true
  // and another utterance is in progress, adds it to the end of the queue.
  // Otherwise, interrupts any current utterance and speaks this one
  // immediately.
  virtual void SpeakOrEnqueue(Utterance* utterance) = 0;

  // Stop all utterances and flush the queue. Implies leaving pause mode
  // as well.
  virtual void Stop() = 0;

  // Pause the speech queue. Some engines may support pausing in the middle
  // of an utterance.
  virtual void Pause() = 0;

  // Resume speaking.
  virtual void Resume() = 0;

  // Handle events received from the speech engine. Events are forwarded to
  // the callback function, and in addition, completion and error events
  // trigger finishing the current utterance and starting the next one, if
  // any.
  virtual void OnTtsEvent(int utterance_id,
                          TtsEventType event_type,
                          int char_index,
                          const std::string& error_message) = 0;

  // Return a list of all available voices, including the native voice,
  // if supported, and all voices registered by extensions.
  virtual void GetVoices(content::BrowserContext* browser_context,
                         std::vector<VoiceData>* out_voices) = 0;

  // Called by the extension system or platform implementation when the
  // list of voices may have changed and should be re-queried.
  virtual void VoicesChanged() = 0;

  // Add a delegate that wants to be notified when the set of voices changes.
  virtual void AddVoicesChangedDelegate(VoicesChangedDelegate* delegate) = 0;

  // Remove delegate that wants to be notified when the set of voices changes.
  virtual void RemoveVoicesChangedDelegate(VoicesChangedDelegate* delegate) = 0;

  // Remove delegate that wants to be notified when an utterance fires an event.
  // Note: this cancels speech from any utterance with this delegate, and
  // removes any utterances with this delegate from the queue.
  virtual void RemoveUtteranceEventDelegate(
      UtteranceEventDelegate* delegate) = 0;

  // Set the delegate that processes TTS requests with user-installed
  // extensions.
  virtual void SetTtsEngineDelegate(TtsEngineDelegate* delegate) = 0;

  // Get the delegate that processes TTS requests with user-installed
  // extensions.
  virtual TtsEngineDelegate* GetTtsEngineDelegate() = 0;

 protected:
  virtual ~TtsControllerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TTS_CONTROLLER_DELEGATE_H_
