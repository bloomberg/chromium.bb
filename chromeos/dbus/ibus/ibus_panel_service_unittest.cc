// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_panel_service.h"

#include <map>
#include "base/bind.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "chromeos/dbus/ibus/ibus_input_context_client.h"
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
// TODO(nona): Remove ibus namespace after complete libibus removal.
namespace ibus {

namespace {

class MockIBusPanelCandidateWindowHandler
    : public IBusPanelCandidateWindowHandlerInterface {
 public:
  MockIBusPanelCandidateWindowHandler() {}
  MOCK_METHOD2(UpdateLookupTable, void(const ibus::IBusLookupTable& table,
                                       bool visible));
  MOCK_METHOD0(HideLookupTable, void());
  MOCK_METHOD2(UpdateAuxiliaryText, void(const std::string& text,
                                         bool visible));
  MOCK_METHOD0(HideAuxiliaryText, void());
  MOCK_METHOD3(UpdatePreeditText, void(const std::string& text,
                                       uint32 cursor_pos,
                                       bool visible) );
  MOCK_METHOD0(HidePreeditText, void());
  MOCK_METHOD2(SetCursorLocation, void(const ibus::Rect& cursor_location,
                                       const ibus::Rect& composition_head));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIBusPanelCandidateWindowHandler);
};

class MockIBusPanelPropertyHandler : public IBusPanelPropertyHandlerInterface {
 public:
  MockIBusPanelPropertyHandler() {}
  MOCK_METHOD1(RegisterProperties,
               void(const ibus::IBusPropertyList& properties));
  MOCK_METHOD1(UpdateProperty, void(const ibus::IBusProperty& property));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIBusPanelPropertyHandler);
};

class MockResponseSender {
 public:
  MOCK_METHOD1(Run, void(dbus::Response* reponse));
};

// This class is used to verify that a method call response is empty. This class
// verifies the response has correct message serial number and has no entry in
// response.
class EmptyResponseVerifier {
 public:
  explicit EmptyResponseVerifier(uint32 expected_serial_number)
      : expected_serial_number_(expected_serial_number) {}

  // Verifies the given |response| has no argument.
  void Verify(dbus::Response* response) {
    scoped_ptr<dbus::Response> response_deleter(response);
    EXPECT_EQ(expected_serial_number_, response->GetReplySerial());
    dbus::MessageReader reader(response);
    EXPECT_FALSE(reader.HasMoreData());
  }

 private:
  const uint32 expected_serial_number_;

  DISALLOW_COPY_AND_ASSIGN(EmptyResponseVerifier);
};

// This class is used to verify the CandidateClicked method call arguments.
class CandidateClickedVerifier {
 public:
  CandidateClickedVerifier(uint32 expected_index,
                           ibus::IBusMouseButton expected_button,
                           uint32 expected_state)
      : expected_index_(expected_index),
        expected_button_(expected_button),
        expected_state_(expected_state) {}

  // Verifies the given |signal| is a valid message.
  void Verify(dbus::Signal* signal) {
    uint32 index = 0;
    uint32 button = 0;
    uint32 state = 0;

    // Read signal arguments.
    dbus::MessageReader reader(signal);
    EXPECT_TRUE(reader.PopUint32(&index));
    EXPECT_TRUE(reader.PopUint32(&button));
    EXPECT_TRUE(reader.PopUint32(&state));
    EXPECT_FALSE(reader.HasMoreData());

    // Check arguments.
    EXPECT_EQ(expected_index_, index);
    EXPECT_EQ(expected_button_, static_cast<ibus::IBusMouseButton>(button));
    EXPECT_EQ(expected_state_, state);
  }

 private:
  uint32 expected_index_;
  ibus::IBusMouseButton expected_button_;
  uint32 expected_state_;

  DISALLOW_COPY_AND_ASSIGN(CandidateClickedVerifier);
};

// This class is used to verify that a method call has empty argument.
class NullArgumentVerifier {
 public:
  explicit NullArgumentVerifier(const std::string& expected_signal_name)
      : expected_signal_name_(expected_signal_name) {}

  // Verifies the given |signal| is a valid message.
  void Verify(dbus::Signal* signal) {
    EXPECT_EQ(expected_signal_name_, signal->GetMember());
    dbus::MessageReader reader(signal);
    EXPECT_FALSE(reader.HasMoreData());
  }

