// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_input_context_client.h"

#include <map>
#include <string>
#include "base/message_loop.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

namespace chromeos {

// TODO(nona): Remove after complete libibus removal.
using chromeos::ibus::IBusText;

namespace {
const char kObjectPath[] = "/org/freedesktop/IBus/InputContext_1010";

// Following variables are used in callback expectations.
const uint32 kCapabilities = 12345;
const int32 kCursorX = 30;
const int32 kCursorY = 31;
const int32 kCursorWidth = 32;
const int32 kCursorHeight = 33;
const uint32 kKeyval = 34;
const uint32 kKeycode = 35;
const uint32 kState = 36;
const int32 kCompositionX = 37;
const int32 kCompositionY = 38;
const int32 kCompositionWidth = 39;
const int32 kCompositionHeight = 40;
const bool kIsKeyHandled = false;
const char kSurroundingText[] = "Surrounding Text";
const uint32 kCursorPos = 2;
const uint32 kAnchorPos = 7;
const char kPropertyKey[] = "Property Key";
const ibus::IBusPropertyState kPropertyState =
    ibus::IBUS_PROPERTY_STATE_CHECKED;

class MockInputContextHandler : public IBusInputContextHandlerInterface {
 public:
  MOCK_METHOD1(CommitText, void(const IBusText& text));
  MOCK_METHOD3(ForwardKeyEvent,
               void(uint32 keyval, uint32 keycode, uint32 state));
  MOCK_METHOD0(ShowPreeditText, void());
  MOCK_METHOD0(HidePreeditText, void());
  MOCK_METHOD3(UpdatePreeditText,
               void(const IBusText& text, uint32 cursor_pos, bool visible));
};

class MockProcessKeyEventHandler {
 public:
  MOCK_METHOD1(Run, void(bool is_key_handled));
};

class MockProcessKeyEventErrorHandler {
 public:
  MOCK_METHOD0(Run, void());
};

MATCHER_P(IBusTextEq, expected_text, "The expected IBusText does not match") {
  // TODO(nona): Check attributes.
  return (arg.text() == expected_text->text());
}

}  // namespace

class IBusInputContextClientTest : public testing::Test {
 public:
  IBusInputContextClientTest()
      : response_(NULL),
        on_set_cursor_location_call_count_(0) {}

  virtual void SetUp() OVERRIDE {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create a mock proxy.
    mock_proxy_ = new dbus::MockObjectProxy(mock_bus_.get(),
                                            ibus::kServiceName,
                                            dbus::ObjectPath(kObjectPath));

    // Create a client.
    client_.reset(IBusInputContextClient::Create(
        REAL_DBUS_CLIENT_IMPLEMENTATION));

    // Set an expectation so mock_bus's GetObjectProxy() for the given service
    // name and the object path will return mock_proxy_. The GetObjectProxy
    // function is called in Initialized function.
    EXPECT_CALL(*mock_bus_, GetObjectProxy(ibus::kServiceName,
                                           dbus::ObjectPath(kObjectPath)))
        .WillOnce(Return(mock_proxy_.get()));

    // Set expectations so mock_proxy's ConnectToSignal will use
    // OnConnectToSignal() to run the callback. The ConnectToSignal is called in
    // Initialize function.
    EXPECT_CALL(*mock_proxy_, ConnectToSignal(
        ibus::input_context::kServiceInterface,
        ibus::input_context::kCommitTextSignal, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusInputContextClientTest::OnConnectToSignal));
    EXPECT_CALL(*mock_proxy_, ConnectToSignal(
        ibus::input_context::kServiceInterface,
        ibus::input_context::kForwardKeyEventSignal, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusInputContextClientTest::OnConnectToSignal));
    EXPECT_CALL(*mock_proxy_, ConnectToSignal(
        ibus::input_context::kServiceInterface,
        ibus::input_context::kHidePreeditTextSignal, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusInputContextClientTest::OnConnectToSignal));
    EXPECT_CALL(*mock_proxy_, ConnectToSignal(
        ibus::input_context::kServiceInterface,
        ibus::input_context::kShowPreeditTextSignal, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusInputContextClientTest::OnConnectToSignal));
    EXPECT_CALL(*mock_proxy_, ConnectToSignal(
        ibus::input_context::kServiceInterface,
        ibus::input_context::kUpdatePreeditTextSignal, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusInputContextClientTest::OnConnectToSignal));

    // Call Initialize to create object proxy and connect signals.
    client_->Initialize(mock_bus_.get(), dbus::ObjectPath(kObjectPath));
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_TRUE(client_->IsObjectProxyReady());
    client_->ResetObjectProxy();
    EXPECT_FALSE(client_->IsObjectProxyReady());
  }

