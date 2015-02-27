// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_usb.h"

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/midi/usb_midi_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

template<typename T, size_t N>
std::vector<T> ToVector(const T (&array)[N]) {
  return std::vector<T>(array, array + N);
}

class Logger {
 public:
  Logger() {}
  ~Logger() {}

  void AddLog(const std::string& message) { log_ += message; }
  std::string TakeLog() {
    std::string result;
    result.swap(log_);
    return result;
  }

 private:
  std::string log_;

  DISALLOW_COPY_AND_ASSIGN(Logger);
};

class FakeUsbMidiDevice : public UsbMidiDevice {
 public:
  explicit FakeUsbMidiDevice(Logger* logger) : logger_(logger) {}
  ~FakeUsbMidiDevice() override {}

  std::vector<uint8> GetDescriptor() override {
    logger_->AddLog("UsbMidiDevice::GetDescriptor\n");
    return descriptor_;
  }

  void Send(int endpoint_number, const std::vector<uint8>& data) override {
    logger_->AddLog("UsbMidiDevice::Send ");
    logger_->AddLog(base::StringPrintf("endpoint = %d data =",
                                       endpoint_number));
    for (size_t i = 0; i < data.size(); ++i)
      logger_->AddLog(base::StringPrintf(" 0x%02x", data[i]));
    logger_->AddLog("\n");
  }

  void SetDescriptor(const std::vector<uint8> descriptor) {
    descriptor_ = descriptor;
  }

 private:
  std::vector<uint8> descriptor_;
  Logger* logger_;

  DISALLOW_COPY_AND_ASSIGN(FakeUsbMidiDevice);
};

class FakeMidiManagerClient : public MidiManagerClient {
 public:
  explicit FakeMidiManagerClient(Logger* logger)
      : complete_start_session_(false),
        result_(MIDI_NOT_SUPPORTED),
        logger_(logger) {}
  ~FakeMidiManagerClient() override {}

  void AddInputPort(const MidiPortInfo& info) override {
    input_ports_.push_back(info);
  }

  void AddOutputPort(const MidiPortInfo& info) override {
    output_ports_.push_back(info);
  }

  void SetInputPortState(uint32 port_index, MidiPortState state) override {}

  void SetOutputPortState(uint32 port_index, MidiPortState state) override {}

  void CompleteStartSession(MidiResult result) override {
    complete_start_session_ = true;
    result_ = result;
  }

  void ReceiveMidiData(uint32 port_index,
                       const uint8* data,
                       size_t size,
                       double timestamp) override {
    logger_->AddLog("MidiManagerClient::ReceiveMidiData ");
    logger_->AddLog(base::StringPrintf("port_index = %d data =", port_index));
    for (size_t i = 0; i < size; ++i)
      logger_->AddLog(base::StringPrintf(" 0x%02x", data[i]));
    logger_->AddLog("\n");
  }

  void AccumulateMidiBytesSent(size_t size) override {
    logger_->AddLog("MidiManagerClient::AccumulateMidiBytesSent ");
    // Windows has no "%zu".
    logger_->AddLog(base::StringPrintf("size = %u\n",
                                       static_cast<unsigned>(size)));
  }

  bool complete_start_session_;
  MidiResult result_;
  MidiPortInfoList input_ports_;
  MidiPortInfoList output_ports_;

 private:
  Logger* logger_;

  DISALLOW_COPY_AND_ASSIGN(FakeMidiManagerClient);
};

class TestUsbMidiDeviceFactory : public UsbMidiDevice::Factory {
 public:
  TestUsbMidiDeviceFactory() {}
  ~TestUsbMidiDeviceFactory() override {}
  void EnumerateDevices(UsbMidiDeviceDelegate* device,
                        Callback callback) override {
    callback_ = callback;
  }

  Callback callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestUsbMidiDeviceFactory);
};

class MidiManagerUsbForTesting : public MidiManagerUsb {
 public:
  explicit MidiManagerUsbForTesting(
      scoped_ptr<UsbMidiDevice::Factory> device_factory)
      : MidiManagerUsb(device_factory.Pass()) {}
  ~MidiManagerUsbForTesting() override {}

  void CallCompleteInitialization(MidiResult result) {
    CompleteInitialization(result);
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MidiManagerUsbForTesting);
};

