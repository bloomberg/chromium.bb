// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/speech/speech_input_dispatcher_host.h"
#include "chrome/browser/speech/speech_input_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace speech_input {
class FakeSpeechInputManager;
}

// This class does not need to be refcounted (typically done by PostTask) since
// it will outlive the test and gets released only when the test shuts down.
// Disabling refcounting here saves a bit of unnecessary code and the factory
// method can return a plain pointer below as required by the real code.
DISABLE_RUNNABLE_METHOD_REFCOUNT(speech_input::FakeSpeechInputManager);

namespace speech_input {

const char* kTestResult = "Pictures of the moon";

class FakeSpeechInputManager : public SpeechInputManager {
 public:
  explicit FakeSpeechInputManager(Delegate* delegate)
      : caller_id_(0, 0),
        delegate_(delegate) {
  }

  // SpeechInputManager methods.
  void StartRecognition(const SpeechInputCallerId& caller_id) {
    EXPECT_EQ(0, caller_id_.first);
    caller_id_ = caller_id;
    // Give the fake result in a short while.
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
        &FakeSpeechInputManager::SetFakeRecognitionResult));
  }
  void CancelRecognition(const SpeechInputCallerId& caller_id) {
    EXPECT_EQ(caller_id_.first, caller_id.first);
    EXPECT_EQ(caller_id_.second, caller_id.second);
    caller_id_ = SpeechInputCallerId(0, 0);
  }
  void StopRecording(const SpeechInputCallerId& caller_id) {
    EXPECT_EQ(caller_id_.first, caller_id.first);
    EXPECT_EQ(caller_id_.second, caller_id.second);
    // Nothing to do here since we aren't really recording.
  }

 private:
  void SetFakeRecognitionResult() {
    if (caller_id_.first) {  // Do a check in case we were cancelled..
      delegate_->DidCompleteRecording(caller_id_);
      delegate_->SetRecognitionResult(caller_id_,
                                      ASCIIToUTF16(kTestResult));
      delegate_->DidCompleteRecognition(caller_id_);
      caller_id_ = SpeechInputCallerId(0, 0);
    }
  }

  SpeechInputCallerId caller_id_;
  Delegate* delegate_;
};

// Factory method.
SpeechInputManager* fakeManagerFactory(SpeechInputManager::Delegate* delegate) {
  return new FakeSpeechInputManager(delegate);
}

class SpeechInputBrowserTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest methods
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableSpeechInput);
  }

  GURL testUrl(const FilePath::CharType* filename) {
    const FilePath kTestDir(FILE_PATH_LITERAL("speech"));
    return ui_test_utils::GetTestUrl(kTestDir, FilePath(filename));
  }
};

IN_PROC_BROWSER_TEST_F(SpeechInputBrowserTest, DISABLED_TestBasicRecognition) {
  // Inject the fake manager factory so that the test result is returned to the
  // web page.
  SpeechInputDispatcherHost::set_manager_factory(&fakeManagerFactory);

  // The test page starts speech recognition and waits to receive the above
  // defined test string as the result. Once it receives the result it either
  // navigates to #pass or #fail depending on the result.
  GURL test_url = testUrl(FILE_PATH_LITERAL("basic_recognition.html"));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            test_url,
                                                            2);

  // Check that the page got the result it expected.
  EXPECT_EQ("pass", browser()->GetSelectedTabContents()->GetURL().ref());
}

} //  namespace speech_input
