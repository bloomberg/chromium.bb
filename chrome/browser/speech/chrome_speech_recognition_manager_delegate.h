// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/speech/speech_recognition_bubble_controller.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"

namespace speech {

// This is Chrome's implementation of the SpeechRecognitionManagerDelegate
// interface.
class ChromeSpeechRecognitionManagerDelegate
    : NON_EXPORTED_BASE(public content::SpeechRecognitionManagerDelegate),
      public SpeechRecognitionBubbleControllerDelegate {
 public:
  ChromeSpeechRecognitionManagerDelegate();
  virtual ~ChromeSpeechRecognitionManagerDelegate();

  // SpeechRecognitionBubbleControllerDelegate methods.
  virtual void InfoBubbleButtonClicked(
      int session_id, SpeechRecognitionBubble::Button button) OVERRIDE;
  virtual void InfoBubbleFocusChanged(int session_id) OVERRIDE;

 protected:
  // SpeechRecognitionManagerDelegate methods.
  virtual void GetDiagnosticInformation(bool* can_report_metrics,
                                        std::string* hardware_info) OVERRIDE;
  virtual void CheckRecognitionIsAllowed(
      int session_id,
      base::Callback<void(int session_id, bool is_allowed)> callback) OVERRIDE;
  virtual void ShowRecognitionRequested(int session_id) OVERRIDE;
  virtual void ShowWarmUp(int session_id) OVERRIDE;
  virtual void ShowRecognizing(int session_id) OVERRIDE;
  virtual void ShowRecording(int session_id) OVERRIDE;
  virtual void ShowInputVolume(int session_id,
                               float volume,
                               float noise_volume) OVERRIDE;
  virtual void ShowError(int session_id,
                         const content::SpeechRecognitionError& error) OVERRIDE;
  virtual void DoClose(int session_id) OVERRIDE;

 private:
  class OptionalRequestInfo;

  // Checks for VIEW_TYPE_WEB_CONTENTS host in the UI thread and notifies back
  // the result in the IO thread through |callback|.
  static void CheckRenderViewType(
      int session_id,
      base::Callback<void(int session_id, bool is_allowed)> callback,
      int render_process_id,
      int render_view_id);

  scoped_refptr<SpeechRecognitionBubbleController> bubble_controller_;
  scoped_refptr<OptionalRequestInfo> optional_request_info_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSpeechRecognitionManagerDelegate);
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
