// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_engine_service.h"

#include <map>
#include "base/bind.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "chromeos/dbus/ibus/ibus_lookup_table.h"
#include "chromeos/dbus/ibus/ibus_property.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::Return;
using testing::_;

namespace chromeos {

namespace {
const std::string kObjectPath = "/org/freedesktop/IBus/Engine/1";

class MockIBusEngineHandler : public IBusEngineHandlerInterface {
 public:
  MOCK_METHOD0(FocusIn, void());
  MOCK_METHOD0(FocusOut, void());
  MOCK_METHOD0(Enable, void());
  MOCK_METHOD0(Disable, void());
  MOCK_METHOD2(PropertyActivate, void(const std::string& property_name,
                                      ibus::IBusPropertyState property_state));
  MOCK_METHOD1(PropertyShow, void(const std::string& property_name));
  MOCK_METHOD1(PropertyHide, void(const std::string& property_name));
  MOCK_METHOD1(SetCapability, void(IBusCapability capability));
  MOCK_METHOD0(Reset, void());
  MOCK_METHOD4(ProcessKeyEvent, void(
      uint32 keysym,
      uint32 keycode,
      uint32 state,
      const KeyEventDoneCallback& callback));
  MOCK_METHOD3(CandidateClicked, void(uint32 index,
                                      ibus::IBusMouseButton button,
                                      uint32 state));
  MOCK_METHOD3(SetSurroundingText, void(const std::string& text,
                                        uint32 cursor_pos,
                                        uint32 anchor_pos));
};

class MockResponseSender {
 public:
  // GMock doesn't support mocking methods which take scoped_ptr<>.
  MOCK_METHOD1(MockRun, void(dbus::Response* reponse));
  void Run(scoped_ptr<dbus::Response> response) {
    MockRun(response.get());
  }
};

// Used for method call empty response evaluation.
class EmptyResponseExpectation {
 public:
  explicit EmptyResponseExpectation(const uint32 serial_no)
      : serial_no_(serial_no) {}

  // Evaluates the given |response| has no argument.
  void Evaluate(dbus::Response* response) {
    EXPECT_EQ(serial_no_, response->GetReplySerial());
    dbus::MessageReader reader(response);
    EXPECT_FALSE(reader.HasMoreData());
  }

 private:
  const uint32 serial_no_;

  DISALLOW_COPY_AND_ASSIGN(EmptyResponseExpectation);
};

// Used for method call a boolean response evaluation.
class BoolResponseExpectation {
 public:
  explicit BoolResponseExpectation(uint32 serial_no, bool result)
      : serial_no_(serial_no),
        result_(result) {}

  // Evaluates the given |response| has only one boolean and which is equals to
  // |result_| which is given in ctor.
  void Evaluate(dbus::Response* response) {
    EXPECT_EQ(serial_no_, response->GetReplySerial());
    dbus::MessageReader reader(response);
    bool result = false;
    EXPECT_TRUE(reader.PopBool(&result));
    EXPECT_EQ(result_, result);
    EXPECT_FALSE(reader.HasMoreData());
  }

 private:
  uint32 serial_no_;
  bool result_;

  DISALLOW_COPY_AND_ASSIGN(BoolResponseExpectation);
};

// Used for RegisterProperties signal message evaluation.
class RegisterPropertiesExpectation {
 public:
  explicit RegisterPropertiesExpectation(
      const IBusPropertyList& property_list)
      : property_list_(property_list) {}

  // Evaluates the given |signal| is a valid message.
  void Evaluate(dbus::Signal* signal) {
    IBusPropertyList property_list;

    // Read a signal argument.
    dbus::MessageReader reader(signal);
    EXPECT_TRUE(PopIBusPropertyList(&reader, &property_list));
    EXPECT_FALSE(reader.HasMoreData());

    // Check an argument.
    EXPECT_EQ(property_list_.size(), property_list.size());
    for (size_t i = 0; i < property_list_.size(); ++i) {
      EXPECT_EQ(property_list_[i]->key(), property_list[i]->key());
      EXPECT_EQ(property_list_[i]->type(), property_list[i]->type());
      EXPECT_EQ(property_list_[i]->label(), property_list[i]->label());
      EXPECT_EQ(property_list_[i]->tooltip(), property_list[i]->tooltip());
      EXPECT_EQ(property_list_[i]->visible(), property_list[i]->visible());
      EXPECT_EQ(property_list_[i]->checked(), property_list[i]->checked());
    }
  }

