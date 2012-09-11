// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/api/serial/serial_connection.h"
#include "chrome/browser/extensions/api/serial/serial_port_enumerator.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

const char kConnectionIdKey[] = "connectionId";
const char kPortsKey[] = "ports";
const char kDataKey[] = "data";
const char kBytesReadKey[] = "bytesRead";
const char kBytesToReadKey[] = "bytesToRead";
const char kBytesWrittenKey[] = "bytesWritten";
const char kBitrateKey[] = "bitrate";
const char kOptionsKey[] = "options";
const char kSuccessKey[] = "success";
const char kDtrKey[] = "dtr";
const char kRtsKey[] = "rts";
const char kDcdKey[] = "dcd";
const char kCtsKey[] = "cts";

const char kErrorGetControlSignalsFailed[] = "Failed to get control signals.";
const char kErrorSetControlSignalsFailed[] = "Failed to set control signals.";
const char kSerialReadInvalidBytesToRead[] = "Number of bytes to read must "
    "be a positive number less than 1,048,576.";

SerialAsyncApiFunction::SerialAsyncApiFunction()
    : manager_(NULL) {
}

SerialAsyncApiFunction::~SerialAsyncApiFunction() {
}

bool SerialAsyncApiFunction::PrePrepare() {
  manager_ = ExtensionSystem::Get(profile())->serial_connection_manager();
  return manager_ != NULL;
}

SerialConnection* SerialAsyncApiFunction::GetSerialConnection(
    int api_resource_id) {
  return manager_->Get(extension_->id(), api_resource_id);
}

void SerialAsyncApiFunction::RemoveSerialConnection(int api_resource_id) {
  manager_->Remove(extension_->id(), api_resource_id);
}

SerialGetPortsFunction::SerialGetPortsFunction() {}

bool SerialGetPortsFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);
  return true;
}

void SerialGetPortsFunction::Work() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  ListValue* ports = new ListValue();
  SerialPortEnumerator::StringSet port_names =
      SerialPortEnumerator::GenerateValidSerialPortNames();
  SerialPortEnumerator::StringSet::const_iterator i = port_names.begin();
  while (i != port_names.end()) {
    ports->Append(Value::CreateStringValue(*i++));
  }

  SetResult(ports);
}

bool SerialGetPortsFunction::Respond() {
  return true;
}

// It's a fool's errand to come up with a default bitrate, because we don't get
// to control both sides of the communication. Unless the other side has
// implemented auto-bitrate detection (rare), if we pick the wrong rate, then
// you're gonna have a bad time. Close doesn't count.
//
// But we'd like to pick something that has a chance of working, and 9600 is a
// good balance between popularity and speed. So 9600 it is.
SerialOpenFunction::SerialOpenFunction()
    : src_id_(-1),
      bitrate_(9600),
      event_notifier_(NULL) {
}

SerialOpenFunction::~SerialOpenFunction() {
}

bool SerialOpenFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);

  params_ = api::serial::Open::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  if (params_->options.get()) {
    scoped_ptr<DictionaryValue> options = params_->options->ToValue();
    if (options->HasKey(kBitrateKey))
      EXTENSION_FUNCTION_VALIDATE(options->GetInteger(kBitrateKey, &bitrate_));

    src_id_ = ExtractSrcId(options.get());
    event_notifier_ = CreateEventNotifier(src_id_);
  }

  return true;
}

void SerialOpenFunction::AsyncWorkStart() {
  Work();
}

void SerialOpenFunction::Work() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  const SerialPortEnumerator::StringSet name_set(
    SerialPortEnumerator::GenerateValidSerialPortNames());
  if (DoesPortExist(params_->port)) {
    SerialConnection* serial_connection = CreateSerialConnection(
      params_->port,
      bitrate_,
      extension_->id(),
      event_notifier_);
    CHECK(serial_connection);
    int id = manager_->Add(serial_connection);
    CHECK(id);

    bool open_result = serial_connection->Open();
    if (!open_result) {
      serial_connection->Close();
      RemoveSerialConnection(id);
      id = -1;
    }

    DictionaryValue* result = new DictionaryValue();
    result->SetInteger(kConnectionIdKey, id);
    SetResult(result);
    AsyncWorkCompleted();
  } else {
    DictionaryValue* result = new DictionaryValue();
    result->SetInteger(kConnectionIdKey, -1);
    SetResult(result);
    AsyncWorkCompleted();
  }
}

SerialConnection* SerialOpenFunction::CreateSerialConnection(
    const std::string& port,
    int bitrate,
    const std::string& owner_extension_id,
    ApiResourceEventNotifier* event_notifier) {
  return new SerialConnection(port, bitrate, owner_extension_id,
                              event_notifier);
}

bool SerialOpenFunction::DoesPortExist(const std::string& port) {
  const SerialPortEnumerator::StringSet name_set(
    SerialPortEnumerator::GenerateValidSerialPortNames());
  return SerialPortEnumerator::DoesPortExist(name_set, params_->port);
}

bool SerialOpenFunction::Respond() {
  return true;
}

SerialCloseFunction::SerialCloseFunction() {
}

SerialCloseFunction::~SerialCloseFunction() {
}

bool SerialCloseFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);

  params_ = api::serial::Close::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialCloseFunction::Work() {
  bool close_result = false;
  SerialConnection* serial_connection = GetSerialConnection(
      params_->connection_id);
  if (serial_connection) {
    serial_connection->Close();
    RemoveSerialConnection(params_->connection_id);
    close_result = true;
  }

  SetResult(Value::CreateBooleanValue(close_result));
}