class MidiManagerUsbTest : public ::testing::Test {
 public:
  MidiManagerUsbTest() : message_loop_(new base::MessageLoop) {
    scoped_ptr<TestUsbMidiDeviceFactory> factory(new TestUsbMidiDeviceFactory);
    factory_ = factory.get();
    manager_.reset(new MidiManagerUsbForTesting(factory.Pass()));
  }
  ~MidiManagerUsbTest() override {
    std::string leftover_logs = logger_.TakeLog();
    if (!leftover_logs.empty()) {
      ADD_FAILURE() << "Log should be empty: " << leftover_logs;
    }
  }

 protected:
  void Initialize() {
    client_.reset(new FakeMidiManagerClient(&logger_));
    manager_->StartSession(client_.get());
  }

  void Finalize() {
    manager_->EndSession(client_.get());
  }

  bool IsInitializationCallbackInvoked() {
    return client_->complete_start_session_;
  }

  MidiResult GetInitializationResult() {
    return client_->result_;
  }

  void RunCallbackUntilCallbackInvoked(
      bool result, UsbMidiDevice::Devices* devices) {
    factory_->callback_.Run(result, devices);
    while (!client_->complete_start_session_) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  const MidiPortInfoList& input_ports() { return client_->input_ports_; }
  const MidiPortInfoList& output_ports() { return client_->output_ports_; }

  scoped_ptr<MidiManagerUsbForTesting> manager_;
  scoped_ptr<FakeMidiManagerClient> client_;
  // Owned by manager_.
  TestUsbMidiDeviceFactory* factory_;
  Logger logger_;

 private:
  scoped_ptr<base::MessageLoop> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerUsbTest);
};


TEST_F(MidiManagerUsbTest, Initialize) {
  scoped_ptr<FakeUsbMidiDevice> device(new FakeUsbMidiDevice(&logger_));
  uint8 descriptor[] = {
    0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x86, 0x1a,
    0x2d, 0x75, 0x54, 0x02, 0x00, 0x02, 0x00, 0x01, 0x09, 0x02,
    0x75, 0x00, 0x02, 0x01, 0x00, 0x80, 0x30, 0x09, 0x04, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x09, 0x24, 0x01, 0x00,
    0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01, 0x00, 0x02,
    0x01, 0x03, 0x00, 0x00, 0x07, 0x24, 0x01, 0x00, 0x01, 0x51,
    0x00, 0x06, 0x24, 0x02, 0x01, 0x02, 0x00, 0x06, 0x24, 0x02,
    0x01, 0x03, 0x00, 0x06, 0x24, 0x02, 0x02, 0x06, 0x00, 0x09,
    0x24, 0x03, 0x01, 0x07, 0x01, 0x06, 0x01, 0x00, 0x09, 0x24,
    0x03, 0x02, 0x04, 0x01, 0x02, 0x01, 0x00, 0x09, 0x24, 0x03,
    0x02, 0x05, 0x01, 0x03, 0x01, 0x00, 0x09, 0x05, 0x02, 0x02,
    0x20, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x02,
    0x03, 0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x25, 0x01, 0x01, 0x07,
  };
  device->SetDescriptor(ToVector(descriptor));

  Initialize();
  ScopedVector<UsbMidiDevice> devices;
  devices.push_back(device.release());
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(MIDI_OK, GetInitializationResult());

  ASSERT_EQ(1u, input_ports().size());
  ASSERT_EQ(2u, output_ports().size());
  ASSERT_TRUE(manager_->input_stream());
  std::vector<UsbMidiJack> jacks = manager_->input_stream()->jacks();
  ASSERT_EQ(2u, manager_->output_streams().size());
  EXPECT_EQ(2u, manager_->output_streams()[0]->jack().jack_id);
  EXPECT_EQ(3u, manager_->output_streams()[1]->jack().jack_id);
  ASSERT_EQ(1u, jacks.size());
  EXPECT_EQ(2, jacks[0].endpoint_number());

  EXPECT_EQ("UsbMidiDevice::GetDescriptor\n", logger_.TakeLog());
}