 private:
  const IBusPropertyList& property_list_;

  DISALLOW_COPY_AND_ASSIGN(RegisterPropertiesExpectation);
};

// Used for mocking ProcessKeyEventHandler.
class ProcessKeyEventHandler {
 public:
  explicit ProcessKeyEventHandler(bool expected_value)
      : expected_value_(expected_value) {
  }

  void ProcessKeyEvent(
      uint32 keysym,
      uint32 keycode,
      uint32 state,
      const IBusEngineHandlerInterface::KeyEventDoneCallback& callback) {
    callback.Run(expected_value_);
  }

 private:
  bool expected_value_;

  DISALLOW_COPY_AND_ASSIGN(ProcessKeyEventHandler);
};

// Used for mocking asynchronous ProcessKeyEventHandler.
class DelayProcessKeyEventHandler {
 public:
  DelayProcessKeyEventHandler(bool expected_value,
                              MessageLoop* message_loop)
      : expected_value_(expected_value),
        message_loop_(message_loop) {
  }

  void ProcessKeyEvent(
      uint32 keysym,
      uint32 keycode,
      uint32 state,
      const IBusEngineHandlerInterface::KeyEventDoneCallback& callback) {
    message_loop_->PostTask(FROM_HERE, base::Bind(callback, expected_value_));
  }

 private:
  bool expected_value_;
  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(DelayProcessKeyEventHandler);
};

// Used for UpdatePreedit signal message evaluation.
class UpdatePreeditExpectation {
 public:
  UpdatePreeditExpectation(
      const IBusText& ibus_text,
      uint32 cursor_pos,
      bool is_visible,
      IBusEngineService::IBusEnginePreeditFocusOutMode mode)
      : ibus_text_(ibus_text),
        cursor_pos_(cursor_pos),
        is_visible_(is_visible),
        mode_(mode) {}

  // Evaluates the given |signal| is a valid message.
  void Evaluate(dbus::Signal* signal) {
    IBusText ibus_text;
    uint32 cursor_pos = 0;
    bool is_visible = false;
    uint32 preedit_mode = 0;

    // Read signal arguments.
    dbus::MessageReader reader(signal);
    EXPECT_TRUE(PopIBusText(&reader, &ibus_text));
    EXPECT_TRUE(reader.PopUint32(&cursor_pos));
    EXPECT_TRUE(reader.PopBool(&is_visible));
    EXPECT_TRUE(reader.PopUint32(&preedit_mode));
    EXPECT_FALSE(reader.HasMoreData());

    // Check arguments.
    EXPECT_EQ(ibus_text_.text(), ibus_text.text());
    EXPECT_EQ(cursor_pos_, cursor_pos);
    EXPECT_EQ(is_visible_, is_visible);
    EXPECT_EQ(mode_,
              static_cast<IBusEngineService::IBusEnginePreeditFocusOutMode>(
                  preedit_mode));
  }

 private:
  const IBusText& ibus_text_;
  uint32 cursor_pos_;
  bool is_visible_;
  IBusEngineService::IBusEnginePreeditFocusOutMode mode_;

  DISALLOW_COPY_AND_ASSIGN(UpdatePreeditExpectation);
};

// Used for UpdateAuxiliaryText signal message evaluation.
class UpdateAuxiliaryTextExpectation {
 public:
  UpdateAuxiliaryTextExpectation(const IBusText& ibus_text,
                                 bool is_visible)
      : ibus_text_(ibus_text), is_visible_(is_visible) {}

  // Evaluates the given |signal| is a valid message.
  void Evaluate(dbus::Signal* signal) {
    IBusText ibus_text;
    bool is_visible = false;

    // Read signal arguments.
    dbus::MessageReader reader(signal);
    EXPECT_TRUE(PopIBusText(&reader, &ibus_text));
    EXPECT_TRUE(reader.PopBool(&is_visible));
    EXPECT_FALSE(reader.HasMoreData());

    // Check arguments.
    EXPECT_EQ(ibus_text_.text(), ibus_text.text());
    EXPECT_EQ(is_visible_, is_visible);
  }

