// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CHROME_SPEECH_INPUT_MANAGER_H_
#define CHROME_BROWSER_SPEECH_CHROME_SPEECH_INPUT_MANAGER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "chrome/browser/speech/speech_input_bubble_controller.h"
#include "content/browser/speech/speech_input_manager.h"
#include "content/browser/speech/speech_recognizer.h"

namespace speech_input {

class ChromeSpeechInputManager : public SpeechInputManager,
                                 public SpeechInputBubbleControllerDelegate,
                                 public SpeechRecognizerDelegate {
 public:
  static ChromeSpeechInputManager* GetInstance();

  // SpeechInputManager methods.
  virtual void StartRecognition(SpeechInputManagerDelegate* delegate,
                                int caller_id,
                                int render_process_id,
                                int render_view_id,
                                const gfx::Rect& element_rect,
                                const std::string& language,
                                const std::string& grammar,
                                const std::string& origin_url);
  virtual void CancelRecognition(int caller_id);
  virtual void StopRecording(int caller_id);
  virtual void CancelAllRequestsWithDelegate(
      SpeechInputManagerDelegate* delegate);

  // SpeechRecognizer::Delegate methods.
  virtual void DidStartReceivingAudio(int caller_id);
  virtual void SetRecognitionResult(int caller_id,
                                    bool error,
                                    const SpeechInputResultArray& result);
  virtual void DidCompleteRecording(int caller_id);
  virtual void DidCompleteRecognition(int caller_id);
  virtual void OnRecognizerError(int caller_id,
                                 SpeechRecognizer::ErrorCode error);
  virtual void DidCompleteEnvironmentEstimation(int caller_id);
  virtual void SetInputVolume(int caller_id, float volume, float noise_volume);

  // SpeechInputBubbleController::Delegate methods.
  virtual void InfoBubbleButtonClicked(int caller_id,
                                       SpeechInputBubble::Button button);
  virtual void InfoBubbleFocusChanged(int caller_id);

 private:
  class OptionalRequestInfo;

  struct SpeechInputRequest {
    SpeechInputManagerDelegate* delegate;
    scoped_refptr<SpeechRecognizer> recognizer;
    bool is_active;  // Set to true when recording or recognition is going on.
  };

  // Private constructor to enforce singleton.
  friend struct DefaultSingletonTraits<ChromeSpeechInputManager>;
  ChromeSpeechInputManager();
  virtual ~ChromeSpeechInputManager();

  bool HasPendingRequest(int caller_id) const;
  SpeechInputManagerDelegate* GetDelegate(int caller_id) const;

  void CancelRecognitionAndInformDelegate(int caller_id);

  // Starts/restarts recognition for an existing request.
  void StartRecognitionForRequest(int caller_id);

  typedef std::map<int, SpeechInputRequest> SpeechRecognizerMap;
  SpeechRecognizerMap requests_;
  int recording_caller_id_;
  scoped_refptr<SpeechInputBubbleController> bubble_controller_;
  scoped_refptr<OptionalRequestInfo> optional_request_info_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSpeechInputManager);
};

}  // namespace speech_input

#endif  // CHROME_BROWSER_SPEECH_CHROME_SPEECH_INPUT_MANAGER_H_