bool SerialCloseFunction::Respond() {
  return true;
}

SerialReadFunction::SerialReadFunction() {
}

SerialReadFunction::~SerialReadFunction() {
}

bool SerialReadFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);

  params_ = api::serial::Read::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  if (params_->bytes_to_read <= 0 || params_->bytes_to_read >= 1024 * 1024) {
    error_ = kSerialReadInvalidBytesToRead;
    return false;
  }

  return true;
}

void SerialReadFunction::Work() {
  int bytes_read = -1;
  scoped_refptr<net::IOBufferWithSize> io_buffer(
      new net::IOBufferWithSize(params_->bytes_to_read));
  SerialConnection* serial_connection(GetSerialConnection(
      params_->connection_id));

  if (serial_connection)
    bytes_read = serial_connection->Read(io_buffer);

  DictionaryValue* result = new DictionaryValue();

  // The API is defined to require a 'data' value, so we will always
  // create a BinaryValue, even if it's zero-length.
  if (bytes_read < 0)
    bytes_read = 0;
  result->SetInteger(kBytesReadKey, bytes_read);
  result->Set(kDataKey, base::BinaryValue::CreateWithCopiedBuffer(
      io_buffer->data(), bytes_read));
  SetResult(result);
}

bool SerialReadFunction::Respond() {
  return true;
}

SerialWriteFunction::SerialWriteFunction()
    : io_buffer_(NULL), io_buffer_size_(0) {
}

SerialWriteFunction::~SerialWriteFunction() {
}

bool SerialWriteFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);

  params_ = api::serial::Write::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  io_buffer_size_ = params_->data.size();
  io_buffer_ = new net::WrappedIOBuffer(params_->data.data());

  return true;
}

void SerialWriteFunction::Work() {
  int bytes_written = -1;
  SerialConnection* serial_connection = GetSerialConnection(
      params_->connection_id);
  if (serial_connection)
    bytes_written = serial_connection->Write(io_buffer_, io_buffer_size_);
  else
    error_ = kSerialConnectionNotFoundError;

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kBytesWrittenKey, bytes_written);
  SetResult(result);
}

bool SerialWriteFunction::Respond() {
  return true;
}

SerialFlushFunction::SerialFlushFunction() {
}

SerialFlushFunction::~SerialFlushFunction() {
}

bool SerialFlushFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);

  params_ = api::serial::Flush::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SerialFlushFunction::Work() {
  bool flush_result = false;
  SerialConnection* serial_connection = GetSerialConnection(
      params_->connection_id);
  if (serial_connection) {
    serial_connection->Flush();
    flush_result = true;
  }

  SetResult(Value::CreateBooleanValue(flush_result));
}

bool SerialFlushFunction::Respond() {
  return true;
}

SerialGetControlSignalsFunction::SerialGetControlSignalsFunction()
    : api_response_(false) {
}

SerialGetControlSignalsFunction::~SerialGetControlSignalsFunction() {
}

bool SerialGetControlSignalsFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);

  params_ = api::serial::GetControlSignals::Params::Create(
      *args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialGetControlSignalsFunction::Work() {
  DictionaryValue *result = new DictionaryValue();
  SerialConnection* serial_connection = GetSerialConnection(
      params_->connection_id);
  if (serial_connection) {
    SerialConnection::ControlSignals control_signals = { 0 };
    if (serial_connection->GetControlSignals(control_signals)) {
      api_response_ = true;
      result->SetBoolean(kDcdKey, control_signals.dcd);
      result->SetBoolean(kCtsKey, control_signals.cts);
    } else {
      error_ = kErrorGetControlSignalsFailed;
    }
  } else {
    error_ = kSerialConnectionNotFoundError;
    result->SetBoolean(kSuccessKey, false);
  }

  SetResult(result);
}

bool SerialGetControlSignalsFunction::Respond() {
  return api_response_;
}

SerialSetControlSignalsFunction::SerialSetControlSignalsFunction() {
}

SerialSetControlSignalsFunction::~SerialSetControlSignalsFunction() {
}

bool SerialSetControlSignalsFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);

  params_ = api::serial::SetControlSignals::Params::Create(
      *args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialSetControlSignalsFunction::Work() {
  SerialConnection* serial_connection = GetSerialConnection(
      params_->connection_id);
  if (serial_connection) {
    SerialConnection::ControlSignals control_signals = { 0 };
    control_signals.should_set_dtr = params_->options.dtr.get() != NULL;
    if (control_signals.should_set_dtr)
      control_signals.dtr = *(params_->options.dtr);
    control_signals.should_set_rts = params_->options.rts.get() != NULL;
    if (control_signals.should_set_rts)
      control_signals.rts = *(params_->options.rts);
    if (serial_connection->SetControlSignals(control_signals)) {
      SetResult(Value::CreateBooleanValue(true));
    } else {
      error_ = kErrorSetControlSignalsFailed;
      SetResult(Value::CreateBooleanValue(false));
    }
  } else {
    error_ = kSerialConnectionNotFoundError;
    SetResult(Value::CreateBooleanValue(false));
  }
}

bool SerialSetControlSignalsFunction::Respond() {
  return true;
}

}  // namespace extensions