 private:
  const IBusText& ibus_text_;
  bool is_visible_;

  DISALLOW_COPY_AND_ASSIGN(UpdateAuxiliaryTextExpectation);
};

// Used for UpdateLookupTable signal message evaluation.
class UpdateLookupTableExpectation {
 public:
  UpdateLookupTableExpectation(const IBusLookupTable& lookup_table,
                               bool is_visible)
      : lookup_table_(lookup_table), is_visible_(is_visible) {}

  // Evaluates the given |signal| is a valid message.
  void Evaluate(dbus::Signal* signal) {
    IBusLookupTable lookup_table;
    bool is_visible = false;

    // Read signal arguments.
    dbus::MessageReader reader(signal);
    EXPECT_TRUE(PopIBusLookupTable(&reader, &lookup_table));
    EXPECT_TRUE(reader.PopBool(&is_visible));
    EXPECT_FALSE(reader.HasMoreData());

    // Check arguments.
    EXPECT_EQ(lookup_table_.page_size(), lookup_table.page_size());
    EXPECT_EQ(lookup_table_.cursor_position(), lookup_table.cursor_position());
    EXPECT_EQ(lookup_table_.is_cursor_visible(),
              lookup_table.is_cursor_visible());
    EXPECT_EQ(is_visible_, is_visible);
  }

 private:
  const IBusLookupTable& lookup_table_;
  bool is_visible_;

  DISALLOW_COPY_AND_ASSIGN(UpdateLookupTableExpectation);
};

// Used for UpdateProperty signal message evaluation.
class UpdatePropertyExpectation {
 public:
  explicit UpdatePropertyExpectation(const IBusProperty& property)
      : property_(property) {}

  // Evaluates the given |signal| is a valid message.
  void Evaluate(dbus::Signal* signal) {
    IBusProperty property;

    // Read a signal argument.
    dbus::MessageReader reader(signal);
    EXPECT_TRUE(PopIBusProperty(&reader, &property));
    EXPECT_FALSE(reader.HasMoreData());

    // Check the argument.
    EXPECT_EQ(property_.key(), property.key());
    EXPECT_EQ(property_.type(), property.type());
    EXPECT_EQ(property_.label(), property.label());
    EXPECT_EQ(property_.tooltip(), property.tooltip());
    EXPECT_EQ(property_.visible(), property.visible());
    EXPECT_EQ(property_.checked(), property.checked());
  }

 private:
  const IBusProperty& property_;

  DISALLOW_COPY_AND_ASSIGN(UpdatePropertyExpectation);
};

// Used for ForwardKeyEvent signal message evaluation.
class ForwardKeyEventExpectation {
 public:
  ForwardKeyEventExpectation(uint32 keyval, uint32 keycode, uint32 state)
      : keyval_(keyval),
        keycode_(keycode),
        state_(state) {}

  // Evaluates the given |signal| is a valid message.
  void Evaluate(dbus::Signal* signal) {
    uint32 keyval = 0;
    uint32 keycode = 0;
    uint32 state = 0;

    // Read signal arguments.
    dbus::MessageReader reader(signal);
    EXPECT_TRUE(reader.PopUint32(&keyval));
    EXPECT_TRUE(reader.PopUint32(&keycode));
    EXPECT_TRUE(reader.PopUint32(&state));
    EXPECT_FALSE(reader.HasMoreData());

    // Check arguments.
    EXPECT_EQ(keyval_, keyval);
    EXPECT_EQ(keycode_, keycode);
    EXPECT_EQ(state_, state);
  }

 private:
  uint32 keyval_;
  uint32 keycode_;
  uint32 state_;

  DISALLOW_COPY_AND_ASSIGN(ForwardKeyEventExpectation);
};

// Used for RequireSurroundingText signal message evaluation.
class RequireSurroundingTextExpectation {
 public:
  RequireSurroundingTextExpectation() {}

  // Evaluates the given |signal| is a valid message.
  void Evaluate(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    EXPECT_FALSE(reader.HasMoreData());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RequireSurroundingTextExpectation);
};

}  // namespace

class IBusEngineServiceTest : public testing::Test {
 public:
  IBusEngineServiceTest() {}

