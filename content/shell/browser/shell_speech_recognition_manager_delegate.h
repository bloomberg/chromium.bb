// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_SHELL_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"

namespace content {

// This is content_shell's delegate used by the speech recognition manager to
// check for permission to record audio. For content_shell, we always authorize
// speech recognition (see crbug.com/237119).
class ShellSpeechRecognitionManagerDelegate
    : public SpeechRecognitionManagerDelegate {
 public:
  ShellSpeechRecognitionManagerDelegate() {}
  virtual ~ShellSpeechRecognitionManagerDelegate() {}

  // SpeechRecognitionManagerDelegate methods.
  virtual void GetDiagnosticInformation(
      bool* can_report_metrics, std::string* hardware_info) OVERRIDE {}
  virtual void CheckRecognitionIsAllowed(int session_id,
      base::Callback<void(bool ask_user, bool is_allowed)> callback) OVERRIDE;
  virtual SpeechRecognitionEventListener* GetEventListener() OVERRIDE;
  virtual bool FilterProfanities(int render_process_id) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellSpeechRecognitionManagerDelegate);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
