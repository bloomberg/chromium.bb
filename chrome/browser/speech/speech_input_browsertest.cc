// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/speech/speech_input_dispatcher_host.h"
#include "chrome/browser/speech/speech_input_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"

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
  explicit FakeSpeechInputManager()
      : caller_id_(0),
        delegate_(NULL) {
  }

  // SpeechInputManager methods.
  void StartRecognition(Delegate* delegate,
                        int caller_id,
                        int render_process_id,
                        int render_view_id,
                        const gfx::Rect& element_rect) {
    EXPECT_EQ(0, caller_id_);
    EXPECT_EQ(NULL, delegate_);
    caller_id_ = caller_id;
    delegate_ = delegate;
    // Give the fake result in a short while.
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
        &FakeSpeechInputManager::SetFakeRecognitionResult));
  }
  void CancelRecognition(int caller_id) {
    EXPECT_EQ(caller_id_, caller_id);
    caller_id_ = 0;
    delegate_ = NULL;
  }
  void StopRecording(int caller_id) {
    EXPECT_EQ(caller_id_, caller_id);
    // Nothing to do here since we aren't really recording.
  }

 private:
  void SetFakeRecognitionResult() {
    if (caller_id_) {  // Do a check in case we were cancelled..
      delegate_->DidCompleteRecording(caller_id_);
      delegate_->SetRecognitionResult(caller_id_,
                                      ASCIIToUTF16(kTestResult));
      delegate_->DidCompleteRecognition(caller_id_);
      caller_id_ = 0;
      delegate_ = NULL;
    }
  }

  int caller_id_;
  Delegate* delegate_;
};

// Factory method.
SpeechInputManager* fakeManagerAccessor() {
  static FakeSpeechInputManager fake_speech_input_manager;
  return &fake_speech_input_manager;
}

class SpeechInputBrowserTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest methods
  GURL testUrl(const FilePath::CharType* filename) {
    const FilePath kTestDir(FILE_PATH_LITERAL("speech"));
    return ui_test_utils::GetTestUrl(kTestDir, FilePath(filename));
  }
};

IN_PROC_BROWSER_TEST_F(SpeechInputBrowserTest, FLAKY_TestBasicRecognition) {
  // Inject the fake manager factory so that the test result is returned to the
  // web page.
  SpeechInputDispatcherHost::set_manager_accessor(&fakeManagerAccessor);

  // The test page calculates the speech button's coordinate in the page on load
  // and sets that coordinate in the URL fragment. We send mouse down & up
  // events at that coordinate to trigger speech recognition.
  GURL test_url = testUrl(FILE_PATH_LITERAL("basic_recognition.html"));
  ui_test_utils::NavigateToURL(browser(), test_url);
  std::string coords = browser()->GetSelectedTabContents()->GetURL().ref();
  LOG(INFO) << "Coordinates given by script: " << coords;
  int comma_pos = coords.find(',');
  ASSERT_NE(-1, comma_pos);
  int x = 0;
  ASSERT_TRUE(base::StringToInt(coords.substr(0, comma_pos).c_str(), &x));
  int y = 0;
  ASSERT_TRUE(base::StringToInt(coords.substr(comma_pos + 1).c_str(), &y));

  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  mouse_event.x = x;
  mouse_event.y = y;
  mouse_event.clickCount = 1;
  TabContents* tab_contents = browser()->GetSelectedTabContents();
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);
  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);

  // The above defined fake speech input manager would receive the speech input
  // request and return the test string as recognition result. The test page
  // then sets the URL fragment as 'pass' if it received the expected string.
  ui_test_utils::WaitForNavigations(&tab_contents->controller(), 1);
  EXPECT_EQ("pass", browser()->GetSelectedTabContents()->GetURL().ref());
}

} //  namespace speech_input
