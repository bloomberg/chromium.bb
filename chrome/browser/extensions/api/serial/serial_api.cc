// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_api.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/extensions/api/api_resource_controller.h"
#include "chrome/browser/extensions/api/serial/serial_connection.h"

namespace extensions {

const char kConnectionIdKey[] = "connectionId";
const char kDataKey[] = "data";
const char kBytesReadKey[] = "bytesRead";
const char kBytesWrittenKey[] = "bytesWritten";

SerialOpenFunction::SerialOpenFunction()
    : src_id_(-1) {
}

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

bool SerialWriteFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &connection_id_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &data_));
  return true;
}

void SerialWriteFunction::Work() {
  int bytes_written = -1;
  SerialConnection* serial_connection =
      controller()->GetSerialConnection(connection_id_);
  if (serial_connection)
    bytes_written = serial_connection->Write(data_);
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kBytesWrittenKey, bytes_written);
  result_.reset(result);
}

bool SerialWriteFunction::Respond() {
  return true;
}

}  // namespace extensions