  // Handles FocusIn method call.
  void OnFocusIn(dbus::MethodCall* method_call,
                 int timeout_ms,
                 const dbus::ObjectProxy::ResponseCallback& callback,
                 const dbus::ObjectProxy::ErrorCallback& error_callback) {
    EXPECT_EQ(ibus::input_context::kServiceInterface,
              method_call->GetInterface());
    EXPECT_EQ(ibus::input_context::kFocusInMethod, method_call->GetMember());
    dbus::MessageReader reader(method_call);
    EXPECT_FALSE(reader.HasMoreData());

    message_loop_.PostTask(FROM_HERE, base::Bind(callback, response_));
  }

  // Handles FocusOut method call.
  void OnFocusOut(dbus::MethodCall* method_call,
                  int timeout_ms,
                  const dbus::ObjectProxy::ResponseCallback& callback,
                  const dbus::ObjectProxy::ErrorCallback& error_callback) {
    EXPECT_EQ(ibus::input_context::kServiceInterface,
              method_call->GetInterface());
    EXPECT_EQ(ibus::input_context::kFocusOutMethod, method_call->GetMember());
    dbus::MessageReader reader(method_call);
    EXPECT_FALSE(reader.HasMoreData());

    message_loop_.PostTask(FROM_HERE, base::Bind(callback, response_));
  }

  // Handles Reset method call.
  void OnReset(dbus::MethodCall* method_call,
               int timeout_ms,
               const dbus::ObjectProxy::ResponseCallback& callback,
               const dbus::ObjectProxy::ErrorCallback& error_callback) {
    EXPECT_EQ(ibus::input_context::kServiceInterface,
              method_call->GetInterface());
    EXPECT_EQ(ibus::input_context::kResetMethod, method_call->GetMember());
    dbus::MessageReader reader(method_call);
    EXPECT_FALSE(reader.HasMoreData());

    message_loop_.PostTask(FROM_HERE, base::Bind(callback, response_));
  }

  // Handles SetCursorLocation method call.
  void OnSetCursorLocation(const ibus::Rect& cursor_location,
                           const ibus::Rect& composition_head) {
    ++on_set_cursor_location_call_count_;
  }

  // Handles SetCapabilities method call.
  void OnSetCapabilities(
      dbus::MethodCall* method_call,
      int timeout_ms,
      const dbus::ObjectProxy::ResponseCallback& callback,
      const dbus::ObjectProxy::ErrorCallback& error_callback) {
    EXPECT_EQ(ibus::input_context::kServiceInterface,
              method_call->GetInterface());
    EXPECT_EQ(ibus::input_context::kSetCapabilitiesMethod,
              method_call->GetMember());
    uint32 capabilities;
    dbus::MessageReader reader(method_call);
    EXPECT_TRUE(reader.PopUint32(&capabilities));
    EXPECT_EQ(kCapabilities, capabilities);
    EXPECT_FALSE(reader.HasMoreData());

    message_loop_.PostTask(FROM_HERE, base::Bind(callback, response_));
  }

  // Handles ProcessKeyEvent method call.
  void OnProcessKeyEvent(
      dbus::MethodCall* method_call,
      int timeout_ms,
      const dbus::ObjectProxy::ResponseCallback& callback,
      const dbus::ObjectProxy::ErrorCallback& error_callback) {
    EXPECT_EQ(ibus::input_context::kServiceInterface,
              method_call->GetInterface());
    EXPECT_EQ(ibus::input_context::kProcessKeyEventMethod,
              method_call->GetMember());
    uint32 keyval, keycode, state;
    dbus::MessageReader reader(method_call);
    EXPECT_TRUE(reader.PopUint32(&keyval));
    EXPECT_TRUE(reader.PopUint32(&keycode));
    EXPECT_TRUE(reader.PopUint32(&state));
    EXPECT_FALSE(reader.HasMoreData());

    message_loop_.PostTask(FROM_HERE, base::Bind(callback, response_));
  }

