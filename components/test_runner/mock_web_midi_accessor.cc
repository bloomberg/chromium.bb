// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/mock_web_midi_accessor.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/web_task.h"
#include "components/test_runner/web_test_delegate.h"
#include "components/test_runner/web_test_runner.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/webmidi/WebMIDIAccessorClient.h"

namespace test_runner {

MockWebMIDIAccessor::MockWebMIDIAccessor(blink::WebMIDIAccessorClient* client,
                                         TestInterfaces* interfaces)
    : client_(client), interfaces_(interfaces), weak_factory_(this) {}

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
  interfaces_->GetDelegate()->PostTask(new WebCallbackTask(base::Bind(
      &MockWebMIDIAccessor::ReportStartedSession, weak_factory_.GetWeakPtr(),
      interfaces_->GetTestRunner()->midiAccessorResult())));
}

void MockWebMIDIAccessor::ReportStartedSession(bool success) {
  client_->didStartSession(success, "InvalidStateError", "");
}

void MockWebMIDIAccessor::sendMIDIData(unsigned port_index,
                                       const unsigned char* data,
                                       size_t length,
                                       double timestamp) {
  // Emulate a loopback device for testing.
  client_->didReceiveMIDIData(port_index, data, length, timestamp);
}

}  // namespace test_runner