TEST_F(MidiManagerUsbTest, InitializeMultipleDevices) {
  scoped_ptr<FakeUsbMidiDevice> device1(new FakeUsbMidiDevice(&logger_));
  scoped_ptr<FakeUsbMidiDevice> device2(new FakeUsbMidiDevice(&logger_));
  uint8 descriptor[] = {
      0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x86, 0x1a, 0x2d, 0x75,
      0x54, 0x02, 0x00, 0x02, 0x00, 0x01, 0x09, 0x02, 0x75, 0x00, 0x02, 0x01,
      0x00, 0x80, 0x30, 0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
      0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01,
      0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x07, 0x24, 0x01, 0x00, 0x01, 0x51,
      0x00, 0x06, 0x24, 0x02, 0x01, 0x02, 0x00, 0x06, 0x24, 0x02, 0x01, 0x03,
      0x00, 0x06, 0x24, 0x02, 0x02, 0x06, 0x00, 0x09, 0x24, 0x03, 0x01, 0x07,
      0x01, 0x06, 0x01, 0x00, 0x09, 0x24, 0x03, 0x02, 0x04, 0x01, 0x02, 0x01,
      0x00, 0x09, 0x24, 0x03, 0x02, 0x05, 0x01, 0x03, 0x01, 0x00, 0x09, 0x05,
      0x02, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x02,
      0x03, 0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x05, 0x25,
      0x01, 0x01, 0x07,
  };
  device1->SetDescriptor(ToVector(descriptor));
  device2->SetDescriptor(ToVector(descriptor));

  Initialize();
  ScopedVector<UsbMidiDevice> devices;
  devices.push_back(device1.release());
  devices.push_back(device2.release());
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(MIDI_OK, GetInitializationResult());

  ASSERT_EQ(2u, input_ports().size());
  ASSERT_EQ(4u, output_ports().size());
  ASSERT_TRUE(manager_->input_stream());
  std::vector<UsbMidiJack> jacks = manager_->input_stream()->jacks();
  ASSERT_EQ(4u, manager_->output_streams().size());
  EXPECT_EQ(2u, manager_->output_streams()[0]->jack().jack_id);
  EXPECT_EQ(3u, manager_->output_streams()[1]->jack().jack_id);
  ASSERT_EQ(2u, jacks.size());
  EXPECT_EQ(2, jacks[0].endpoint_number());

  EXPECT_EQ(
      "UsbMidiDevice::GetDescriptor\n"
      "UsbMidiDevice::GetDescriptor\n",
      logger_.TakeLog());
}

TEST_F(MidiManagerUsbTest, InitializeFail) {
  Initialize();

  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(false, NULL);
  EXPECT_EQ(MIDI_INITIALIZATION_ERROR, GetInitializationResult());
}

TEST_F(MidiManagerUsbTest, InitializeFailBecauseOfInvalidDescriptor) {
  scoped_ptr<FakeUsbMidiDevice> device(new FakeUsbMidiDevice(&logger_));
  uint8 descriptor[] = {0x04};
  device->SetDescriptor(ToVector(descriptor));

  Initialize();
  ScopedVector<UsbMidiDevice> devices;
  devices.push_back(device.release());
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(MIDI_INITIALIZATION_ERROR, GetInitializationResult());
  EXPECT_EQ("UsbMidiDevice::GetDescriptor\n", logger_.TakeLog());
}

TEST_F(MidiManagerUsbTest, Send) {
  scoped_ptr<FakeUsbMidiDevice> device(new FakeUsbMidiDevice(&logger_));
  FakeMidiManagerClient client(&logger_);
  uint8 descriptor[] = {
    0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x86, 0x1a,
    0x2d, 0x75, 0x54, 0x02, 0x00, 0x02, 0x00, 0x01, 0x09, 0x02,
    0x75, 0x00, 0x02, 0x01, 0x00, 0x80, 0x30, 0x09, 0x04, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x09, 0x24, 0x01, 0x00,
    0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01, 0x00, 0x02,
    0x01, 0x03, 0x00, 0x00, 0x07, 0x24, 0x01, 0x00, 0x01, 0x51,
    0x00, 0x06, 0x24, 0x02, 0x01, 0x02, 0x00, 0x06, 0x24, 0x02,
    0x01, 0x03, 0x00, 0x06, 0x24, 0x02, 0x02, 0x06, 0x00, 0x09,
    0x24, 0x03, 0x01, 0x07, 0x01, 0x06, 0x01, 0x00, 0x09, 0x24,
    0x03, 0x02, 0x04, 0x01, 0x02, 0x01, 0x00, 0x09, 0x24, 0x03,
    0x02, 0x05, 0x01, 0x03, 0x01, 0x00, 0x09, 0x05, 0x02, 0x02,
    0x20, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x02,
    0x03, 0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x25, 0x01, 0x01, 0x07,
  };

  device->SetDescriptor(ToVector(descriptor));
  uint8 data[] = {
    0x90, 0x45, 0x7f,
    0xf0, 0x00, 0x01, 0xf7,
  };

  Initialize();
  ScopedVector<UsbMidiDevice> devices;
  devices.push_back(device.release());
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(MIDI_OK, GetInitializationResult());
  ASSERT_EQ(2u, manager_->output_streams().size());

  manager_->DispatchSendMidiData(&client, 1, ToVector(data), 0);
  EXPECT_EQ("UsbMidiDevice::GetDescriptor\n"
            "UsbMidiDevice::Send endpoint = 2 data = "
            "0x19 0x90 0x45 0x7f "
            "0x14 0xf0 0x00 0x01 "
            "0x15 0xf7 0x00 0x00\n"
            "MidiManagerClient::AccumulateMidiBytesSent size = 7\n",
            logger_.TakeLog());
}

