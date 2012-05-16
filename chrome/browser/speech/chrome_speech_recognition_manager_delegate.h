// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/speech/speech_recognition_bubble_controller.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"

namespace speech {

// This is Chrome's implementation of the SpeechRecognitionManagerDelegate
// interface.
class ChromeSpeechRecognitionManagerDelegate
    : NON_EXPORTED_BASE(public content::SpeechRecognitionManagerDelegate),
      public content::SpeechRecognitionEventListener,
      public SpeechRecognitionBubbleControllerDelegate {
 public:
  ChromeSpeechRecognitionManagerDelegate();
  virtual ~ChromeSpeechRecognitionManagerDelegate();

 protected:
  // SpeechRecognitionBubbleControllerDelegate methods.
  virtual void InfoBubbleButtonClicked(
      int session_id, SpeechRecognitionBubble::Button button) OVERRIDE;
  virtual void InfoBubbleFocusChanged(int session_id) OVERRIDE;

  // SpeechRecognitionEventListener methods.
  virtual void OnRecognitionStart(int session_id) OVERRIDE;
  virtual void OnAudioStart(int session_id) OVERRIDE;
  virtual void OnEnvironmentEstimationComplete(int session_id) OVERRIDE;
  virtual void OnSoundStart(int session_id) OVERRIDE;
  virtual void OnSoundEnd(int session_id) OVERRIDE;
  virtual void OnAudioEnd(int session_id) OVERRIDE;
  virtual void OnRecognitionEnd(int session_id) OVERRIDE;
  virtual void OnRecognitionResult(
      int session_id, const content::SpeechRecognitionResult& result) OVERRIDE;
  virtual void OnRecognitionError(
      int session_id, const content::SpeechRecognitionError& error) OVERRIDE;
  virtual void OnAudioLevelsChange(int session_id, float volume,
                                   float noise_volume) OVERRIDE;

  // SpeechRecognitionManagerDelegate methods.
  virtual void GetDiagnosticInformation(bool* can_report_metrics,
                                        std::string* hardware_info) OVERRIDE;
  virtual void CheckRecognitionIsAllowed(
      int session_id,
      base::Callback<void(int session_id, bool is_allowed)> callback) OVERRIDE;
  virtual content::SpeechRecognitionEventListener* GetEventListener() OVERRIDE;

 private:
  class OptionalRequestInfo;

  // Checks for VIEW_TYPE_TAB_CONTENTS host in the UI thread and notifies back
  // the result in the IO thread through |callback|.
  static void CheckRenderViewType(
      int session_id,
      base::Callback<void(int session_id, bool is_allowed)> callback,
      int render_process_id,
      int render_view_id);

  // Starts a new recognition session, using the config of the last one
  // (which is copied into |last_session_config_|). Used for "try again".
  void RestartLastSession();

  scoped_refptr<SpeechRecognitionBubbleController> bubble_controller_;
  scoped_refptr<OptionalRequestInfo> optional_request_info_;
  scoped_ptr<content::SpeechRecognitionSessionConfig> last_session_config_;

  // TODO(primiano) this information should be kept into the bubble_controller_.
  int active_bubble_session_id_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSpeechRecognitionManagerDelegate);
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
