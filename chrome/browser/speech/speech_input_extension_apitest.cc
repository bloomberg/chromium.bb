// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/speech/speech_input_extension_api.h"
#include "chrome/browser/speech/speech_input_extension_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/speech/speech_recognizer.h"
#include "content/public/common/speech_input_result.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace net {
class URLRequestContextGetter;
}

// Mock class used to test the extension speech input API.
class SpeechInputExtensionApiTest : public ExtensionApiTest,
                                    public SpeechInputExtensionInterface {
 public:
  SpeechInputExtensionApiTest();
  virtual ~SpeechInputExtensionApiTest();

  void SetRecordingDevicesAvailable(bool available) {
    recording_devices_available_ = available;
  }

  void SetRecognitionError(content::SpeechInputError error) {
    next_error_ = error;
  }

  void SetRecognitionResult(const content::SpeechInputResult& result) {
    next_result_ = result;
  }

  void SetRecognitionDelay(int result_delay_ms) {
    result_delay_ms_ = result_delay_ms;
  }

  // Used as delay when the corresponding call should not be dispatched.
  static const int kDontDispatchCall = -1;

  // ExtensionApiTest methods.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  // SpeechInputExtensionInterface methods.
  virtual bool HasAudioInputDevices() OVERRIDE {
    return recording_devices_available_;
  }

  virtual bool IsRecordingInProcess() OVERRIDE {
    // Only the mock recognizer is supposed to be recording during testing.
    return HasValidRecognizer();
  }

  virtual bool HasValidRecognizer() OVERRIDE {
    return recognizer_is_valid_;
  }

  virtual void StartRecording(
      speech_input::SpeechRecognizerDelegate* delegate,
      net::URLRequestContextGetter* context_getter,
      int caller_id,
      const std::string& language,
      const std::string& grammar,
      bool filter_profanities) OVERRIDE;

  virtual void StopRecording(bool recognition_failed) OVERRIDE;

  SpeechInputExtensionManager* GetManager() {
    return SpeechInputExtensionManager::GetForProfile(browser()->profile());
  }

  // Auxiliary class used to hook the API manager into the test during its
  // lifetime. Required since browser() is not available during the set up
  // or tear down callbacks, or during the test class construction.
  class AutoManagerHook {
   public:
    explicit AutoManagerHook(SpeechInputExtensionApiTest* test)
        : test_(test) {
      test_->GetManager()->SetSpeechInputExtensionInterface(test_);
    }

    ~AutoManagerHook() {
      test_->GetManager()->SetSpeechInputExtensionInterface(NULL);
    }

   private:
    SpeechInputExtensionApiTest* test_;
  };

 private:
  void ProvideResults(int caller_id);

  bool recording_devices_available_;
  bool recognizer_is_valid_;
  content::SpeechInputError next_error_;
  content::SpeechInputResult next_result_;
  int result_delay_ms_;
};

SpeechInputExtensionApiTest::SpeechInputExtensionApiTest()
    : recording_devices_available_(true),
      recognizer_is_valid_(false),
      next_error_(content::SPEECH_INPUT_ERROR_NONE),
      result_delay_ms_(0) {
}

SpeechInputExtensionApiTest::~SpeechInputExtensionApiTest() {
}

void SpeechInputExtensionApiTest::StartRecording(
      speech_input::SpeechRecognizerDelegate* delegate,
      net::URLRequestContextGetter* context_getter,
      int caller_id,
      const std::string& language,
      const std::string& grammar,
      bool filter_profanities) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  recognizer_is_valid_ = true;

  // Notify that recording started.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::DidStartReceivingAudio,
      GetManager(), caller_id), 0);

  // Notify sound start in the input device.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&SpeechInputExtensionManager::DidStartReceivingSpeech,
      GetManager(), caller_id), 0);

  if (result_delay_ms_ != kDontDispatchCall) {
    // Dispatch the recognition results.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        base::Bind(&SpeechInputExtensionApiTest::ProvideResults, this,
        caller_id), result_delay_ms_);
  }
}

void SpeechInputExtensionApiTest::StopRecording(bool recognition_failed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  recognizer_is_valid_ = false;
}

void SpeechInputExtensionApiTest::ProvideResults(int caller_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (next_error_ != content::SPEECH_INPUT_ERROR_NONE) {
    GetManager()->OnRecognizerError(caller_id, next_error_);
    return;
  }

  GetManager()->DidStopReceivingSpeech(caller_id);
  GetManager()->SetRecognitionResult(caller_id, next_result_);
}

// Every test should leave the manager in the idle state when finished.
IN_PROC_BROWSER_TEST_F(SpeechInputExtensionApiTest, StartStopTest) {
  AutoManagerHook hook(this);

  SetRecognitionDelay(kDontDispatchCall);
  ASSERT_TRUE(RunExtensionTest("speech_input/start_stop")) << message_;
}

IN_PROC_BROWSER_TEST_F(SpeechInputExtensionApiTest, NoDevicesAvailable) {
  AutoManagerHook hook(this);

  SetRecordingDevicesAvailable(false);
  ASSERT_TRUE(RunExtensionTest("speech_input/start_error")) << message_;
}

IN_PROC_BROWSER_TEST_F(SpeechInputExtensionApiTest, RecognitionSuccessful) {
  AutoManagerHook hook(this);

  content::SpeechInputResult result;
  result.hypotheses.push_back(
      content::SpeechInputHypothesis(UTF8ToUTF16("this is a test"), 0.99));
  SetRecognitionResult(result);
  ASSERT_TRUE(RunExtensionTest("speech_input/recognition")) << message_;
}

IN_PROC_BROWSER_TEST_F(SpeechInputExtensionApiTest, RecognitionError) {
  AutoManagerHook hook(this);

  SetRecognitionError(content::SPEECH_INPUT_ERROR_NETWORK);
  ASSERT_TRUE(RunExtensionTest("speech_input/recognition_error")) << message_;
}