  virtual void SetUp() OVERRIDE {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create a mock exported object.
    mock_exported_object_ = new dbus::MockExportedObject(
        mock_bus_.get(),
        dbus::ObjectPath(kObjectPath));

    EXPECT_CALL(*mock_bus_.get(),
                GetExportedObject(dbus::ObjectPath(kObjectPath)))
        .WillOnce(Return(mock_exported_object_.get()));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kFocusInMethod , _, _))
        .WillRepeatedly(
            Invoke(this, &IBusEngineServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kFocusOutMethod , _, _))
        .WillRepeatedly(
            Invoke(this, &IBusEngineServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kEnableMethod , _, _))
        .WillRepeatedly(
            Invoke(this, &IBusEngineServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kDisableMethod , _, _))
        .WillRepeatedly(
            Invoke(this, &IBusEngineServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kPropertyActivateMethod , _, _))
        .WillRepeatedly(
            Invoke(this, &IBusEngineServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kPropertyShowMethod , _, _))
        .WillRepeatedly(
            Invoke(this, &IBusEngineServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kPropertyHideMethod , _, _))
        .WillRepeatedly(
            Invoke(this, &IBusEngineServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kSetCapabilityMethod , _, _))
        .WillRepeatedly(
            Invoke(this, &IBusEngineServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kResetMethod , _, _))
        .WillRepeatedly(
            Invoke(this, &IBusEngineServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kProcessKeyEventMethod , _, _))
        .WillRepeatedly(
            Invoke(this, &IBusEngineServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kCandidateClickedMethod , _, _))
        .WillRepeatedly(
            Invoke(this, &IBusEngineServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::engine::kServiceInterface,
        ibus::engine::kSetSurroundingTextMethod , _, _))
        .WillRepeatedly(
            Invoke(this, &IBusEngineServiceTest::OnMethodExported));

    // Suppress uninteresting mock function call warning.
    EXPECT_CALL(*mock_bus_.get(),
                AssertOnOriginThread())
        .WillRepeatedly(Return());

    // Create a service
    service_.reset(IBusEngineService::Create(
        REAL_DBUS_CLIENT_IMPLEMENTATION,
        mock_bus_.get(),
        dbus::ObjectPath(kObjectPath)));

    // Set engine handler.
    engine_handler_.reset(new MockIBusEngineHandler());
    service_->SetEngine(engine_handler_.get());
  }

 protected:
  // The service to be tested.
  scoped_ptr<IBusEngineService> service_;
  // The mock engine handler. Do not free, this is owned by IBusEngineService.
  scoped_ptr<MockIBusEngineHandler> engine_handler_;
  // The mock bus.
  scoped_refptr<dbus::MockBus> mock_bus_;
  // The mock exported object.
  scoped_refptr<dbus::MockExportedObject> mock_exported_object_;
  // A message loop to emulate asynchronous behavior.
  MessageLoop message_loop_;
  // The map from method call to method call handler.
  std::map<std::string, dbus::ExportedObject::MethodCallCallback>
      method_callback_map_;

 private:
  // Used to implement the mock method call.
  void OnMethodExported(
      const std::string& interface_name,
      const std::string& method_name,
      const dbus::ExportedObject::MethodCallCallback& method_callback,
      const dbus::ExportedObject::OnExportedCallback& on_exported_callback) {
    method_callback_map_[method_name] = method_callback;
    const bool success = true;
    message_loop_.PostTask(FROM_HERE, base::Bind(on_exported_callback,
                                                 interface_name,
                                                 method_name,
                                                 success));
  }
};