  void OnProcessKeyEventFail(
      dbus::MethodCall* method_call,
      int timeout_ms,
      const dbus::ObjectProxy::ResponseCallback& callback,
      const dbus::ObjectProxy::ErrorCallback& error_callback) {
    EXPECT_EQ(ibus::input_context::kServiceInterface,
              method_call->GetInterface());
    EXPECT_EQ(ibus::input_context::kProcessKeyEventMethod,
              method_call->GetMember());
    uint32 keyval, keycode, state;
    dbus::MessageReader reader(method_call);
    EXPECT_TRUE(reader.PopUint32(&keyval));
    EXPECT_TRUE(reader.PopUint32(&keycode));
    EXPECT_TRUE(reader.PopUint32(&state));
    EXPECT_FALSE(reader.HasMoreData());

    message_loop_.PostTask(FROM_HERE, base::Bind(error_callback,
                                                 error_response_));
  }

  // Handles SetSurroudingText method call.
  void OnSetSurroundingText(
      dbus::MethodCall* method_call,
      int timeout_ms,
      const dbus::ObjectProxy::ResponseCallback& callback,
      const dbus::ObjectProxy::ErrorCallback& error_callback) {
    EXPECT_EQ(ibus::input_context::kServiceInterface,
              method_call->GetInterface());
    EXPECT_EQ(ibus::input_context::kSetSurroundingTextMethod,
              method_call->GetMember());
    dbus::MessageReader reader(method_call);
    std::string text;
    uint32 cursor_pos = 0;
    uint32 anchor_pos = 0;

    EXPECT_TRUE(ibus::PopStringFromIBusText(&reader, &text));
    EXPECT_TRUE(reader.PopUint32(&cursor_pos));
    EXPECT_TRUE(reader.PopUint32(&anchor_pos));
    EXPECT_FALSE(reader.HasMoreData());

    EXPECT_EQ(kSurroundingText, text);
    EXPECT_EQ(kCursorPos, cursor_pos);
    EXPECT_EQ(kAnchorPos, anchor_pos);

    message_loop_.PostTask(FROM_HERE, base::Bind(callback, response_));
  }

  // Handles PropertyActivate method call.
  void OnPropertyActivate(
      dbus::MethodCall* method_call,
      int timeout_ms,
      const dbus::ObjectProxy::ResponseCallback& callback,
      const dbus::ObjectProxy::ErrorCallback& error_callback) {
    EXPECT_EQ(ibus::input_context::kServiceInterface,
              method_call->GetInterface());
    EXPECT_EQ(ibus::input_context::kPropertyActivateMethod,
              method_call->GetMember());
    dbus::MessageReader reader(method_call);
    std::string key;
    uint32 state = 0;

    EXPECT_TRUE(reader.PopString(&key));
    EXPECT_TRUE(reader.PopUint32(&state));
    EXPECT_FALSE(reader.HasMoreData());

    EXPECT_EQ(kPropertyKey, key);
    EXPECT_EQ(kPropertyState, static_cast<ibus::IBusPropertyState>(state));

    message_loop_.PostTask(FROM_HERE, base::Bind(callback, response_));
  }

 protected:
  // The client to be tested.
  scoped_ptr<IBusInputContextClient> client_;
  // The mock bus.
  scoped_refptr<dbus::MockBus> mock_bus_;
  // The mock object proxy.
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  // Response returned by mock methods.
  dbus::Response* response_;
  dbus::ErrorResponse* error_response_;
  // A message loop to emulate asynchronous behavior.
  MessageLoop message_loop_;
  // The map from signal to signal handler.
  std::map<std::string, dbus::ObjectProxy::SignalCallback> signal_callback_map_;
  // Call count of OnSetCursorLocation.
  int on_set_cursor_location_call_count_;

 private:
  // Used to implement the mock proxy.
  void OnConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      const dbus::ObjectProxy::OnConnectedCallback& on_connected_callback) {
    signal_callback_map_[signal_name] = signal_callback;
    const bool success = true;
    message_loop_.PostTask(FROM_HERE, base::Bind(on_connected_callback,
                                                 interface_name,
                                                 signal_name,
                                                 success));
  }
};

TEST_F(IBusInputContextClientTest, CommitTextHandler) {
  const char kSampleText[] = "Sample Text";
  IBusText ibus_text;
  ibus_text.set_text(kSampleText);

  // Set handler expectations.
  MockInputContextHandler mock_handler;
  EXPECT_CALL(mock_handler, CommitText(IBusTextEq(&ibus_text)));
  client_->SetInputContextHandler(&mock_handler);
  message_loop_.RunUntilIdle();

  // Emit signal.
  dbus::Signal signal(ibus::input_context::kServiceInterface,
                      ibus::input_context::kCommitTextSignal);
  dbus::MessageWriter writer(&signal);
  AppendIBusText(ibus_text, &writer);
  ASSERT_FALSE(
      signal_callback_map_[ibus::input_context::kCommitTextSignal].is_null());
  signal_callback_map_[ibus::input_context::kCommitTextSignal].Run(&signal);

  // Unset the handler so expect not calling handler.
  client_->SetInputContextHandler(NULL);
  signal_callback_map_[ibus::input_context::kCommitTextSignal].Run(&signal);
}

TEST_F(IBusInputContextClientTest, ForwardKeyEventHandlerTest) {
  // Set handler expectations.
  MockInputContextHandler mock_handler;
  EXPECT_CALL(mock_handler, ForwardKeyEvent(kKeyval, kKeycode, kState));
  client_->SetInputContextHandler(&mock_handler);
  message_loop_.RunUntilIdle();

  // Emit signal.
  dbus::Signal signal(ibus::input_context::kServiceInterface,
                      ibus::input_context::kForwardKeyEventSignal);
  dbus::MessageWriter writer(&signal);
  writer.AppendUint32(kKeyval);
  writer.AppendUint32(kKeycode);
  writer.AppendUint32(kState);
  ASSERT_FALSE(
      signal_callback_map_[ibus::input_context::kForwardKeyEventSignal]
          .is_null());
  signal_callback_map_[ibus::input_context::kForwardKeyEventSignal].Run(
      &signal);

  // Unset the handler so expect not calling handler.
  client_->SetInputContextHandler(NULL);
  signal_callback_map_[ibus::input_context::kForwardKeyEventSignal].Run(
      &signal);
}

TEST_F(IBusInputContextClientTest, HidePreeditTextHandlerTest) {
  // Set handler expectations.
  MockInputContextHandler mock_handler;
  EXPECT_CALL(mock_handler, HidePreeditText());
  client_->SetInputContextHandler(&mock_handler);
  message_loop_.RunUntilIdle();

  // Emit signal.
  dbus::Signal signal(ibus::input_context::kServiceInterface,
                      ibus::input_context::kHidePreeditTextSignal);
  ASSERT_FALSE(
      signal_callback_map_[ibus::input_context::kHidePreeditTextSignal]
          .is_null());
  signal_callback_map_[ibus::input_context::kHidePreeditTextSignal].Run(
      &signal);

  // Unset the handler so expect not calling handler.
  client_->SetInputContextHandler(NULL);
  signal_callback_map_[ibus::input_context::kHidePreeditTextSignal].Run(
      &signal);
}

TEST_F(IBusInputContextClientTest, ShowPreeditTextHandlerTest) {
  // Set handler expectations.
  MockInputContextHandler mock_handler;
  EXPECT_CALL(mock_handler, ShowPreeditText());
  client_->SetInputContextHandler(&mock_handler);
  message_loop_.RunUntilIdle();

  // Emit signal.
  dbus::Signal signal(ibus::input_context::kServiceInterface,
                      ibus::input_context::kShowPreeditTextSignal);
  ASSERT_FALSE(
      signal_callback_map_[ibus::input_context::kShowPreeditTextSignal]
          .is_null());
  signal_callback_map_[ibus::input_context::kShowPreeditTextSignal].Run(
      &signal);

  // Unset the handler so expect not calling handler.
  client_->SetInputContextHandler(NULL);
  signal_callback_map_[ibus::input_context::kShowPreeditTextSignal].Run(
      &signal);
}

TEST_F(IBusInputContextClientTest, UpdatePreeditTextHandlerTest) {
  const uint32 kCursorPos = 20;
  const bool kVisible = false;
  const char kSampleText[] = "Sample Text";
  IBusText ibus_text;
  ibus_text.set_text(kSampleText);

  // Set handler expectations.
  MockInputContextHandler mock_handler;
  EXPECT_CALL(mock_handler,
              UpdatePreeditText(IBusTextEq(&ibus_text), kCursorPos, kVisible));
  client_->SetInputContextHandler(&mock_handler);
  message_loop_.RunUntilIdle();

  // Emit signal.
  dbus::Signal signal(ibus::input_context::kServiceInterface,
                      ibus::input_context::kUpdatePreeditTextSignal);
  dbus::MessageWriter writer(&signal);
  AppendIBusText(ibus_text, &writer);
  writer.AppendUint32(kCursorPos);
  writer.AppendBool(kVisible);
  ASSERT_FALSE(
      signal_callback_map_[ibus::input_context::kUpdatePreeditTextSignal]
          .is_null());
  signal_callback_map_[ibus::input_context::kUpdatePreeditTextSignal].Run(
      &signal);

  // Unset the handler so expect not calling handler.
  client_->SetInputContextHandler(NULL);
  signal_callback_map_[ibus::input_context::kUpdatePreeditTextSignal].Run(
      &signal);
}

TEST_F(IBusInputContextClientTest, FocusInTest) {
  // Set expectations.
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(this, &IBusInputContextClientTest::OnFocusIn));
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  response_ = response.get();

  // Call FocusIn.
  client_->FocusIn();
  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(IBusInputContextClientTest, FocusOutTest) {
  // Set expectations.
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(this, &IBusInputContextClientTest::OnFocusOut));
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  response_ = response.get();

  // Call FocusOut.
  client_->FocusOut();
  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(IBusInputContextClientTest, ResetTest) {
  // Set expectations.
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(this, &IBusInputContextClientTest::OnReset));
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  response_ = response.get();

  // Call Reset.
  client_->Reset();
  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(IBusInputContextClientTest, SetCapabilitiesTest) {
  // Set expectations.
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(this, &IBusInputContextClientTest::OnSetCapabilities));
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  response_ = response.get();

  // Call SetCapabilities.
  client_->SetCapabilities(kCapabilities);
  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(IBusInputContextClientTest, SetCursorLocationTest) {
  on_set_cursor_location_call_count_ = 0;
  client_->SetSetCursorLocationHandler(
      base::Bind(&IBusInputContextClientTest::OnSetCursorLocation,
                 base::Unretained(this)));
  const ibus::Rect cursor_location(kCursorX,
                                   kCursorY,
                                   kCursorWidth,
                                   kCursorHeight);
  const ibus::Rect composition_location(kCompositionX,
                                        kCompositionY,
                                        kCompositionWidth,
                                        kCompositionHeight);
  // Call SetCursorLocation.
  client_->SetCursorLocation(cursor_location, composition_location);

  EXPECT_EQ(1, on_set_cursor_location_call_count_);
  client_->UnsetSetCursorLocationHandler();
}

TEST_F(IBusInputContextClientTest, OnProcessKeyEvent) {
  // Set expectations.
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(this, &IBusInputContextClientTest::OnProcessKeyEvent));
  MockProcessKeyEventHandler callback;
  MockProcessKeyEventErrorHandler error_callback;

  EXPECT_CALL(callback, Run(kIsKeyHandled));
  EXPECT_CALL(error_callback, Run()).Times(0);
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(kIsKeyHandled);
  response_ = response.get();

  // Call ProcessKeyEvent.
  client_->ProcessKeyEvent(kKeyval,
                           kKeycode,
                           kState,
                           base::Bind(&MockProcessKeyEventHandler::Run,
                                      base::Unretained(&callback)),
                           base::Bind(&MockProcessKeyEventErrorHandler::Run,
                                      base::Unretained(&error_callback)));
  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(IBusInputContextClientTest, OnProcessKeyEventFail) {
  // Set expectations.
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(this,
                       &IBusInputContextClientTest::OnProcessKeyEventFail));
  MockProcessKeyEventHandler callback;
  MockProcessKeyEventErrorHandler error_callback;

  EXPECT_CALL(callback, Run(_)).Times(0);
  EXPECT_CALL(error_callback, Run());
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(kIsKeyHandled);
  response_ = response.get();

  // Call ProcessKeyEvent.
  client_->ProcessKeyEvent(kKeyval,
                           kKeycode,
                           kState,
                           base::Bind(&MockProcessKeyEventHandler::Run,
                                      base::Unretained(&callback)),
                           base::Bind(&MockProcessKeyEventErrorHandler::Run,
                                      base::Unretained(&error_callback)));
  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(IBusInputContextClientTest, SetSurroundingTextTest) {
  // Set expectations.
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(this,
                       &IBusInputContextClientTest::OnSetSurroundingText));
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  response_ = response.get();

  // Call SetCursorLocation.
  client_->SetSurroundingText(kSurroundingText, kCursorPos, kAnchorPos);
  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(IBusInputContextClientTest, PropertyActivateTest) {
  // Set expectations.
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(this,
                       &IBusInputContextClientTest::OnPropertyActivate));
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  response_ = response.get();

  // Call SetCursorLocation.
  client_->PropertyActivate(kPropertyKey, kPropertyState);
  // Run the message loop.
  message_loop_.RunUntilIdle();
}
}  // namespace chromeos
