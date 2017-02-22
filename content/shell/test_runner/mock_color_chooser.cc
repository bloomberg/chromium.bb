// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/mock_color_chooser.h"

#include "base/bind.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/web_test_delegate.h"

namespace test_runner {

MockColorChooser::MockColorChooser(blink::WebColorChooserClient* client,
                                   WebTestDelegate* delegate,
                                   TestRunner* test_runner)
    : client_(client),
      delegate_(delegate),
      test_runner_(test_runner),
      weak_factory_(this) {
  test_runner_->DidOpenChooser();
}

MockColorChooser::~MockColorChooser() {
  test_runner_->DidCloseChooser();
}

void MockColorChooser::setSelectedColor(const blink::WebColor color) {}

void MockColorChooser::endChooser() {
  delegate_->PostDelayedTask(base::Bind(&MockColorChooser::InvokeDidEndChooser,
                                        weak_factory_.GetWeakPtr()),
                             0);
}

void MockColorChooser::InvokeDidEndChooser() {
  client_->didEndChooser();
}

}  // namespace test_runner
