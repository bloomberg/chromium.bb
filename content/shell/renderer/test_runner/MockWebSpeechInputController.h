// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBSPEECHINPUTCONTROLLER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBSPEECHINPUTCONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebSpeechInputController.h"
#include "third_party/WebKit/public/web/WebSpeechInputResult.h"

namespace blink {
class WebSecurityOrigin;
class WebSpeechInputListener;
class WebString;
}

namespace WebTestRunner {

class WebTestDelegate;

class MockWebSpeechInputController : public blink::WebSpeechInputController {
public:
    explicit MockWebSpeechInputController(blink::WebSpeechInputListener*);
    virtual ~MockWebSpeechInputController();

    void addMockRecognitionResult(const blink::WebString& result, double confidence, const blink::WebString& language);
    void setDumpRect(bool);
    void clearResults();
    void setDelegate(WebTestDelegate*);

    // WebSpeechInputController implementation:
    virtual bool startRecognition(int requestId, const blink::WebRect& elementRect, const blink::WebString& language, const blink::WebString& grammar, const blink::WebSecurityOrigin&) OVERRIDE;
    virtual void cancelRecognition(int requestId) OVERRIDE;
    virtual void stopRecording(int requestId) OVERRIDE;

    WebTaskList* taskList() { return &m_taskList; }

private:
    void speechTaskFired();

    class SpeechTask : public WebMethodTask<MockWebSpeechInputController> {
    public:
        SpeechTask(MockWebSpeechInputController*);
        void stop();

    private:
        virtual void runIfValid() OVERRIDE;
    };

    blink::WebSpeechInputListener* m_listener;

    WebTaskList m_taskList;
    SpeechTask* m_speechTask;

    bool m_recording;
    int m_requestId;
    blink::WebRect m_requestRect;
    std::string m_language;

    std::map<std::string, std::vector<blink::WebSpeechInputResult> > m_recognitionResults;
    std::vector<blink::WebSpeechInputResult> m_resultsForEmptyLanguage;
    bool m_dumpRect;

    WebTestDelegate* m_delegate;

    DISALLOW_COPY_AND_ASSIGN(MockWebSpeechInputController);
};

}

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBSPEECHINPUTCONTROLLER_H_
