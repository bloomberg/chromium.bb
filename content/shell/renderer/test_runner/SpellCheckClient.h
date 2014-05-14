// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_SPELLCHECKCLIENT_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_SPELLCHECKCLIENT_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/MockSpellCheck.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/web/WebSpellCheckClient.h"

namespace content {

class WebTestDelegate;
class WebTestProxyBase;

class SpellCheckClient : public blink::WebSpellCheckClient {
public:
    explicit SpellCheckClient(WebTestProxyBase*);
    virtual ~SpellCheckClient();

    void setDelegate(WebTestDelegate*);

    WebTaskList* mutable_task_list() { return &m_taskList; }
    MockSpellCheck* mockSpellCheck() { return &m_spellcheck; }

    // blink::WebSpellCheckClient implementation.
    virtual void spellCheck(const blink::WebString&, int& offset, int& length, blink::WebVector<blink::WebString>* optionalSuggestions);
    virtual void checkTextOfParagraph(const blink::WebString&, blink::WebTextCheckingTypeMask, blink::WebVector<blink::WebTextCheckingResult>*);
    virtual void requestCheckingOfText(const blink::WebString&,
                                       const blink::WebVector<uint32_t>&,
                                       const blink::WebVector<unsigned>&,
                                       blink::WebTextCheckingCompletion*);
    virtual blink::WebString autoCorrectWord(const blink::WebString&);

private:
    void finishLastTextCheck();

    // The mock spellchecker used in spellCheck().
    MockSpellCheck m_spellcheck;

    blink::WebString m_lastRequestedTextCheckString;
    blink::WebTextCheckingCompletion* m_lastRequestedTextCheckingCompletion;

    WebTaskList m_taskList;

    WebTestDelegate* m_delegate;

    WebTestProxyBase* m_webTestProxy;

    DISALLOW_COPY_AND_ASSIGN(SpellCheckClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_SPELLCHECKCLIENT_H_
