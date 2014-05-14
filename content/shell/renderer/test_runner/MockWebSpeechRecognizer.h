// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBSPEECHRECOGNIZER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBSPEECHRECOGNIZER_H_

#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/web/WebSpeechRecognizer.h"

namespace blink {
class WebSpeechRecognitionHandle;
class WebSpeechRecognitionParams;
class WebSpeechRecognizerClient;
}

namespace content {

class WebTestDelegate;

class MockWebSpeechRecognizer : public blink::WebSpeechRecognizer {
public:
    MockWebSpeechRecognizer();
    virtual ~MockWebSpeechRecognizer();

    void setDelegate(WebTestDelegate*);

    // WebSpeechRecognizer implementation:
    virtual void start(const blink::WebSpeechRecognitionHandle&, const blink::WebSpeechRecognitionParams&, blink::WebSpeechRecognizerClient*) OVERRIDE;
    virtual void stop(const blink::WebSpeechRecognitionHandle&, blink::WebSpeechRecognizerClient*) OVERRIDE;
    virtual void abort(const blink::WebSpeechRecognitionHandle&, blink::WebSpeechRecognizerClient*) OVERRIDE;

    // Methods accessed by layout tests:
    void addMockResult(const blink::WebString& transcript, float confidence);
    void setError(const blink::WebString& error, const blink::WebString& message);
    bool wasAborted() const { return m_wasAborted; }

    // Methods accessed from Task objects:
    blink::WebSpeechRecognizerClient* client() { return m_client; }
    blink::WebSpeechRecognitionHandle& handle() { return m_handle; }
    WebTaskList* mutable_task_list() { return &m_taskList; }

    class Task {
    public:
        Task(MockWebSpeechRecognizer* recognizer) : m_recognizer(recognizer) { }
        virtual ~Task() { }
        virtual void run() = 0;
    protected:
        MockWebSpeechRecognizer* m_recognizer;
    };

private:
    void startTaskQueue();
    void clearTaskQueue();

    WebTaskList m_taskList;
    blink::WebSpeechRecognitionHandle m_handle;
    blink::WebSpeechRecognizerClient* m_client;
    std::vector<blink::WebString> m_mockTranscripts;
    std::vector<float> m_mockConfidences;
    bool m_wasAborted;

    // Queue of tasks to be run.
    std::deque<Task*> m_taskQueue;
    bool m_taskQueueRunning;

    WebTestDelegate* m_delegate;

    // Task for stepping the queue.
    class StepTask : public WebMethodTask<MockWebSpeechRecognizer> {
    public:
        StepTask(MockWebSpeechRecognizer* object) : WebMethodTask<MockWebSpeechRecognizer>(object) { }
        virtual void runIfValid() OVERRIDE;
    };

    DISALLOW_COPY_AND_ASSIGN(MockWebSpeechRecognizer);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBSPEECHRECOGNIZER_H_
