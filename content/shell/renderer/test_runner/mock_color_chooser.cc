// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_color_chooser.h"

#include "content/shell/renderer/test_runner/web_test_delegate.h"
#include "content/shell/renderer/test_runner/web_test_proxy.h"

namespace content {

namespace {
class HostMethodTask : public WebMethodTask<MockColorChooser> {
 public:
  typedef void (MockColorChooser::*CallbackMethodType)();
  HostMethodTask(MockColorChooser* object, CallbackMethodType callback)
      : WebMethodTask<MockColorChooser>(object),
        callback_(callback) {}

  virtual void RunIfValid() OVERRIDE { (object_->*callback_)(); }

 private:
  CallbackMethodType callback_;
};

} // namespace

MockColorChooser::MockColorChooser(blink::WebColorChooserClient* client,
                                   WebTestDelegate* delegate,
                                   WebTestProxyBase* proxy)
    : client_(client),
      delegate_(delegate),
      proxy_(proxy) {
  proxy_->DidOpenChooser();
}

MockColorChooser::~MockColorChooser() {
  proxy_->DidCloseChooser();
}

void MockColorChooser::setSelectedColor(const blink::WebColor color) {}

void MockColorChooser::endChooser() {
  delegate_->PostDelayedTask(
      new HostMethodTask(this, &MockColorChooser::InvokeDidEndChooser), 0);
}

void MockColorChooser::InvokeDidEndChooser() {
  client_->didEndChooser();
}

}  // namespace content
