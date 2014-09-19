// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_web_midi_accessor.h"

#include "content/shell/renderer/test_runner/test_interfaces.h"
#include "content/shell/renderer/test_runner/test_runner.h"
#include "content/shell/renderer/test_runner/web_test_delegate.h"
#include "content/shell/renderer/test_runner/web_test_runner.h"
#include "third_party/WebKit/public/platform/WebMIDIAccessorClient.h"

namespace content {

namespace {

class DidStartSessionTask : public WebMethodTask<MockWebMIDIAccessor> {
 public:
  DidStartSessionTask(MockWebMIDIAccessor* object,
                      blink::WebMIDIAccessorClient* client,
                      bool result)
      : WebMethodTask<MockWebMIDIAccessor>(object),
        client_(client),
        result_(result) {}

  virtual void RunIfValid() OVERRIDE {
    client_->didStartSession(result_, "InvalidStateError", "");
  }

 private:
  blink::WebMIDIAccessorClient* client_;
  bool result_;

  DISALLOW_COPY_AND_ASSIGN(DidStartSessionTask);
};

}  // namespace

MockWebMIDIAccessor::MockWebMIDIAccessor(blink::WebMIDIAccessorClient* client,
                                         TestInterfaces* interfaces)
    : client_(client), interfaces_(interfaces) {
}

MockWebMIDIAccessor::~MockWebMIDIAccessor() {
}

void MockWebMIDIAccessor::startSession() {
  // Add a mock input and output port.
  client_->didAddInputPort("MockInputID",
                           "MockInputManufacturer",
                           "MockInputName",
                           "MockInputVersion");
  client_->didAddOutputPort("MockOutputID",
                            "MockOutputManufacturer",
                            "MockOutputName",
                            "MockOutputVersion");
  interfaces_->GetDelegate()->PostTask(new DidStartSessionTask(
      this, client_, interfaces_->GetTestRunner()->midiAccessorResult()));
}

}  // namespace content