TEST_F(IBusEngineServiceTest, FocusInTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  EXPECT_CALL(*engine_handler_, FocusIn());
  MockResponseSender response_sender;
  EmptyResponseExpectation response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseExpectation::Evaluate));

  // Create method call;
  dbus::MethodCall method_call(ibus::engine::kServiceInterface,
                               ibus::engine::kFocusInMethod);
  method_call.SetSerial(kSerialNo);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::engine::kFocusInMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::engine::kFocusInMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Call exported function without engine.
  service_->UnsetEngine();
  EXPECT_CALL(*engine_handler_, FocusIn()).Times(0);
  EXPECT_CALL(response_sender, MockRun(_)).Times(0);
  method_callback_map_[ibus::engine::kFocusInMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusEngineServiceTest, FocusOutTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  EXPECT_CALL(*engine_handler_, FocusOut());
  MockResponseSender response_sender;
  EmptyResponseExpectation response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseExpectation::Evaluate));

  // Create method call;
  dbus::MethodCall method_call(ibus::engine::kServiceInterface,
                               ibus::engine::kFocusOutMethod);
  method_call.SetSerial(kSerialNo);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::engine::kFocusOutMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::engine::kFocusOutMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Call exported function without engine.
  service_->UnsetEngine();
  EXPECT_CALL(*engine_handler_, FocusOut()).Times(0);
  EXPECT_CALL(response_sender, MockRun(_)).Times(0);
  method_callback_map_[ibus::engine::kFocusOutMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusEngineServiceTest, EnableTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  EXPECT_CALL(*engine_handler_, Enable());
  MockResponseSender response_sender;
  EmptyResponseExpectation response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseExpectation::Evaluate));

  // Create method call;
  dbus::MethodCall method_call(ibus::engine::kServiceInterface,
                               ibus::engine::kEnableMethod);
  method_call.SetSerial(kSerialNo);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::engine::kEnableMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::engine::kEnableMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Call exported function without engine.
  service_->UnsetEngine();
  EXPECT_CALL(*engine_handler_, Enable()).Times(0);
  EXPECT_CALL(response_sender, MockRun(_)).Times(0);
  method_callback_map_[ibus::engine::kEnableMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusEngineServiceTest, DisableTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  EXPECT_CALL(*engine_handler_, Disable());
  MockResponseSender response_sender;
  EmptyResponseExpectation response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseExpectation::Evaluate));

  // Create method call;
  dbus::MethodCall method_call(ibus::engine::kServiceInterface,
                               ibus::engine::kDisableMethod);
  method_call.SetSerial(kSerialNo);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::engine::kDisableMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::engine::kDisableMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Call exported function without engine.
  service_->UnsetEngine();
  EXPECT_CALL(*engine_handler_, Disable()).Times(0);
  EXPECT_CALL(response_sender, MockRun(_)).Times(0);
  method_callback_map_[ibus::engine::kDisableMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusEngineServiceTest, PropertyActivateTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  const std::string kPropertyName = "Property Name";
  const ibus::IBusPropertyState kIBusPropertyState =
      ibus::IBUS_PROPERTY_STATE_UNCHECKED;
  EXPECT_CALL(*engine_handler_, PropertyActivate(kPropertyName,
                                                 kIBusPropertyState));
  MockResponseSender response_sender;
  EmptyResponseExpectation response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseExpectation::Evaluate));

  // Create method call;
  dbus::MethodCall method_call(ibus::engine::kServiceInterface,
                               ibus::engine::kPropertyActivateMethod);
  method_call.SetSerial(kSerialNo);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kPropertyName);
  writer.AppendUint32(static_cast<uint32>(kIBusPropertyState));

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::engine::kPropertyActivateMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::engine::kPropertyActivateMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Call exported function without engine.
  service_->UnsetEngine();
  EXPECT_CALL(*engine_handler_, PropertyActivate(kPropertyName,
                                                 kIBusPropertyState)).Times(0);
  EXPECT_CALL(response_sender, MockRun(_)).Times(0);
  method_callback_map_[ibus::engine::kPropertyActivateMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusEngineServiceTest, ResetTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  EXPECT_CALL(*engine_handler_, Reset());
  MockResponseSender response_sender;
  EmptyResponseExpectation response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseExpectation::Evaluate));

  // Create method call;
  dbus::MethodCall method_call(ibus::engine::kServiceInterface,
                               ibus::engine::kResetMethod);
  method_call.SetSerial(kSerialNo);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::engine::kResetMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::engine::kResetMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Call exported function without engine.
  service_->UnsetEngine();
  EXPECT_CALL(*engine_handler_, Reset()).Times(0);
  EXPECT_CALL(response_sender, MockRun(_)).Times(0);
  method_callback_map_[ibus::engine::kResetMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusEngineServiceTest, PropertyShowTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  const std::string kPropertyName = "Property Name";
  EXPECT_CALL(*engine_handler_, PropertyShow(kPropertyName));
  MockResponseSender response_sender;
  EmptyResponseExpectation response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseExpectation::Evaluate));

  // Create method call;
  dbus::MethodCall method_call(ibus::engine::kServiceInterface,
                               ibus::engine::kPropertyShowMethod);
  method_call.SetSerial(kSerialNo);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kPropertyName);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::engine::kPropertyShowMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::engine::kPropertyShowMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Call exported function without engine.
  service_->UnsetEngine();
  EXPECT_CALL(*engine_handler_, PropertyShow(kPropertyName)).Times(0);
  EXPECT_CALL(response_sender, MockRun(_)).Times(0);
  method_callback_map_[ibus::engine::kPropertyShowMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusEngineServiceTest, PropertyHideTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  const std::string kPropertyName = "Property Name";
  EXPECT_CALL(*engine_handler_, PropertyHide(kPropertyName));
  MockResponseSender response_sender;
  EmptyResponseExpectation response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseExpectation::Evaluate));

  // Create method call;
  dbus::MethodCall method_call(ibus::engine::kServiceInterface,
                               ibus::engine::kPropertyHideMethod);
  method_call.SetSerial(kSerialNo);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kPropertyName);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::engine::kPropertyHideMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::engine::kPropertyHideMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Call exported function without engine.
  service_->UnsetEngine();
  EXPECT_CALL(*engine_handler_, PropertyHide(kPropertyName)).Times(0);
  EXPECT_CALL(response_sender, MockRun(_)).Times(0);
  method_callback_map_[ibus::engine::kPropertyHideMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusEngineServiceTest, SetCapabilityTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  const IBusEngineHandlerInterface::IBusCapability kIBusCapability =
      IBusEngineHandlerInterface::IBUS_CAPABILITY_PREEDIT_TEXT;
  EXPECT_CALL(*engine_handler_, SetCapability(kIBusCapability));
  MockResponseSender response_sender;
  EmptyResponseExpectation response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseExpectation::Evaluate));

  // Create method call;
  dbus::MethodCall method_call(ibus::engine::kServiceInterface,
                               ibus::engine::kSetCapabilityMethod);
  method_call.SetSerial(kSerialNo);
  dbus::MessageWriter writer(&method_call);
  writer.AppendUint32(static_cast<uint32>(kIBusCapability));

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::engine::kSetCapabilityMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::engine::kSetCapabilityMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Call exported function without engine.
  service_->UnsetEngine();
  EXPECT_CALL(*engine_handler_, SetCapability(kIBusCapability)).Times(0);
  EXPECT_CALL(response_sender, MockRun(_)).Times(0);
  method_callback_map_[ibus::engine::kSetCapabilityMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusEngineServiceTest, ProcessKeyEventTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  const uint32 kKeySym = 0x64;
  const uint32 kKeyCode = 0x20;
  const uint32 kState = 0x00;
  const bool kResult = true;

  ProcessKeyEventHandler handler(kResult);
  EXPECT_CALL(*engine_handler_, ProcessKeyEvent(kKeySym, kKeyCode, kState, _))
      .WillOnce(Invoke(&handler,
                       &ProcessKeyEventHandler::ProcessKeyEvent));
  MockResponseSender response_sender;
  BoolResponseExpectation response_expectation(kSerialNo, kResult);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(Invoke(&response_expectation,
                       &BoolResponseExpectation::Evaluate));

  // Create method call;
  dbus::MethodCall method_call(ibus::engine::kServiceInterface,
                               ibus::engine::kProcessKeyEventMethod);
  method_call.SetSerial(kSerialNo);
  dbus::MessageWriter writer(&method_call);
  writer.AppendUint32(kKeySym);
  writer.AppendUint32(kKeyCode);
  writer.AppendUint32(kState);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::engine::kProcessKeyEventMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::engine::kProcessKeyEventMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Call exported function without engine.
  service_->UnsetEngine();
  EXPECT_CALL(*engine_handler_,
              ProcessKeyEvent(kKeySym, kKeyCode, kState, _)).Times(0);
  EXPECT_CALL(response_sender, MockRun(_)).Times(0);
  method_callback_map_[ibus::engine::kProcessKeyEventMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusEngineServiceTest, DelayProcessKeyEventTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  const uint32 kKeySym = 0x64;
  const uint32 kKeyCode = 0x20;
  const uint32 kState = 0x00;
  const bool kResult = true;

  DelayProcessKeyEventHandler handler(kResult, &message_loop_);
  EXPECT_CALL(*engine_handler_, ProcessKeyEvent(kKeySym, kKeyCode, kState, _))
      .WillOnce(Invoke(&handler,
                       &DelayProcessKeyEventHandler::ProcessKeyEvent));
  MockResponseSender response_sender;
  BoolResponseExpectation response_expectation(kSerialNo, kResult);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(Invoke(&response_expectation,
                       &BoolResponseExpectation::Evaluate));

  // Create method call;
  dbus::MethodCall method_call(ibus::engine::kServiceInterface,
                               ibus::engine::kProcessKeyEventMethod);
  method_call.SetSerial(kSerialNo);
  dbus::MessageWriter writer(&method_call);
  writer.AppendUint32(kKeySym);
  writer.AppendUint32(kKeyCode);
  writer.AppendUint32(kState);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::engine::kProcessKeyEventMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::engine::kProcessKeyEventMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Call KeyEventDone callback.
  message_loop_.RunUntilIdle();

  // Call exported function without engine.
  service_->UnsetEngine();
  EXPECT_CALL(*engine_handler_,
              ProcessKeyEvent(kKeySym, kKeyCode, kState, _)).Times(0);
  EXPECT_CALL(response_sender, MockRun(_)).Times(0);
  method_callback_map_[ibus::engine::kProcessKeyEventMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusEngineServiceTest, CandidateClickedTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  const uint32 kIndex = 4;
  const ibus::IBusMouseButton kIBusMouseButton = ibus::IBUS_MOUSE_BUTTON_MIDDLE;
  const uint32 kState = 3;
  EXPECT_CALL(*engine_handler_, CandidateClicked(kIndex, kIBusMouseButton,
                                                 kState));
  MockResponseSender response_sender;
  EmptyResponseExpectation response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseExpectation::Evaluate));

  // Create method call;
  dbus::MethodCall method_call(ibus::engine::kServiceInterface,
                               ibus::engine::kCandidateClickedMethod);
  method_call.SetSerial(kSerialNo);
  dbus::MessageWriter writer(&method_call);
  writer.AppendUint32(kIndex);
  writer.AppendUint32(static_cast<uint32>(kIBusMouseButton));
  writer.AppendUint32(kState);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::engine::kCandidateClickedMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::engine::kCandidateClickedMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Call exported function without engine.
  service_->UnsetEngine();
  EXPECT_CALL(*engine_handler_, CandidateClicked(kIndex, kIBusMouseButton,
                                                 kState)).Times(0);
  EXPECT_CALL(response_sender, MockRun(_)).Times(0);
  method_callback_map_[ibus::engine::kCandidateClickedMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusEngineServiceTest, SetSurroundingTextTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  const std::string kText = "Sample Text";
  const uint32 kCursorPos = 3;
  const uint32 kAnchorPos = 4;
  EXPECT_CALL(*engine_handler_, SetSurroundingText(kText, kCursorPos,
                                                   kAnchorPos));
  MockResponseSender response_sender;
  EmptyResponseExpectation response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseExpectation::Evaluate));

  // Create method call;
  dbus::MethodCall method_call(ibus::engine::kServiceInterface,
                               ibus::engine::kSetSurroundingTextMethod);
  method_call.SetSerial(kSerialNo);
  dbus::MessageWriter writer(&method_call);
  AppendStringAsIBusText(kText, &writer);
  writer.AppendUint32(kCursorPos);
  writer.AppendUint32(kAnchorPos);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::engine::kSetSurroundingTextMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::engine::kSetSurroundingTextMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Call exported function without engine.
  service_->UnsetEngine();
  EXPECT_CALL(*engine_handler_, SetSurroundingText(kText, kCursorPos,
                                                   kAnchorPos)).Times(0);
  EXPECT_CALL(response_sender, MockRun(_)).Times(0);
  method_callback_map_[ibus::engine::kSetSurroundingTextMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusEngineServiceTest, RegisterProperties) {
  // Set expectations.
  IBusPropertyList property_list;
  property_list.push_back(new IBusProperty());
  property_list[0]->set_key("Sample Key");
  property_list[0]->set_type(IBusProperty::IBUS_PROPERTY_TYPE_MENU);
  property_list[0]->set_label("Sample Label");
  property_list[0]->set_tooltip("Sample Tooltip");
  property_list[0]->set_visible(true);
  property_list[0]->set_checked(true);

  RegisterPropertiesExpectation expectation(property_list);
  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .WillOnce(Invoke(&expectation,
                       &RegisterPropertiesExpectation::Evaluate));
  // Emit signal.
  service_->RegisterProperties(property_list);
}

