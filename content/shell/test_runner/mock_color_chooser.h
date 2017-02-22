// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_MOCK_COLOR_CHOOSER_H_
#define CONTENT_SHELL_TEST_RUNNER_MOCK_COLOR_CHOOSER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/web/WebColorChooser.h"
#include "third_party/WebKit/public/web/WebColorChooserClient.h"

namespace test_runner {

class TestRunner;
class WebTestDelegate;

class MockColorChooser : public blink::WebColorChooser {
 public:
  // Caller has to guarantee that |client| and |delegate| are alive
  // until |WebColorChooserClient::didEndChooser| is called.
  // Caller has to guarantee that |test_runner| lives longer
  // than MockColorChooser.
  MockColorChooser(blink::WebColorChooserClient* client,
                   WebTestDelegate* delegate,
                   TestRunner* test_runner);
  ~MockColorChooser() override;

  // blink::WebColorChooser implementation.
  void setSelectedColor(const blink::WebColor color) override;
  void endChooser() override;

  void InvokeDidEndChooser();

 private:
  blink::WebColorChooserClient* client_;
  WebTestDelegate* delegate_;
  TestRunner* test_runner_;

  base::WeakPtrFactory<MockColorChooser> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockColorChooser);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_MOCK_COLOR_CHOOSER_H_
