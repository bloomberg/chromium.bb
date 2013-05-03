// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_MESSAGE_FILTER_H_
#define CHROME_BROWSER_SPEECH_TTS_MESSAGE_FILTER_H_

#include "chrome/browser/speech/tts_controller.h"
#include "chrome/common/tts_messages.h"
#include "content/public/browser/browser_message_filter.h"

class Profile;

class TtsMessageFilter
    : public content::BrowserMessageFilter,
      public UtteranceEventDelegate {
 public:
  TtsMessageFilter(int render_process_id, Profile* profile);

  // content::BrowserMessageFilter implementation.
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // UtteranceEventDelegate implementation.
  virtual void OnTtsEvent(Utterance* utterance,
                          TtsEventType event_type,
                          int char_index,
                          const std::string& error_message) OVERRIDE;

 private:
  virtual ~TtsMessageFilter();

  void OnInitializeVoiceList();
  void OnSpeak(const TtsUtteranceRequest& utterance);
  void OnPause();
  void OnResume();
  void OnCancel();

  int render_process_id_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(TtsMessageFilter);
};

#endif  // CHROME_BROWSER_SPEECH_TTS_MESSAGE_FILTER_H_