TEST_F(IBusEngineServiceTest, UpdatePreeditTest) {
  // Set expectations.
  IBusText ibus_text;
  ibus_text.set_text("Sample Text");
  const uint32 kCursorPos = 9;
  const bool kIsVisible = false;
  const IBusEngineService::IBusEnginePreeditFocusOutMode kPreeditMode =
      IBusEngineService::IBUS_ENGINE_PREEEDIT_FOCUS_OUT_MODE_CLEAR;
  UpdatePreeditExpectation expectation(ibus_text, kCursorPos, kIsVisible,
                                       kPreeditMode);
  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .WillOnce(Invoke(&expectation, &UpdatePreeditExpectation::Evaluate));

  // Emit signal.
  service_->UpdatePreedit(ibus_text, kCursorPos, kIsVisible, kPreeditMode);
}

TEST_F(IBusEngineServiceTest, UpdateAuxiliaryText) {
  IBusText ibus_text;
  ibus_text.set_text("Sample Text");
  const bool kIsVisible = false;
  UpdateAuxiliaryTextExpectation expectation(ibus_text, kIsVisible);

  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .WillOnce(Invoke(&expectation,
                       &UpdateAuxiliaryTextExpectation::Evaluate));

  // Emit signal.
  service_->UpdateAuxiliaryText(ibus_text, kIsVisible);
}

