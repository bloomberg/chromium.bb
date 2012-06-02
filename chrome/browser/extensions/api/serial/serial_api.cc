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

SerialOpenFunction::SerialOpenFunction()
    : src_id_(-1),
      event_notifier_(NULL) {}

bool SerialOpenFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);

  size_t argument_position = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(argument_position++, &port_));
  src_id_ = ExtractSrcId(argument_position);
  event_notifier_ = CreateEventNotifier(src_id_);

  return true;
}

void SerialOpenFunction::AsyncWorkStart() {
  Work();
}

void SerialOpenFunction::Work() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  const SerialPortEnumerator::StringSet name_set(
    SerialPortEnumerator::GenerateValidSerialPortNames());
  if (SerialPortEnumerator::DoesPortExist(name_set, port_)) {
    SerialConnection* serial_connection = new SerialConnection(
      port_,
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

bool SerialCloseFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);

  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &connection_id_));
  return true;
}

void SerialCloseFunction::Work() {
  bool close_result = false;
  SerialConnection* serial_connection =
      controller()->GetSerialConnection(connection_id_);
  if (serial_connection) {
    serial_connection->Close();
    controller()->RemoveSerialConnection(connection_id_);
    close_result = true;
  }

  result_.reset(Value::CreateBooleanValue(close_result));
}

bool SerialCloseFunction::Respond() {
  return true;
}

bool SerialReadFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);

  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &connection_id_));
  return true;
}

void SerialReadFunction::Work() {
  uint8 byte = '\0';
  int bytes_read = -1;
  SerialConnection* serial_connection =
      controller()->GetSerialConnection(connection_id_);
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
    : connection_id_(-1), io_buffer_(NULL), io_buffer_size_(0) {
}

SerialWriteFunction::~SerialWriteFunction() {}

bool SerialWriteFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);

  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &connection_id_));
  base::BinaryValue *data = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBinary(1, &data));

  io_buffer_size_ = data->GetSize();
  io_buffer_ = new net::WrappedIOBuffer(data->GetBuffer());

  return true;
}

void SerialWriteFunction::Work() {
  int bytes_written = -1;
  SerialConnection* serial_connection =
      controller()->GetSerialConnection(connection_id_);
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

bool SerialFlushFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);

  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &connection_id_));
  return true;
}

void SerialFlushFunction::Work() {
  bool flush_result = false;
  SerialConnection* serial_connection =
      controller()->GetSerialConnection(connection_id_);
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