TEST_F(MidiManagerUsbTest, SendFromCompromizedRenderer) {
  scoped_ptr<FakeUsbMidiDevice> device(new FakeUsbMidiDevice(&logger_));
  FakeMidiManagerClient client(&logger_);
  uint8 descriptor[] = {
    0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x86, 0x1a,
    0x2d, 0x75, 0x54, 0x02, 0x00, 0x02, 0x00, 0x01, 0x09, 0x02,
    0x75, 0x00, 0x02, 0x01, 0x00, 0x80, 0x30, 0x09, 0x04, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x09, 0x24, 0x01, 0x00,
    0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01, 0x00, 0x02,
    0x01, 0x03, 0x00, 0x00, 0x07, 0x24, 0x01, 0x00, 0x01, 0x51,
    0x00, 0x06, 0x24, 0x02, 0x01, 0x02, 0x00, 0x06, 0x24, 0x02,
    0x01, 0x03, 0x00, 0x06, 0x24, 0x02, 0x02, 0x06, 0x00, 0x09,
    0x24, 0x03, 0x01, 0x07, 0x01, 0x06, 0x01, 0x00, 0x09, 0x24,
    0x03, 0x02, 0x04, 0x01, 0x02, 0x01, 0x00, 0x09, 0x24, 0x03,
    0x02, 0x05, 0x01, 0x03, 0x01, 0x00, 0x09, 0x05, 0x02, 0x02,
    0x20, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x02,
    0x03, 0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x25, 0x01, 0x01, 0x07,
  };

  device->SetDescriptor(ToVector(descriptor));
  uint8 data[] = {
    0x90, 0x45, 0x7f,
    0xf0, 0x00, 0x01, 0xf7,
  };

  Initialize();
  ScopedVector<UsbMidiDevice> devices;
  devices.push_back(device.release());
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(MIDI_OK, GetInitializationResult());
  ASSERT_EQ(2u, manager_->output_streams().size());
  EXPECT_EQ("UsbMidiDevice::GetDescriptor\n", logger_.TakeLog());

  // The specified port index is invalid. The manager must ignore the request.
  manager_->DispatchSendMidiData(&client, 99, ToVector(data), 0);
  EXPECT_EQ("", logger_.TakeLog());

  // The specified port index is invalid. The manager must ignore the request.
  manager_->DispatchSendMidiData(&client, 2, ToVector(data), 0);
  EXPECT_EQ("", logger_.TakeLog());
}

