// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_MANAGER_IMPL_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "ui/gfx/rect.h"

namespace content {
class ResourceContext;
class SpeechRecognitionManagerDelegate;
class SpeechRecognitionPreferences;
struct SpeechRecognitionResult;
class SpeechRecognizer;
}

namespace net {
class URLRequestContextGetter;
}

namespace speech {

class InputTagSpeechDispatcherHost;

class CONTENT_EXPORT SpeechRecognitionManagerImpl
    : NON_EXPORTED_BASE(public content::SpeechRecognitionManager),
      NON_EXPORTED_BASE(public content::SpeechRecognitionEventListener) {
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

  // SpeechRecognitionEventListener methods.
  virtual void OnRecognitionStart(int caller_id) OVERRIDE;
  virtual void OnAudioStart(int caller_id) OVERRIDE;
  virtual void OnEnvironmentEstimationComplete(int caller_id) OVERRIDE;
  virtual void OnSoundStart(int caller_id) OVERRIDE;
  virtual void OnSoundEnd(int caller_id) OVERRIDE;
  virtual void OnAudioEnd(int caller_id) OVERRIDE;
  virtual void OnRecognitionEnd(int caller_id) OVERRIDE;
  virtual void OnRecognitionResult(
      int caller_id, const content::SpeechRecognitionResult& result) OVERRIDE;
  virtual void OnRecognitionError(
      int caller_id, const content::SpeechRecognitionError& error) OVERRIDE;
  virtual void OnAudioLevelsChange(
      int caller_id, float volume, float noise_volume) OVERRIDE;

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
    scoped_refptr<content::SpeechRecognizer> recognizer;
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
  scoped_ptr<content::SpeechRecognitionManagerDelegate> delegate_;
};

}  // namespace speech

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_MANAGER_IMPL_H_
