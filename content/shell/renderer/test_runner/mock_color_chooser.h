// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_COLOR_CHOOSER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_COLOR_CHOOSER_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/web/WebColorChooser.h"
#include "third_party/WebKit/public/web/WebColorChooserClient.h"

namespace content {

class WebTestDelegate;
class WebTestProxyBase;

class MockColorChooser : public blink::WebColorChooser {
public:
    MockColorChooser(blink::WebColorChooserClient* client,
                     WebTestDelegate* delegate,
                     WebTestProxyBase* proxy);
    virtual ~MockColorChooser();

    // blink::WebColorChooser implementation.
    virtual void setSelectedColor(const blink::WebColor color) OVERRIDE;
    virtual void endChooser() OVERRIDE;

    void InvokeDidEndChooser();
    WebTaskList* mutable_task_list() { return &task_list_; }

private:
    blink::WebColorChooserClient* client_;
    WebTestDelegate* delegate_;
    WebTestProxyBase* proxy_;
    WebTaskList task_list_;

    DISALLOW_COPY_AND_ASSIGN(MockColorChooser);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_COLOR_CHOOSER_H_
