// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/speech/speech_recognition_bubble_controller.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"

class SpeechRecognitionTrayIconController;

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
      base::Callback<void(bool ask_user, bool is_allowed)> callback) OVERRIDE;
  virtual content::SpeechRecognitionEventListener* GetEventListener() OVERRIDE;

 private:
  class OptionalRequestInfo;
  class TabWatcher;

  // Shows the recognition tray icon for a given |context_name|, eventually
  // with a notification balloon. The balloon is shown only once per profile
  // for a given context_name. |render_process_id| is required to lookup the
  // profile associated with the renderer that initiated the recognition.
  static void ShowTrayIconOnUIThread(
      const std::string& context_name,
      int render_process_id,
      scoped_refptr<SpeechRecognitionTrayIconController> tray_icon_controller);

  // Checks for VIEW_TYPE_TAB_CONTENTS host in the UI thread and notifies back
  // the result in the IO thread through |callback|.
  static void CheckRenderViewType(
      base::Callback<void(bool ask_user, bool is_allowed)> callback,
      int render_process_id,
      int render_view_id,
      bool js_api);

  // Starts a new recognition session, using the config of the last one
  // (which is copied into |last_session_config_|). Used for "try again".
  void RestartLastSession();

  // Callback called by |tab_watcher_| on the IO thread to signal tab closure.
  void TabClosedCallback(int render_process_id, int render_view_id);

  // Lazy initializers for bubble and tray icon controller.
  SpeechRecognitionBubbleController* GetBubbleController();
  SpeechRecognitionTrayIconController* GetTrayIconController();

  scoped_refptr<SpeechRecognitionBubbleController> bubble_controller_;
  scoped_refptr<SpeechRecognitionTrayIconController> tray_icon_controller_;
  scoped_refptr<OptionalRequestInfo> optional_request_info_;
  scoped_ptr<content::SpeechRecognitionSessionConfig> last_session_config_;
  scoped_refptr<TabWatcher> tab_watcher_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSpeechRecognitionManagerDelegate);
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
