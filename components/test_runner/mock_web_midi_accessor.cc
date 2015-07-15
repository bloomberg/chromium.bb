// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/mock_web_midi_accessor.h"

#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/web_test_delegate.h"
#include "components/test_runner/web_test_runner.h"
#include "third_party/WebKit/public/platform/WebMIDIAccessorClient.h"

namespace test_runner {

namespace {

class DidStartSessionTask : public WebMethodTask<MockWebMIDIAccessor> {
 public:
  DidStartSessionTask(MockWebMIDIAccessor* object,
                      blink::WebMIDIAccessorClient* client,
                      bool result)
      : WebMethodTask<MockWebMIDIAccessor>(object),
        client_(client),
        result_(result) {}

  void RunIfValid() override {
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
  blink::WebMIDIAccessorClient::MIDIPortState state =
      blink::WebMIDIAccessorClient::MIDIPortStateConnected;
  client_->didAddInputPort("MockInputID",
                           "MockInputManufacturer",
                           "MockInputName",
                           "MockInputVersion",
                           state);
  client_->didAddOutputPort("MockOutputID",
                            "MockOutputManufacturer",
                            "MockOutputName",
                            "MockOutputVersion",
                            state);
  interfaces_->GetDelegate()->PostTask(new DidStartSessionTask(
      this, client_, interfaces_->GetTestRunner()->midiAccessorResult()));
}

void MockWebMIDIAccessor::sendMIDIData(unsigned port_index,
                                       const unsigned char* data,
                                       size_t length,
                                       double timestamp) {
  // Emulate a loopback device for testing.
  client_->didReceiveMIDIData(port_index, data, length, timestamp);
}

}  // namespace test_runner
