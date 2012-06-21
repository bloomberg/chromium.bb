// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_object.h"

#include "base/logging.h"
#include "chromeos/dbus/ibus/ibus_property.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "dbus/message.h"

namespace chromeos {
// TODO(nona): Remove ibus namespace after complete libibus removal.
namespace ibus {

///////////////////////////////////////////////////////////////////////////////
// IBusObjectReader
IBusObjectReader::IBusObjectReader(const std::string& type_name,
                                   dbus::MessageReader* reader)
    : type_name_(type_name),
      original_reader_(reader),
      top_variant_reader_(NULL),
      contents_reader_(NULL),
      check_result_(IBUS_OBJECT_NOT_CHECKED) {
}

IBusObjectReader::~IBusObjectReader() {
}

bool IBusObjectReader::InitWithoutAttachment() {
  DCHECK(original_reader_);
  DCHECK_EQ(IBUS_OBJECT_NOT_CHECKED, check_result_);

  top_variant_reader_.reset(new dbus::MessageReader(NULL));
  contents_reader_.reset(new dbus::MessageReader(NULL));
  check_result_ = IBUS_OBJECT_INVALID;

  // IBus object has a variant on top-level.
  if (!original_reader_->PopVariant(top_variant_reader_.get())) {
    LOG(ERROR) << "Invalid object structure[" << type_name_ << "]: "
               << "can not find top variant field.";
    return false;
  }

  // IBus object has struct on second level.
  if (!top_variant_reader_->PopStruct(contents_reader_.get())) {
    LOG(ERROR) << "Invalid object structure["  << type_name_ << "]: "
               << "can not find top struct field.";
    return false;
  }

  // IBus object has type key at the first element.
  std::string type_name;
  if (!contents_reader_->PopString(&type_name)) {
    LOG(ERROR) << "Invalid object structure[" << type_name_ << "]: "
               << "Can not get type name field.";
    return false;
  }

  if (type_name != type_name_) {
    LOG(ERROR) << "Type check failed: Given variant is not " << type_name_
               << " and actual type is " << type_name << ".";
    return false;
  }

  return true;
}

bool IBusObjectReader::Init() {
  if (!InitWithoutAttachment())
    return false;

  // Ignores attachment field.
  dbus::MessageReader array_reader(NULL);
  if (!contents_reader_->PopArray(&array_reader)) {
    LOG(ERROR) << "Invalid object structure[" << type_name_ << "] "
               << "can not find attachment array field.";
    return false;
  }
  DLOG_IF(WARNING, array_reader.HasMoreData())
      << "Ignoring attachment field in " << type_name_ <<".";
  check_result_ = IBUS_OBJECT_VALID;
  return true;
}

bool IBusObjectReader::InitWithAttachmentReader(dbus::MessageReader* reader) {
  DCHECK(reader);
  if (!InitWithoutAttachment())
    return false;

  // IBus object has array object at the second element, which is used in
  // attaching additional information.
  if (!contents_reader_->PopArray(reader)) {
    LOG(ERROR) << "Invalid object structure[" << type_name_ << "] "
               << "can not find attachment array field.";
    return false;
  }
  check_result_ = IBUS_OBJECT_VALID;
  return true;
}

bool IBusObjectReader::InitWithParentReader(dbus::MessageReader* reader) {
  original_reader_ = reader;
  return Init();
}

bool IBusObjectReader::PopString(std::string* out) {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  return IsValid() && contents_reader_->PopString(out);
}

bool IBusObjectReader::PopUint32(uint32* out) {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  return IsValid() && contents_reader_->PopUint32(out);
}

bool IBusObjectReader::PopInt32(int32* out) {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  return IsValid() && contents_reader_->PopInt32(out);
}

bool IBusObjectReader::PopBool(bool* out) {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  return IsValid() && contents_reader_->PopBool(out);
}

bool IBusObjectReader::PopArray(dbus::MessageReader* reader) {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  return IsValid() && contents_reader_->PopArray(reader);
}

bool IBusObjectReader::PopIBusText(IBusText* text) {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  return IsValid() && chromeos::ibus::PopIBusText(contents_reader_.get(), text);
}

bool IBusObjectReader::PopStringFromIBusText(std::string* text) {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  return IsValid() && chromeos::ibus::PopStringFromIBusText(
      contents_reader_.get(), text);
}

bool IBusObjectReader::PopIBusProperty(IBusProperty* property) {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  return IsValid() && chromeos::ibus::PopIBusProperty(contents_reader_.get(),
                                                      property);
}

bool IBusObjectReader::PopIBusPropertyList(IBusPropertyList* properties) {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  return IsValid() && chromeos::ibus::PopIBusPropertyList(
      contents_reader_.get(), properties);
}

bool IBusObjectReader::HasMoreData() {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  return IsValid() && contents_reader_->HasMoreData();
}

bool IBusObjectReader::PopIBusObject(IBusObjectReader* reader) {
  DCHECK(contents_reader_.get());
  if (!IsValid())
    return false;
  return reader->InitWithParentReader(contents_reader_.get());
}

bool IBusObjectReader::IsValid() const {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  return check_result_ == IBUS_OBJECT_VALID;
}

///////////////////////////////////////////////////////////////////////////////
//  IBusObjectWriter
IBusObjectWriter::IBusObjectWriter(const std::string& type_name,
                                   const std::string& signature,
                                   dbus::MessageWriter* writer)
    : type_name_(type_name),
      signature_(signature),
      original_writer_(writer) {
  if (original_writer_)
    Init();
}

IBusObjectWriter::~IBusObjectWriter() {
}

void IBusObjectWriter::AppendString(const std::string& input) {
  DCHECK(IsInitialized());
  contents_writer_->AppendString(input);
}

void IBusObjectWriter::AppendUint32(uint32 input) {
  DCHECK(IsInitialized());
  contents_writer_->AppendUint32(input);
}

void IBusObjectWriter::AppendInt32(int32 input) {
  DCHECK(IsInitialized());
  contents_writer_->AppendInt32(input);
}

void IBusObjectWriter::AppendBool(bool input) {
  DCHECK(IsInitialized());
  contents_writer_->AppendBool(input);
}

void IBusObjectWriter::OpenArray(const std::string& signature,
                                 dbus::MessageWriter* writer) {
  DCHECK(IsInitialized());
  contents_writer_->OpenArray(signature, writer);
}

void IBusObjectWriter::AppendIBusText(const IBusText& text) {
  DCHECK(IsInitialized());
  chromeos::ibus::AppendIBusText(text, contents_writer_.get());
}

void IBusObjectWriter::AppendStringAsIBusText(const std::string& text) {
  DCHECK(IsInitialized());
  chromeos::ibus::AppendStringAsIBusText(text, contents_writer_.get());
}

void IBusObjectWriter::AppendIBusProperty(const IBusProperty& property) {
  DCHECK(IsInitialized());
  chromeos::ibus::AppendIBusProperty(property, contents_writer_.get());
}

void IBusObjectWriter::AppendIBusPropertyList(
    const IBusPropertyList& property_list) {
  DCHECK(IsInitialized());
  chromeos::ibus::AppendIBusPropertyList(property_list, contents_writer_.get());
}

void IBusObjectWriter::CloseContainer(dbus::MessageWriter* writer) {
  DCHECK(IsInitialized());
  contents_writer_->CloseContainer(writer);
}

void IBusObjectWriter::AppendIBusObject(IBusObjectWriter* writer) {
  DCHECK(IsInitialized());
  DCHECK(!writer->IsInitialized()) << "Given writer is already initialized";

  writer->InitWithParentWriter(contents_writer_.get());
}

void IBusObjectWriter::Init() {
  DCHECK(original_writer_);
  DCHECK(!IsInitialized());

  top_variant_writer_.reset(new dbus::MessageWriter(NULL));
  contents_writer_.reset(new dbus::MessageWriter(NULL));

  const std::string ibus_signature = "(sa{sv}" + signature_ + ")";
  original_writer_->OpenVariant(ibus_signature, top_variant_writer_.get());
  top_variant_writer_->OpenStruct(contents_writer_.get());

  contents_writer_->AppendString(type_name_);
  dbus::MessageWriter header_array_writer(NULL);

  // There is no case setting any attachment in ChromeOS, so setting empty
  // array is enough.
  contents_writer_->OpenArray("{sv}", &header_array_writer);
  contents_writer_->CloseContainer(&header_array_writer);
}

void IBusObjectWriter::InitWithParentWriter(dbus::MessageWriter* writer) {
  original_writer_ = writer;
  Init();
}

void IBusObjectWriter::CloseAll() {
  DCHECK(original_writer_);
  DCHECK(IsInitialized());

  top_variant_writer_->CloseContainer(contents_writer_.get());
  original_writer_->CloseContainer(top_variant_writer_.get());
  top_variant_writer_.reset();
  contents_writer_.reset();
}

bool IBusObjectWriter::IsInitialized() const {
  return contents_writer_.get() != NULL;
}

}  // namespace ibus
}  // namespace chromeos
