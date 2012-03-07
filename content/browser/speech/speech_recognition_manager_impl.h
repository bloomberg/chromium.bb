// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_MANAGER_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_MANAGER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognizer_delegate.h"
#include "ui/gfx/rect.h"

class AudioManager;

namespace content {
class ResourceContext;
class SpeechRecognitionManagerDelegate;
class SpeechRecognitionPreferences;
struct SpeechRecognitionResult;
}

namespace net {
class URLRequestContextGetter;
}

namespace speech {

class InputTagSpeechDispatcherHost;
class SpeechRecognizerImpl;

class CONTENT_EXPORT SpeechRecognitionManagerImpl
    : NON_EXPORTED_BASE(public content::SpeechRecognitionManager),
      NON_EXPORTED_BASE(public content::SpeechRecognizerDelegate) {
 public:
  static SpeechRecognitionManagerImpl* GetInstance();

  // SpeechRecognitionManager implementation:
  virtual void StartRecognitionForRequest(int caller_id) OVERRIDE;
  virtual void CancelRecognitionForRequest(int caller_id) OVERRIDE;
  virtual void FocusLostForRequest(int caller_id) OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual bool IsCapturingAudio() OVERRIDE;
  virtual string16 GetAudioInputDeviceModel() OVERRIDE;
  virtual void ShowAudioInputSettings() OVERRIDE;

  // Handlers for requests from render views.

  // |delegate| is a weak pointer and should remain valid until
  // its |DidCompleteRecognition| method is called or recognition is cancelled.
  // |render_process_id| is the ID of the renderer process initiating the
  // request.
  // |element_rect| is the display bounds of the html element requesting speech
  // input (in page coordinates).
  virtual void StartRecognition(
      InputTagSpeechDispatcherHost* delegate,
      int caller_id,
      int render_process_id,
      int render_view_id,
      const gfx::Rect& element_rect,
      const std::string& language,
      const std::string& grammar,
      const std::string& origin_url,
      net::URLRequestContextGetter* context_getter,
      content::SpeechRecognitionPreferences* speech_recognition_prefs);
  virtual void CancelRecognition(int caller_id);
  virtual void CancelAllRequestsWithDelegate(
      InputTagSpeechDispatcherHost* delegate);
  virtual void StopRecording(int caller_id);

  // Overridden from content::SpeechRecognizerDelegate:
  virtual void DidStartReceivingAudio(int caller_id) OVERRIDE;
  virtual void SetRecognitionResult(
      int caller_id, const content::SpeechRecognitionResult& result) OVERRIDE;
  virtual void DidCompleteRecording(int caller_id) OVERRIDE;
  virtual void DidCompleteRecognition(int caller_id) OVERRIDE;
  virtual void DidStartReceivingSpeech(int caller_id) OVERRIDE;
  virtual void DidStopReceivingSpeech(int caller_id) OVERRIDE;

  virtual void OnRecognizerError(
      int caller_id, content::SpeechRecognitionErrorCode error) OVERRIDE;
  virtual void DidCompleteEnvironmentEstimation(int caller_id) OVERRIDE;
  virtual void SetInputVolume(int caller_id, float volume,
                              float noise_volume) OVERRIDE;

 protected:
  // Private constructor to enforce singleton.
  friend struct DefaultSingletonTraits<SpeechRecognitionManagerImpl>;
  SpeechRecognitionManagerImpl();
  virtual ~SpeechRecognitionManagerImpl();

  bool HasPendingRequest(int caller_id) const;

 private:
  struct Request {
    Request();
    ~Request();

    InputTagSpeechDispatcherHost* delegate;
    scoped_refptr<SpeechRecognizerImpl> recognizer;
    bool is_active;  // Set to true when recording or recognition is going on.
  };

  struct SpeechRecognitionParams;

  InputTagSpeechDispatcherHost* GetDelegate(int caller_id) const;

  void CheckRenderViewTypeAndStartRecognition(
      const SpeechRecognitionParams& params);
  void ProceedStartingRecognition(const SpeechRecognitionParams& params);

  void CancelRecognitionAndInformDelegate(int caller_id);

  typedef std::map<int, Request> SpeechRecognizerMap;
  SpeechRecognizerMap requests_;
  std::string request_info_;
  bool can_report_metrics_;
  int recording_caller_id_;
  content::SpeechRecognitionManagerDelegate* delegate_;
};

}  // namespace speech

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_MANAGER_H_
