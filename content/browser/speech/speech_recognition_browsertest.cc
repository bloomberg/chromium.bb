// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/speech/input_tag_speech_dispatcher_host.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

using content::NavigationController;
using content::WebContents;

namespace speech {
class FakeSpeechRecognitionManager;
}

namespace speech {

const char kTestResult[] = "Pictures of the moon";

class FakeSpeechRecognitionManager : public SpeechRecognitionManagerImpl {
 public:
  FakeSpeechRecognitionManager()
      : caller_id_(0),
        delegate_(NULL),
        did_cancel_all_(false),
        should_send_fake_response_(true),
        recognition_started_event_(false, false) {
  }

  std::string grammar() {
    return grammar_;
  }

  bool did_cancel_all() {
    return did_cancel_all_;
  }

  void set_should_send_fake_response(bool send) {
    should_send_fake_response_ = send;
  }

  bool should_send_fake_response() {
    return should_send_fake_response_;
  }

  base::WaitableEvent& recognition_started_event() {
    return recognition_started_event_;
  }

  // SpeechRecognitionManager methods.
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
      content::SpeechRecognitionPreferences* recognition_prefs) OVERRIDE {
    VLOG(1) << "StartRecognition invoked.";
    EXPECT_EQ(0, caller_id_);
    EXPECT_EQ(NULL, delegate_);
    caller_id_ = caller_id;
    delegate_ = delegate;
    grammar_ = grammar;
    if (should_send_fake_response_) {
      // Give the fake result in a short while.
      MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
          &FakeSpeechRecognitionManager::SetFakeRecognitionResult,
          // This class does not need to be refcounted (typically done by
          // PostTask) since it will outlive the test and gets released only
          // when the test shuts down. Disabling refcounting here saves a bit
          // of unnecessary code and the factory method can return a plain
          // pointer below as required by the real code.
          base::Unretained(this)));
    }
    recognition_started_event_.Signal();
  }
  virtual void CancelRecognition(int caller_id) OVERRIDE {
    VLOG(1) << "CancelRecognition invoked.";
    EXPECT_EQ(caller_id_, caller_id);
    caller_id_ = 0;
    delegate_ = NULL;
  }
  virtual void StopRecording(int caller_id) OVERRIDE {
    VLOG(1) << "StopRecording invoked.";
    EXPECT_EQ(caller_id_, caller_id);
    // Nothing to do here since we aren't really recording.
  }
  virtual void CancelAllRequestsWithDelegate(
      InputTagSpeechDispatcherHost* delegate) OVERRIDE {
    VLOG(1) << "CancelAllRequestsWithDelegate invoked.";
    // delegate_ is set to NULL if a fake result was received (see below), so
    // check that delegate_ matches the incoming parameter only when there is
    // no fake result sent.
    EXPECT_TRUE(should_send_fake_response_ || delegate_ == delegate);
    did_cancel_all_ = true;
  }

 private:
  void SetFakeRecognitionResult() {
    if (caller_id_) {  // Do a check in case we were cancelled..
      VLOG(1) << "Setting fake recognition result.";
      delegate_->DidCompleteRecording(caller_id_);
      content::SpeechRecognitionResult results;
      results.hypotheses.push_back(content::SpeechRecognitionHypothesis(
          ASCIIToUTF16(kTestResult), 1.0));
      delegate_->SetRecognitionResult(caller_id_, results);
      delegate_->DidCompleteRecognition(caller_id_);
      caller_id_ = 0;
      delegate_ = NULL;
      VLOG(1) << "Finished setting fake recognition result.";
    }
  }

  int caller_id_;
  InputTagSpeechDispatcherHost* delegate_;
  std::string grammar_;
  bool did_cancel_all_;
  bool should_send_fake_response_;
  base::WaitableEvent recognition_started_event_;
};

class SpeechRecognitionBrowserTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest methods
  virtual void SetUpCommandLine(CommandLine* command_line) {
    EXPECT_TRUE(!command_line->HasSwitch(switches::kDisableSpeechInput));
  }

  GURL testUrl(const FilePath::CharType* filename) {
    const FilePath kTestDir(FILE_PATH_LITERAL("speech"));
    return ui_test_utils::GetTestUrl(kTestDir, FilePath(filename));
  }

 protected:
  void LoadAndStartSpeechRecognitionTest(const FilePath::CharType* filename) {
    // The test page calculates the speech button's coordinate in the page on
    // load & sets that coordinate in the URL fragment. We send mouse down & up
    // events at that coordinate to trigger speech recognition.
    GURL test_url = testUrl(filename);
    ui_test_utils::NavigateToURL(browser(), test_url);

    WebKit::WebMouseEvent mouse_event;
    mouse_event.type = WebKit::WebInputEvent::MouseDown;
    mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
    mouse_event.x = 0;
    mouse_event.y = 0;
    mouse_event.clickCount = 1;
    WebContents* web_contents = browser()->GetSelectedWebContents();

    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&web_contents->GetController()));
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

  void RunSpeechRecognitionTest(const FilePath::CharType* filename) {
    // The fake speech input manager would receive the speech input
    // request and return the test string as recognition result. The test page
    // then sets the URL fragment as 'pass' if it received the expected string.
    LoadAndStartSpeechRecognitionTest(filename);

    EXPECT_EQ("pass", browser()->GetSelectedWebContents()->GetURL().ref());
  }

  // InProcessBrowserTest methods.
  virtual void SetUpInProcessBrowserTestFixture() {
    fake_speech_recognition_manager_.set_should_send_fake_response(true);
    speech_recognition_manager_ = &fake_speech_recognition_manager_;

    // Inject the fake manager factory so that the test result is returned to
    // the web page.
    InputTagSpeechDispatcherHost::set_manager(speech_recognition_manager_);
  }

  virtual void TearDownInProcessBrowserTestFixture() {
    speech_recognition_manager_ = NULL;
  }

  FakeSpeechRecognitionManager fake_speech_recognition_manager_;

  // This is used by the static |fakeManager|, and it is a pointer rather than a
  // direct instance per the style guide.
  static SpeechRecognitionManagerImpl* speech_recognition_manager_;
};

SpeechRecognitionManagerImpl*
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
  RunSpeechRecognitionTest(FILE_PATH_LITERAL("basic_recognition.html"));
  EXPECT_TRUE(fake_speech_recognition_manager_.grammar().empty());
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest, GrammarAttribute) {
  RunSpeechRecognitionTest(FILE_PATH_LITERAL("grammar_attribute.html"));
  EXPECT_EQ("http://example.com/grammar.xml",
            fake_speech_recognition_manager_.grammar());
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest, TestCancelAll) {
  // The test checks that the cancel-all callback gets issued when a session
  // is pending, so don't send a fake response.
  // We are not expecting a navigation event being raised from the JS of the
  // test page JavaScript in this case.
  fake_speech_recognition_manager_.set_should_send_fake_response(false);

  LoadAndStartSpeechRecognitionTest(
      FILE_PATH_LITERAL("basic_recognition.html"));

  // Make the renderer crash. This should trigger
  // InputTagSpeechDispatcherHost to cancel all pending sessions.
  GURL test_url("about:crash");
  ui_test_utils::NavigateToURL(browser(), test_url);

  EXPECT_TRUE(fake_speech_recognition_manager_.did_cancel_all());
}

}  // namespace speech
