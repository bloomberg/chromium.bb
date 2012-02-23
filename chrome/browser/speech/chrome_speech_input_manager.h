// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CHROME_SPEECH_INPUT_MANAGER_H_
#define CHROME_BROWSER_SPEECH_CHROME_SPEECH_INPUT_MANAGER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "chrome/browser/speech/speech_input_bubble_controller.h"
#include "content/browser/speech/speech_input_manager.h"

namespace speech_input {

// This is Chrome's implementation of the SpeechInputManager interface. This
// class is a singleton and accessed via the Get method.
class ChromeSpeechInputManager : public SpeechInputManager,
                                 public SpeechInputBubbleControllerDelegate {
 public:
  static ChromeSpeechInputManager* GetInstance();

  // SpeechInputBubbleController::Delegate methods.
  virtual void InfoBubbleButtonClicked(int caller_id,
                                       SpeechInputBubble::Button button)
                                       OVERRIDE;
  virtual void InfoBubbleFocusChanged(int caller_id) OVERRIDE;

 protected:
  // SpeechInputManager methods.
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
                            SpeechInputManager::MicError error) OVERRIDE;
  virtual void ShowRecognizerError(int caller_id,
                                   content::SpeechInputError error) OVERRIDE;
  virtual void DoClose(int caller_id) OVERRIDE;

 private:
  class OptionalRequestInfo;

  // Private constructor to enforce singleton.
  friend struct DefaultSingletonTraits<ChromeSpeechInputManager>;
  ChromeSpeechInputManager();
  virtual ~ChromeSpeechInputManager();

  scoped_refptr<SpeechInputBubbleController> bubble_controller_;
  scoped_refptr<OptionalRequestInfo> optional_request_info_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSpeechInputManager);
};

}  // namespace speech_input

#endif  // CHROME_BROWSER_SPEECH_CHROME_SPEECH_INPUT_MANAGER_H_
