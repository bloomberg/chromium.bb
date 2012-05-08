// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_api.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/extensions/api/api_resource_controller.h"
#include "chrome/browser/extensions/api/serial/serial_connection.h"
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
  work_thread_id_ = BrowserThread::FILE;
  return true;
}

void SerialGetPortsFunction::Work() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  ListValue* ports = new ListValue();
  SerialConnection::StringSet port_names =
      SerialConnection::GenerateValidSerialPortNames();
  SerialConnection::StringSet::const_iterator i = port_names.begin();
  while (i != port_names.end()) {
    ports->Append(Value::CreateStringValue(*i++));
  }

  result_.reset(ports);
}

bool SerialGetPortsFunction::Respond() {
  return true;
}

SerialOpenFunction::SerialOpenFunction() : src_id_(-1) {}

bool SerialOpenFunction::Prepare() {
  size_t argument_position = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(argument_position++, &port_));
  src_id_ = ExtractSrcId(argument_position);
  return true;
}

void SerialOpenFunction::Work() {
  APIResourceEventNotifier* event_notifier = CreateEventNotifier(src_id_);
  SerialConnection* serial_connection = new SerialConnection(port_,
                                                             event_notifier);
  CHECK(serial_connection);
  int id = controller()->AddAPIResource(serial_connection);
  CHECK(id);

  bool open_result = serial_connection->Open();
  if (!open_result) {
    serial_connection->Close();
    controller()->RemoveAPIResource(id);
    id = -1;
  }

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kConnectionIdKey, id);
  result_.reset(result);
}

bool SerialOpenFunction::Respond() {
  return true;
}

bool SerialCloseFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &connection_id_));
  return true;
}

void SerialCloseFunction::Work() {
  bool close_result = false;
  SerialConnection* serial_connection =
      controller()->GetSerialConnection(connection_id_);
  if (serial_connection) {
    serial_connection->Close();
    controller()->RemoveAPIResource(connection_id_);
    close_result = true;
  }

  result_.reset(Value::CreateBooleanValue(close_result));
}

bool SerialCloseFunction::Respond() {
  return true;
}

bool SerialReadFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &connection_id_));
  return true;
}

void SerialReadFunction::Work() {
  int bytes_read = -1;
  std::string data;
  SerialConnection* serial_connection =
      controller()->GetSerialConnection(connection_id_);
  if (serial_connection) {
    unsigned char byte = '\0';
    bytes_read = serial_connection->Read(&byte);
    if (bytes_read == 1)
      data = byte;
  }

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kBytesReadKey, bytes_read);
  result->SetString(kDataKey, data);
  result_.reset(result);
}

bool SerialReadFunction::Respond() {
  return true;
}

SerialWriteFunction::SerialWriteFunction()
    : connection_id_(-1), io_buffer_(NULL) {
}

SerialWriteFunction::~SerialWriteFunction() {}

bool SerialWriteFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &connection_id_));
  base::ListValue* data_list_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(1, &data_list_value));
  size_t size = data_list_value->GetSize();
  if (size != 0) {
    io_buffer_ = new net::IOBufferWithSize(size);
    uint8* data_buffer =
        reinterpret_cast<uint8*>(io_buffer_->data());
    for (size_t i = 0; i < size; ++i) {
      int int_value = -1;
      data_list_value->GetInteger(i, &int_value);
      DCHECK(int_value < 256);
      DCHECK(int_value >= 0);
      uint8 truncated_int = static_cast<uint8>(int_value);
      *data_buffer++ = truncated_int;
    }
  }
  return true;
}

void SerialWriteFunction::Work() {
  int bytes_written = -1;
  SerialConnection* serial_connection =
      controller()->GetSerialConnection(connection_id_);
  if (serial_connection)
    bytes_written = serial_connection->Write(io_buffer_, io_buffer_->size());
  else
    error_ = kSerialConnectionNotFoundError;

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kBytesWrittenKey, bytes_written);
  result_.reset(result);
}

bool SerialWriteFunction::Respond() {
  return true;
}

}  // namespace extensions
