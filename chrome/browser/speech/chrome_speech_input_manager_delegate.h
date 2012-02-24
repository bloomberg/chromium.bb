// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CHROME_SPEECH_INPUT_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_SPEECH_CHROME_SPEECH_INPUT_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/speech/speech_input_bubble_controller.h"
#include "content/public/browser/speech_input_manager_delegate.h"

namespace speech_input {

// This is Chrome's implementation of the SpeechInputManager interface. This
// class is a singleton and accessed via the Get method.
class ChromeSpeechInputManagerDelegate
    : NON_EXPORTED_BASE(public content::SpeechInputManagerDelegate),
      public SpeechInputBubbleControllerDelegate {
 public:
  ChromeSpeechInputManagerDelegate();
  virtual ~ChromeSpeechInputManagerDelegate();

  // SpeechInputBubbleControllerDelegate methods.
  virtual void InfoBubbleButtonClicked(
      int caller_id, SpeechInputBubble::Button button) OVERRIDE;
  virtual void InfoBubbleFocusChanged(int caller_id) OVERRIDE;

 protected:
  // SpeechInputManagerDelegate methods.
  virtual void GetRequestInfo(bool* can_report_metrics,
                              std::string* request_info) OVERRIDE;
  virtual void ShowRecognitionRequested(int caller_id,
                                        int render_process_id,
                                        int render_view_id,
                                        const gfx::Rect& element_rect) OVERRIDE;
  virtual void ShowWarmUp(int caller_id) OVERRIDE;
  virtual void ShowRecognizing(int caller_id) OVERRIDE;
  virtual void ShowRecording(int caller_id) OVERRIDE;
  virtual void ShowInputVolume(int caller_id,
                               float volume,
                               float noise_volume) OVERRIDE;
  virtual void ShowMicError(int caller_id,
                            MicError error) OVERRIDE;
  virtual void ShowRecognizerError(int caller_id,
                                   content::SpeechInputError error) OVERRIDE;
  virtual void DoClose(int caller_id) OVERRIDE;

 private:
  class OptionalRequestInfo;

  scoped_refptr<SpeechInputBubbleController> bubble_controller_;
  scoped_refptr<OptionalRequestInfo> optional_request_info_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSpeechInputManagerDelegate);
};

}  // namespace speech_input

#endif  // CHROME_BROWSER_SPEECH_CHROME_SPEECH_INPUT_MANAGER_DELEGATE_H_