TEST_F(MidiManagerUsbTest, Receive) {
  scoped_ptr<FakeUsbMidiDevice> device(new FakeUsbMidiDevice(&logger_));
  uint8 descriptor[] = {
    0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x86, 0x1a,
    0x2d, 0x75, 0x54, 0x02, 0x00, 0x02, 0x00, 0x01, 0x09, 0x02,
    0x75, 0x00, 0x02, 0x01, 0x00, 0x80, 0x30, 0x09, 0x04, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x09, 0x24, 0x01, 0x00,
    0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01, 0x00, 0x02,
    0x01, 0x03, 0x00, 0x00, 0x07, 0x24, 0x01, 0x00, 0x01, 0x51,
    0x00, 0x06, 0x24, 0x02, 0x01, 0x02, 0x00, 0x06, 0x24, 0x02,
    0x01, 0x03, 0x00, 0x06, 0x24, 0x02, 0x02, 0x06, 0x00, 0x09,
    0x24, 0x03, 0x01, 0x07, 0x01, 0x06, 0x01, 0x00, 0x09, 0x24,
    0x03, 0x02, 0x04, 0x01, 0x02, 0x01, 0x00, 0x09, 0x24, 0x03,
    0x02, 0x05, 0x01, 0x03, 0x01, 0x00, 0x09, 0x05, 0x02, 0x02,
    0x20, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x02,
    0x03, 0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x25, 0x01, 0x01, 0x07,
  };

  device->SetDescriptor(ToVector(descriptor));
  uint8 data[] = {
    0x09, 0x90, 0x45, 0x7f,
    0x04, 0xf0, 0x00, 0x01,
    0x49, 0x90, 0x88, 0x99,  // This data should be ignored (CN = 4).
    0x05, 0xf7, 0x00, 0x00,
  };

  Initialize();
  ScopedVector<UsbMidiDevice> devices;
  UsbMidiDevice* device_raw = device.get();
  devices.push_back(device.release());
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(MIDI_OK, GetInitializationResult());

  manager_->ReceiveUsbMidiData(device_raw, 2, data, arraysize(data),
                               base::TimeTicks());
  Finalize();

  EXPECT_EQ("UsbMidiDevice::GetDescriptor\n"
            "MidiManagerClient::ReceiveMidiData port_index = 0 "
            "data = 0x90 0x45 0x7f\n"
            "MidiManagerClient::ReceiveMidiData port_index = 0 "
            "data = 0xf0 0x00 0x01\n"
            "MidiManagerClient::ReceiveMidiData port_index = 0 data = 0xf7\n",
            logger_.TakeLog());
}

TEST_F(MidiManagerUsbTest, AttachDevice) {
  uint8 descriptor[] = {
    0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x86, 0x1a,
    0x2d, 0x75, 0x54, 0x02, 0x00, 0x02, 0x00, 0x01, 0x09, 0x02,
    0x75, 0x00, 0x02, 0x01, 0x00, 0x80, 0x30, 0x09, 0x04, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x09, 0x24, 0x01, 0x00,
    0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01, 0x00, 0x02,
    0x01, 0x03, 0x00, 0x00, 0x07, 0x24, 0x01, 0x00, 0x01, 0x51,
    0x00, 0x06, 0x24, 0x02, 0x01, 0x02, 0x00, 0x06, 0x24, 0x02,
    0x01, 0x03, 0x00, 0x06, 0x24, 0x02, 0x02, 0x06, 0x00, 0x09,
    0x24, 0x03, 0x01, 0x07, 0x01, 0x06, 0x01, 0x00, 0x09, 0x24,
    0x03, 0x02, 0x04, 0x01, 0x02, 0x01, 0x00, 0x09, 0x24, 0x03,
    0x02, 0x05, 0x01, 0x03, 0x01, 0x00, 0x09, 0x05, 0x02, 0x02,
    0x20, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x02,
    0x03, 0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x25, 0x01, 0x01, 0x07,
  };

  Initialize();
  ScopedVector<UsbMidiDevice> devices;
  EXPECT_FALSE(IsInitializationCallbackInvoked());
  RunCallbackUntilCallbackInvoked(true, &devices);
  EXPECT_EQ(MIDI_OK, GetInitializationResult());

  ASSERT_EQ(0u, input_ports().size());
  ASSERT_EQ(0u, output_ports().size());
  ASSERT_TRUE(manager_->input_stream());
  std::vector<UsbMidiJack> jacks = manager_->input_stream()->jacks();
  ASSERT_EQ(0u, manager_->output_streams().size());
  ASSERT_EQ(0u, jacks.size());
  EXPECT_EQ("", logger_.TakeLog());

  scoped_ptr<FakeUsbMidiDevice> new_device(new FakeUsbMidiDevice(&logger_));
  new_device->SetDescriptor(ToVector(descriptor));
  manager_->OnDeviceAttached(new_device.Pass());

  ASSERT_EQ(1u, input_ports().size());
  ASSERT_EQ(2u, output_ports().size());
  ASSERT_TRUE(manager_->input_stream());
  jacks = manager_->input_stream()->jacks();
  ASSERT_EQ(2u, manager_->output_streams().size());
  EXPECT_EQ(2u, manager_->output_streams()[0]->jack().jack_id);
  EXPECT_EQ(3u, manager_->output_streams()[1]->jack().jack_id);
  ASSERT_EQ(1u, jacks.size());
  EXPECT_EQ(2, jacks[0].endpoint_number());
  EXPECT_EQ("UsbMidiDevice::GetDescriptor\n", logger_.TakeLog());
}

}  // namespace

}  // namespace media
