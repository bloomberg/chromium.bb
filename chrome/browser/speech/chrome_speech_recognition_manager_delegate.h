// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/speech/speech_recognition_bubble_controller.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"

namespace speech {

// This is Chrome's implementation of the SpeechRecognitionManager interface.
// This class is a singleton and accessed via the Get method.
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
  virtual void GetRequestInfo(bool* can_report_metrics,
                              std::string* request_info) OVERRIDE;
  virtual void ShowRecognitionRequested(int session_id,
                                        int render_process_id,
                                        int render_view_id,
                                        const gfx::Rect& element_rect) OVERRIDE;
  virtual void ShowWarmUp(int session_id) OVERRIDE;
  virtual void ShowRecognizing(int session_id) OVERRIDE;
  virtual void ShowRecording(int session_id) OVERRIDE;
  virtual void ShowInputVolume(int session_id,
                               float volume,
                               float noise_volume) OVERRIDE;
  virtual void ShowMicError(int session_id,
                            MicError error) OVERRIDE;
  virtual void ShowRecognizerError(
      int session_id, content::SpeechRecognitionErrorCode error) OVERRIDE;
  virtual void DoClose(int session_id) OVERRIDE;

 private:
  class OptionalRequestInfo;

  scoped_refptr<SpeechRecognitionBubbleController> bubble_controller_;
  scoped_refptr<OptionalRequestInfo> optional_request_info_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSpeechRecognitionManagerDelegate);
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
