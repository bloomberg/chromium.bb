// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "content/browser/speech/speech_recognizer.h"
#include "content/common/content_export.h"
#include "ui/gfx/rect.h"

class SpeechInputPreferences;

namespace content {
struct SpeechInputResult;
}

namespace speech_input {

// This is the gatekeeper for speech recognition in the browser process. It
// handles requests received from various render views and makes sure only one
// of them can use speech recognition at a time. It also sends recognition
// results and status events to the render views when required.
class CONTENT_EXPORT SpeechInputManager : public SpeechRecognizerDelegate {
 public:
  // Implemented by the dispatcher host to relay events to the render views.
  class Delegate {
   public:
    virtual void SetRecognitionResult(
        int caller_id,
        const content::SpeechInputResult& result) = 0;
    virtual void DidCompleteRecording(int caller_id) = 0;
    virtual void DidCompleteRecognition(int caller_id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Describes the microphone errors that are reported via ShowMicError.
  enum MicError {
    kNoDeviceAvailable = 0,
    kDeviceInUse
  };

  SpeechInputManager();

  // Invokes the platform provided microphone settings UI in a non-blocking way,
  // via the BrowserThread::FILE thread.
  static void ShowAudioInputSettings();

  virtual ~SpeechInputManager();

  // Handlers for requests from render views.

  // |delegate| is a weak pointer and should remain valid until
  // its |DidCompleteRecognition| method is called or recognition is cancelled.
  // |render_process_id| is the ID of the renderer process initiating the
  // request.
  // |element_rect| is the display bounds of the html element requesting speech
  // input (in page coordinates).
  virtual void StartRecognition(Delegate* delegate,
                                int caller_id,
                                int render_process_id,
                                int render_view_id,
                                const gfx::Rect& element_rect,
                                const std::string& language,
                                const std::string& grammar,
                                const std::string& origin_url,
                                net::URLRequestContextGetter* context_getter,
                                SpeechInputPreferences* speech_input_prefs);
  virtual void CancelRecognition(int caller_id);
  virtual void CancelAllRequestsWithDelegate(Delegate* delegate);
  virtual void StopRecording(int caller_id);

  // SpeechRecognizerDelegate methods.
  virtual void DidStartReceivingAudio(int caller_id) OVERRIDE;
  virtual void SetRecognitionResult(
      int caller_id,
      const content::SpeechInputResult& result) OVERRIDE;
  virtual void DidCompleteRecording(int caller_id) OVERRIDE;
  virtual void DidCompleteRecognition(int caller_id) OVERRIDE;
  virtual void DidStartReceivingSpeech(int caller_id) OVERRIDE;
  virtual void DidStopReceivingSpeech(int caller_id) OVERRIDE;

  virtual void OnRecognizerError(int caller_id,
                                 content::SpeechInputError error) OVERRIDE;
  virtual void DidCompleteEnvironmentEstimation(int caller_id) OVERRIDE;
  virtual void SetInputVolume(int caller_id, float volume,
                              float noise_volume) OVERRIDE;

 protected:
  // The pure virtual methods are used for displaying the current state of
  // recognition and for fetching optional request information.

  // Get the optional request information if available.
  virtual void GetRequestInfo(bool* can_report_metrics,
                              std::string* request_info) = 0;

  // Called when recognition has been requested from point |element_rect_| on
  // the view port for the given caller.
  virtual void ShowRecognitionRequested(int caller_id,
                                        int render_process_id,
                                        int render_view_id,
                                        const gfx::Rect& element_rect) = 0;

  // Called when recognition is starting up.
  virtual void ShowWarmUp(int caller_id) = 0;

  // Called when recognition has started.
  virtual void ShowRecognizing(int caller_id) = 0;

  // Called when recording has started.
  virtual void ShowRecording(int caller_id) = 0;

  // Continuously updated with the current input volume.
  virtual void ShowInputVolume(int caller_id,
                               float volume,
                               float noise_volume) = 0;

  // Called when no microphone has been found.
  virtual void ShowMicError(int caller_id, MicError error) = 0;

  // Called when there has been a error with the recognition.
  virtual void ShowRecognizerError(int caller_id,
                                   content::SpeechInputError error) = 0;

  // Called when recognition has ended or has been canceled.
  virtual void DoClose(int caller_id) = 0;

  // Cancels recognition for the specified caller if it is active.
  void OnFocusChanged(int caller_id);

  bool HasPendingRequest(int caller_id) const;

  // Starts/restarts recognition for an existing request.
  void StartRecognitionForRequest(int caller_id);

  void CancelRecognitionAndInformDelegate(int caller_id);

 private:
  struct SpeechInputRequest {
    SpeechInputRequest();
    ~SpeechInputRequest();

    Delegate* delegate;
    scoped_refptr<SpeechRecognizer> recognizer;
    bool is_active;  // Set to true when recording or recognition is going on.
  };

  struct SpeechInputParams;

  Delegate* GetDelegate(int caller_id) const;

  void CheckRenderViewTypeAndStartRecognition(const SpeechInputParams& params);
  void ProceedStartingRecognition(const SpeechInputParams& params);

  typedef std::map<int, SpeechInputRequest> SpeechRecognizerMap;
  SpeechRecognizerMap requests_;
  std::string request_info_;
  bool can_report_metrics_;
  int recording_caller_id_;
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechInputManager::Delegate SpeechInputManagerDelegate;

}  // namespace speech_input

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_INPUT_MANAGER_H_