 private:
  std::string expected_signal_name_;
  DISALLOW_COPY_AND_ASSIGN(NullArgumentVerifier);
};

class UpdateLookupTableVerifier {
 public:
  UpdateLookupTableVerifier(const ibus::IBusLookupTable& table, bool visible)
      : table_(table),
        visible_(visible) {}

  void Verify(const ibus::IBusLookupTable& table, bool visible) {
    EXPECT_EQ(table_.page_size(), table.page_size());
    EXPECT_EQ(table_.cursor_position(), table.cursor_position());
    EXPECT_EQ(visible_, visible);
  }

 private:
  const ibus::IBusLookupTable& table_;
  const bool visible_;

  DISALLOW_COPY_AND_ASSIGN(UpdateLookupTableVerifier);
};

// This class is used to verify that a method call which has a PropertyList
// object. This class verifies a method call has correct arguments based on
// checking given |keys|.
class PropertyListVerifier {
 public:
  explicit PropertyListVerifier(const std::vector<std::string>& expected_keys)
      : expected_keys_(expected_keys) {
  }

  // Verifies the given |resposne| has IBusPropertyList.
  void Verify(const ibus::IBusPropertyList& properties) {
    ASSERT_EQ(expected_keys_.size(), properties.size());
    for (size_t i = 0; i < properties.size(); ++i) {
      EXPECT_EQ(expected_keys_[i], properties[i]->key());
    }
  }

 private:
  const std::vector<std::string> expected_keys_;
};

// This class is used to verify that a method call which has a Property object.
// This class verifies a method call has correct argument based on |key|.
class PropertyVerifier {
 public:
  explicit PropertyVerifier(const std::string& key) : key_(key) {}

  // Verifies the given |resposne| has IBusPropertyList.
  void Verify(const ibus::IBusProperty& property) {
    EXPECT_EQ(key_, property.key());
  }

 private:
  const std::string key_;
};

}  // namespace

class IBusPanelServiceTest : public testing::Test {
 public:
  IBusPanelServiceTest() {}

  virtual void SetUp() OVERRIDE {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create a mock exported object.
    mock_exported_object_ = new dbus::MockExportedObject(
        mock_bus_.get(),
        dbus::ObjectPath(ibus::panel::kServicePath));

    EXPECT_CALL(*mock_bus_.get(),
                GetExportedObject(dbus::ObjectPath(
                    ibus::panel::kServicePath)))
        .WillOnce(Return(mock_exported_object_.get()));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kUpdateLookupTableMethod, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusPanelServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kHideLookupTableMethod, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusPanelServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kUpdateAuxiliaryTextMethod, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusPanelServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kHideAuxiliaryTextMethod, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusPanelServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kUpdatePreeditTextMethod, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusPanelServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kHidePreeditTextMethod, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusPanelServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kRegisterPropertiesMethod, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusPanelServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kUpdatePropertyMethod, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusPanelServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kFocusInMethod, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusPanelServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kFocusOutMethod, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusPanelServiceTest::OnMethodExported));

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::panel::kServiceInterface,
        ibus::panel::kStateChangedMethod, _, _))
        .WillRepeatedly(
            Invoke(this, &IBusPanelServiceTest::OnMethodExported));

    // Suppress uninteresting mock function call warning.
    EXPECT_CALL(*mock_bus_.get(),
                AssertOnOriginThread())
        .WillRepeatedly(Return());

    stub_input_context_client_.reset(IBusInputContextClient::Create(
        STUB_DBUS_CLIENT_IMPLEMENTATION));

    // Create a service
    service_.reset(IBusPanelService::Create(
        REAL_DBUS_CLIENT_IMPLEMENTATION,
        mock_bus_.get(),
        stub_input_context_client_.get()));

    // Set panel handler.
    candidate_window_handler_.reset(new MockIBusPanelCandidateWindowHandler());
    service_->SetUpCandidateWindowHandler(candidate_window_handler_.get());
    property_handler_.reset(new MockIBusPanelPropertyHandler());
    service_->SetUpPropertyHandler(property_handler_.get());
  }

 protected:
  // The service to be tested.
  scoped_ptr<IBusPanelService> service_;
  // The mock candidate window panel handler. Do not free, this is owned by
  // IBusPanelService.
  scoped_ptr<MockIBusPanelCandidateWindowHandler> candidate_window_handler_;
  // The mock property handler. Do not free, this is owned by IBusPanelService.
  scoped_ptr<MockIBusPanelPropertyHandler> property_handler_;
  // The stub input context client.
  scoped_ptr<IBusInputContextClient> stub_input_context_client_;
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

