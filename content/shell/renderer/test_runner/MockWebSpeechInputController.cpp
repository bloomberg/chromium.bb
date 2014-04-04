// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/MockWebSpeechInputController.h"

#include "base/logging.h"
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebSpeechInputListener.h"

#if ENABLE_INPUT_SPEECH

using namespace blink;
using namespace std;

namespace WebTestRunner {

namespace {

WebSpeechInputResultArray makeRectResult(const WebRect& rect)
{
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%d,%d,%d,%d", rect.x, rect.y, rect.width, rect.height);

    WebSpeechInputResult res;
    res.assign(WebString::fromUTF8(static_cast<const char*>(buffer)), 1.0);

    WebSpeechInputResultArray results;
    results.assign(&res, 1);
    return results;
}

}

MockWebSpeechInputController::MockWebSpeechInputController(WebSpeechInputListener* listener)
    : m_listener(listener)
    , m_speechTask(0)
    , m_recording(false)
    , m_requestId(-1)
    , m_dumpRect(false)
    , m_delegate(0)
{
}

MockWebSpeechInputController::~MockWebSpeechInputController()
{
}

void MockWebSpeechInputController::setDelegate(WebTestDelegate* delegate)
{
    m_delegate = delegate;
}

void MockWebSpeechInputController::addMockRecognitionResult(const WebString& result, double confidence, const WebString& language)
{
    WebSpeechInputResult res;
    res.assign(result, confidence);

    if (language.isEmpty())
        m_resultsForEmptyLanguage.push_back(res);
    else {
        string langString = language.utf8();
        if (m_recognitionResults.find(langString) == m_recognitionResults.end())
            m_recognitionResults[langString] = vector<WebSpeechInputResult>();
        m_recognitionResults[langString].push_back(res);
    }
}

void MockWebSpeechInputController::setDumpRect(bool dumpRect)
{
    m_dumpRect = dumpRect;
}

void MockWebSpeechInputController::clearResults()
{
    m_resultsForEmptyLanguage.clear();
    m_recognitionResults.clear();
    m_dumpRect = false;
}

bool MockWebSpeechInputController::startRecognition(int requestId, const WebRect& elementRect, const WebString& language, const WebString& grammar, const WebSecurityOrigin& origin)
{
    if (m_speechTask)
        return false;

    m_requestId = requestId;
    m_requestRect = elementRect;
    m_recording = true;
    m_language = language.utf8();

    m_speechTask = new SpeechTask(this);
    m_delegate->postTask(m_speechTask);

    return true;
}

void MockWebSpeechInputController::cancelRecognition(int requestId)
{
    if (m_speechTask) {
        DCHECK_EQ(requestId, m_requestId);

        m_speechTask->stop();
        m_recording = false;
        m_listener->didCompleteRecognition(m_requestId);
        m_requestId = 0;
    }
}

void MockWebSpeechInputController::stopRecording(int requestId)
{
    DCHECK_EQ(requestId, m_requestId);
    if (m_speechTask && m_recording) {
        m_speechTask->stop();
        speechTaskFired();
    }
}

void MockWebSpeechInputController::speechTaskFired()
{
    if (m_recording) {
        m_recording = false;
        m_listener->didCompleteRecording(m_requestId);

        m_speechTask = new SpeechTask(this);
        m_delegate->postTask(m_speechTask);
    } else {
        bool noResultsFound = false;
        // We take a copy of the requestId here so that if scripts destroyed the input element
        // inside one of the callbacks below, we'll still know what this session's requestId was.
        int requestId = m_requestId;
        m_requestId = 0;

        if (m_dumpRect) {
            m_listener->setRecognitionResult(requestId, makeRectResult(m_requestRect));
        } else if (m_language.empty()) {
            // Empty language case must be handled separately to avoid problems with HashMap and empty keys.
            if (!m_resultsForEmptyLanguage.empty())
                m_listener->setRecognitionResult(requestId, m_resultsForEmptyLanguage);
            else
                noResultsFound = true;
        } else {
            if (m_recognitionResults.find(m_language) != m_recognitionResults.end())
                m_listener->setRecognitionResult(requestId, m_recognitionResults[m_language]);
            else
                noResultsFound = true;
        }

        if (noResultsFound) {
            // Can't avoid setting a result even if no result was set for the given language.
            // This would avoid generating the events used to check the results and the test would timeout.
            string error("error: no result found for language '");
            error.append(m_language);
            error.append("'");

            WebSpeechInputResult res;
            res.assign(WebString::fromUTF8(error), 1.0);

            vector<WebSpeechInputResult> results;
            results.push_back(res);

            m_listener->setRecognitionResult(requestId, results);
        }
    }
}

MockWebSpeechInputController::SpeechTask::SpeechTask(MockWebSpeechInputController* mock)
    : WebMethodTask<MockWebSpeechInputController>::WebMethodTask(mock)
{
}

void MockWebSpeechInputController::SpeechTask::stop()
{
    m_object->m_speechTask = 0;
    cancel();
}

void MockWebSpeechInputController::SpeechTask::runIfValid()
{
    m_object->m_speechTask = 0;
    m_object->speechTaskFired();
}

}

#endif
