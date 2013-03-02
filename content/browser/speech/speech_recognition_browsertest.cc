// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

namespace content {

class SpeechRecognitionBrowserTest : public ContentBrowserTest {
 public:
  // ContentBrowserTest methods
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EXPECT_TRUE(!command_line->HasSwitch(switches::kDisableSpeechInput));
  }

 protected:
  void LoadAndStartSpeechRecognitionTest(const char* filename) {
    // The test page calculates the speech button's coordinate in the page on
    // load & sets that coordinate in the URL fragment. We send mouse down & up
    // events at that coordinate to trigger speech recognition.
    GURL test_url = GetTestUrl("speech", filename);
    NavigateToURL(shell(), test_url);

    WebKit::WebMouseEvent mouse_event;
    mouse_event.type = WebKit::WebInputEvent::MouseDown;
    mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
    mouse_event.x = 0;
    mouse_event.y = 0;
    mouse_event.clickCount = 1;
    WebContents* web_contents = shell()->web_contents();

    WindowedNotificationObserver observer(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(&web_contents->GetController()));
    web_contents->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
    mouse_event.type = WebKit::WebInputEvent::MouseUp;
    web_contents->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
    fake_speech_recognition_manager_.recognition_started_event().Wait();

    // We should wait for a navigation event, raised by the test page JS code
    // upon the onwebkitspeechchange event, in all cases except when the
    // speech response is inhibited.
    if (fake_speech_recognition_manager_.should_send_fake_response())
      observer.Wait();
  }

  void RunSpeechRecognitionTest(const char* filename) {
    // The fake speech input manager would receive the speech input
    // request and return the test string as recognition result. The test page
    // then sets the URL fragment as 'pass' if it received the expected string.
    LoadAndStartSpeechRecognitionTest(filename);

    EXPECT_EQ("pass", shell()->web_contents()->GetURL().ref());
  }

  // ContentBrowserTest methods.
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    fake_speech_recognition_manager_.set_should_send_fake_response(true);
    speech_recognition_manager_ = &fake_speech_recognition_manager_;

    // Inject the fake manager factory so that the test result is returned to
    // the web page.
    SpeechRecognitionManager::SetManagerForTests(speech_recognition_manager_);
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    speech_recognition_manager_ = NULL;
  }

  FakeSpeechRecognitionManager fake_speech_recognition_manager_;

  // This is used by the static |fakeManager|, and it is a pointer rather than a
  // direct instance per the style guide.
  static SpeechRecognitionManager* speech_recognition_manager_;
};

SpeechRecognitionManager*
    SpeechRecognitionBrowserTest::speech_recognition_manager_ = NULL;

// TODO(satish): Once this flakiness has been fixed, add a second test here to
// check for sending many clicks in succession to the speech button and verify
// that it doesn't cause any crash but works as expected. This should act as the
// test for http://crbug.com/59173
//
// TODO(satish): Similar to above, once this flakiness has been fixed add
// another test here to check that when speech recognition is in progress and
// a renderer crashes, we get a call to
// SpeechRecognitionManager::CancelAllRequestsWithDelegate.
IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest, TestBasicRecognition) {
  RunSpeechRecognitionTest("basic_recognition.html");
  EXPECT_TRUE(fake_speech_recognition_manager_.grammar().empty());
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest, GrammarAttribute) {
  RunSpeechRecognitionTest("grammar_attribute.html");
  EXPECT_EQ("http://example.com/grammar.xml",
            fake_speech_recognition_manager_.grammar());
}

// Flaky on Linux, Windows and Mac http://crbug.com/140765.
IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest, DISABLED_TestCancelAll) {
  // The test checks that the cancel-all callback gets issued when a session
  // is pending, so don't send a fake response.
  // We are not expecting a navigation event being raised from the JS of the
  // test page JavaScript in this case.
  fake_speech_recognition_manager_.set_should_send_fake_response(false);

  LoadAndStartSpeechRecognitionTest("basic_recognition.html");

  // Make the renderer crash. This should trigger
  // InputTagSpeechDispatcherHost to cancel all pending sessions.
  NavigateToURL(shell(), GURL(kChromeUICrashURL));

  EXPECT_TRUE(fake_speech_recognition_manager_.did_cancel_all());
}

}  // namespace content