TEST_F(IBusPanelServiceTest, HideLookupTableTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  EXPECT_CALL(*candidate_window_handler_, HideLookupTable());
  MockResponseSender response_sender;
  EmptyResponseVerifier response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, Run(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseVerifier::Verify));

  // Create method call;
  dbus::MethodCall method_call(ibus::panel::kServiceInterface,
                               ibus::panel::kHideLookupTableMethod);
  method_call.SetSerial(kSerialNo);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::panel::kHideLookupTableMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::panel::kHideLookupTableMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusPanelServiceTest, HideAuxiliaryTextTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  EXPECT_CALL(*candidate_window_handler_, HideAuxiliaryText());
  MockResponseSender response_sender;
  EmptyResponseVerifier response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, Run(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseVerifier::Verify));

  // Create method call;
  dbus::MethodCall method_call(ibus::panel::kServiceInterface,
                               ibus::panel::kHideAuxiliaryTextMethod);
  method_call.SetSerial(kSerialNo);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::panel::kHideAuxiliaryTextMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::panel::kHideAuxiliaryTextMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusPanelServiceTest, HidePreeditTextTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  EXPECT_CALL(*candidate_window_handler_, HidePreeditText());
  MockResponseSender response_sender;
  EmptyResponseVerifier response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, Run(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseVerifier::Verify));

  // Create method call;
  dbus::MethodCall method_call(ibus::panel::kServiceInterface,
                               ibus::panel::kHidePreeditTextMethod);
  method_call.SetSerial(kSerialNo);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::panel::kHidePreeditTextMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::panel::kHidePreeditTextMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusPanelServiceTest, UpdateLookupTableTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  ibus::IBusLookupTable table;
  table.set_page_size(3);
  table.set_cursor_position(4);
  const bool kVisible = false;


  UpdateLookupTableVerifier evaluator(table, kVisible);
  EXPECT_CALL(*candidate_window_handler_, UpdateLookupTable(_, _))
      .WillOnce(Invoke(&evaluator,
                       &UpdateLookupTableVerifier::Verify));
  MockResponseSender response_sender;
  EmptyResponseVerifier response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, Run(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseVerifier::Verify));

  // Create method call;
  dbus::MethodCall method_call(ibus::panel::kServiceInterface,
                               ibus::panel::kUpdateLookupTableMethod);
  method_call.SetSerial(kSerialNo);
  dbus::MessageWriter writer(&method_call);
  AppendIBusLookupTable(table, &writer);
  writer.AppendBool(kVisible);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::panel::kUpdateLookupTableMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::panel::kUpdateLookupTableMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusPanelServiceTest, UpdateAuxiliaryTextTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  const std::string text = "Sample text";
  const bool kVisible = false;

  EXPECT_CALL(*candidate_window_handler_, UpdateAuxiliaryText(text, kVisible));
  MockResponseSender response_sender;
  EmptyResponseVerifier response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, Run(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseVerifier::Verify));

  // Create method call;
  dbus::MethodCall method_call(ibus::panel::kServiceInterface,
                               ibus::panel::kUpdateAuxiliaryTextMethod);
  method_call.SetSerial(kSerialNo);
  dbus::MessageWriter writer(&method_call);
  ibus::AppendStringAsIBusText(text, &writer);
  writer.AppendBool(kVisible);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::panel::kUpdateAuxiliaryTextMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::panel::kUpdateAuxiliaryTextMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusPanelServiceTest, UpdatePreeditTextTest) {
  // Set expectations.
  const uint32 kSerialNo = 1;
  const std::string text = "Sample text";
  const uint32 kCursorPos = 4;
  const bool kVisible = false;

  EXPECT_CALL(*candidate_window_handler_,
              UpdatePreeditText(text, kCursorPos, kVisible));
  MockResponseSender response_sender;
  EmptyResponseVerifier response_expectation(kSerialNo);
  EXPECT_CALL(response_sender, Run(_))
      .WillOnce(Invoke(&response_expectation,
                       &EmptyResponseVerifier::Verify));

  // Create method call;
  dbus::MethodCall method_call(ibus::panel::kServiceInterface,
                               ibus::panel::kUpdatePreeditTextMethod);
  method_call.SetSerial(kSerialNo);
  dbus::MessageWriter writer(&method_call);
  ibus::AppendStringAsIBusText(text, &writer);
  writer.AppendUint32(kCursorPos);
  writer.AppendBool(kVisible);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::panel::kUpdatePreeditTextMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::panel::kUpdatePreeditTextMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

TEST_F(IBusPanelServiceTest, CursorUpTest) {
  // Set expectations.
  NullArgumentVerifier evaluator(ibus::panel::kCursorUpSignal);
  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .WillOnce(Invoke(&evaluator, &NullArgumentVerifier::Verify));

  // Emit signal.
  service_->CursorUp();
}

TEST_F(IBusPanelServiceTest, CursorDownTest) {
  // Set expectations.
  NullArgumentVerifier evaluator(ibus::panel::kCursorDownSignal);
  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .WillOnce(Invoke(&evaluator, &NullArgumentVerifier::Verify));

  // Emit signal.
  service_->CursorDown();
}

TEST_F(IBusPanelServiceTest, PageUpTest) {
  // Set expectations.
  NullArgumentVerifier evaluator(ibus::panel::kPageUpSignal);
  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .WillOnce(Invoke(&evaluator, &NullArgumentVerifier::Verify));

  // Emit signal.
  service_->PageUp();
}

TEST_F(IBusPanelServiceTest, PageDownTest) {
  // Set expectations.
  NullArgumentVerifier evaluator(ibus::panel::kPageDownSignal);
  EXPECT_CALL(*mock_exported_object_, SendSignal(_))
      .WillOnce(Invoke(&evaluator, &NullArgumentVerifier::Verify));

  // Emit signal.
  service_->PageDown();
}

TEST_F(IBusPanelServiceTest, RegisterPropertiesTest) {
  // Set expectations.
  std::vector<std::string> keys;
  keys.push_back("key1");
  keys.push_back("key2");
  keys.push_back("key3");
  ibus::IBusPropertyList properties;
  for (size_t i = 0; i < keys.size(); ++i) {
    ibus::IBusProperty* property = new ibus::IBusProperty;
    property->set_key(keys[i]);
    properties.push_back(property);
  }

  PropertyListVerifier response_expectation(keys);
  EXPECT_CALL(*property_handler_, RegisterProperties(_))
      .WillOnce(Invoke(&response_expectation,
                        &PropertyListVerifier::Verify));

  MockResponseSender response_sender;
  EXPECT_CALL(response_sender, Run(_));

  // Create method call;
  dbus::MethodCall method_call(ibus::panel::kServiceInterface,
                               ibus::panel::kRegisterPropertiesMethod);
  method_call.SetSerial(1UL);
  dbus::MessageWriter writer(&method_call);
  ibus::AppendIBusPropertyList(properties, &writer);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::panel::kRegisterPropertiesMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::panel::kRegisterPropertiesMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run, base::Unretained(&response_sender)));
}

TEST_F(IBusPanelServiceTest, UpdatePropertyTest) {
  // Set expectations.
  const char kKey[] = "key";
  ibus::IBusProperty property;
  property.set_key(kKey);

  PropertyVerifier response_expectation(kKey);
  EXPECT_CALL(*property_handler_, UpdateProperty(_))
      .WillOnce(Invoke(&response_expectation, &PropertyVerifier::Verify));

  MockResponseSender response_sender;
  EXPECT_CALL(response_sender, Run(_));

  // Create method call;
  dbus::MethodCall method_call(ibus::panel::kServiceInterface,
                               ibus::panel::kUpdatePropertyMethod);
  method_call.SetSerial(1UL);
  dbus::MessageWriter writer(&method_call);
  ibus::AppendIBusProperty(property, &writer);

  // Call exported function.
  EXPECT_NE(method_callback_map_.find(ibus::panel::kUpdatePropertyMethod),
            method_callback_map_.end());
  method_callback_map_[ibus::panel::kUpdatePropertyMethod].Run(
      &method_call,
      base::Bind(&MockResponseSender::Run,
                 base::Unretained(&response_sender)));
}

}  // namespace ibus
}  // namespace chromeos
