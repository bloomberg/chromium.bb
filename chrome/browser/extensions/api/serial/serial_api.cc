// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/api_resource_controller.h"
#include "chrome/browser/extensions/api/serial/serial_connection.h"
#include "chrome/browser/extensions/api/serial/serial_port_enumerator.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

const char kConnectionIdKey[] = "connectionId";
const char kPortsKey[] = "ports";
const char kDataKey[] = "data";
const char kBytesReadKey[] = "bytesRead";
const char kBytesWrittenKey[] = "bytesWritten";
const char kBitrateKey[] = "bitrate";

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

  result_.reset(ports);
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

  params_ = api::experimental_serial::Open::Params::Create(*args_);
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
  if (SerialPortEnumerator::DoesPortExist(name_set, params_->port)) {
    SerialConnection* serial_connection = new SerialConnection(
      params_->port,
      bitrate_,
      event_notifier_);
    CHECK(serial_connection);
    int id = controller()->AddAPIResource(serial_connection);
    CHECK(id);

    bool open_result = serial_connection->Open();
    if (!open_result) {
      serial_connection->Close();
      controller()->RemoveSerialConnection(id);
      id = -1;
    }

    DictionaryValue* result = new DictionaryValue();
    result->SetInteger(kConnectionIdKey, id);
    result_.reset(result);
    AsyncWorkCompleted();
  } else {
    DictionaryValue* result = new DictionaryValue();
    result->SetInteger(kConnectionIdKey, -1);
    result_.reset(result);
    AsyncWorkCompleted();
  }
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

  params_ = api::experimental_serial::Close::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SerialCloseFunction::Work() {
  bool close_result = false;
  SerialConnection* serial_connection =
      controller()->GetSerialConnection(params_->connection_id);
  if (serial_connection) {
    serial_connection->Close();
    controller()->RemoveSerialConnection(params_->connection_id);
    close_result = true;
  }

  result_.reset(Value::CreateBooleanValue(close_result));
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

  params_ = api::experimental_serial::Read::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SerialReadFunction::Work() {
  uint8 byte = '\0';
  int bytes_read = -1;
  SerialConnection* serial_connection =
      controller()->GetSerialConnection(params_->connection_id);
  if (serial_connection)
    bytes_read = serial_connection->Read(&byte);

  DictionaryValue* result = new DictionaryValue();

  // The API is defined to require a 'data' value, so we will always
  // create a BinaryValue, even if it's zero-length.
  if (bytes_read < 0)
    bytes_read = 0;
  result->SetInteger(kBytesReadKey, bytes_read);
  result->Set(kDataKey, base::BinaryValue::CreateWithCopiedBuffer(
      reinterpret_cast<char*>(&byte), bytes_read));
  result_.reset(result);
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

  params_ = api::experimental_serial::Write::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  io_buffer_size_ = params_->data->GetSize();
  io_buffer_ = new net::WrappedIOBuffer(params_->data->GetBuffer());

  return true;
}

void SerialWriteFunction::Work() {
  int bytes_written = -1;
  SerialConnection* serial_connection =
      controller()->GetSerialConnection(params_->connection_id);
  if (serial_connection)
    bytes_written = serial_connection->Write(io_buffer_, io_buffer_size_);
  else
    error_ = kSerialConnectionNotFoundError;

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kBytesWrittenKey, bytes_written);
  result_.reset(result);
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

  params_ = api::experimental_serial::Flush::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SerialFlushFunction::Work() {
  bool flush_result = false;
  SerialConnection* serial_connection =
      controller()->GetSerialConnection(params_->connection_id);
  if (serial_connection) {
    serial_connection->Flush();
    flush_result = true;
  }

  result_.reset(Value::CreateBooleanValue(flush_result));
}

bool SerialFlushFunction::Respond() {
  return true;
}

}  // namespace extensions