TEST_F(IBusEngineServiceTest, UpdateLookupTableTest) {
  IBusLookupTable lookup_table;
  lookup_table.set_page_size(10);
  lookup_table.set_cursor_position(2);
  lookup_table.set_is_cursor_visible(false);
  const bool kIsVisible = true;

  UpdateLookupTableExpectation expectation(lookup_table, kIsVisible);
  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .WillOnce(Invoke(&expectation,
                       &UpdateLookupTableExpectation::Evaluate));

  // Emit signal.
  service_->UpdateLookupTable(lookup_table, kIsVisible);
}

TEST_F(IBusEngineServiceTest, UpdatePropertyTest) {
  IBusProperty property;
  property.set_key("Sample Key");
  property.set_type(IBusProperty::IBUS_PROPERTY_TYPE_MENU);
  property.set_label("Sample Label");
  property.set_tooltip("Sample Tooltip");
  property.set_visible(true);
  property.set_checked(true);

  UpdatePropertyExpectation expectation(property);
  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .WillOnce(Invoke(&expectation,
                       &UpdatePropertyExpectation::Evaluate));

  // Emit signal.
  service_->UpdateProperty(property);
}

TEST_F(IBusEngineServiceTest, ForwardKeyEventTest) {
  uint32 keyval = 0x20;
  uint32 keycode = 0x64;
  uint32 state = 0x00;

  ForwardKeyEventExpectation expectation(keyval, keycode, state);

  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .WillOnce(Invoke(&expectation,
                       &ForwardKeyEventExpectation::Evaluate));

  // Emit signal.
  service_->ForwardKeyEvent(keyval, keycode, state);
}

TEST_F(IBusEngineServiceTest, RequireSurroundingTextTest) {
  RequireSurroundingTextExpectation expectation;
  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .WillOnce(Invoke(&expectation,
                       &RequireSurroundingTextExpectation::Evaluate));

  // Emit signal.
  service_->RequireSurroundingText();
}

}  // namespace chromeos
