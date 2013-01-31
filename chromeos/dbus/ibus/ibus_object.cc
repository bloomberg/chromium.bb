// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_object.h"

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/dbus/ibus/ibus_property.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "dbus/message.h"
#include "dbus/values_util.h"

namespace chromeos {

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
  for (std::map<std::string, base::Value*>::iterator ite = attachments_.begin();
       ite != attachments_.end(); ++ite)
    delete ite->second;
}

bool IBusObjectReader::Init() {
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

  dbus::MessageReader attachment_reader(NULL);

  // IBus object has array object at the second element, which is used in
  // attaching additional information.
  if (!contents_reader_->PopArray(&attachment_reader)) {
    LOG(ERROR) << "Invalid object structure[" << type_name_ << "] "
               << "can not find attachment array field.";
    return false;
  }

  while (attachment_reader.HasMoreData()) {
    dbus::MessageReader dictionary_reader(NULL);
    if (!attachment_reader.PopDictEntry(&dictionary_reader)) {
      LOG(ERROR) << "Invalid attachment structure: "
                 << "The attachment field is array of dictionary entry.";
      return false;
    }

    std::string key;
    if (!dictionary_reader.PopString(&key)) {
      LOG(ERROR) << "Invalid attachment structure: "
                 << "The 1st dictionary entry should be string.";
      return false;
    }

    if (key.empty()) {
      LOG(ERROR) << "Invalid attachment key: key is empty.";
      return false;
    }

    dbus::MessageReader variant_reader(NULL);
    if (!dictionary_reader.PopVariant(&variant_reader)) {
      LOG(ERROR) << "Invalid attachment structure: "
                 << "The 2nd dictionary entry should be variant.";
      return false;
    }

    dbus::MessageReader sub_variant_reader(NULL);
    if (!variant_reader.PopVariant(&sub_variant_reader)) {
      LOG(ERROR) << "Invalid attachment structure: "
                 << "The 2nd variant entry should contain variant.";
      return false;
    }

    attachments_[key] =  dbus::PopDataAsValue(&sub_variant_reader);
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
  return IsValid() && chromeos::PopIBusText(contents_reader_.get(), text);
}

bool IBusObjectReader::PopStringFromIBusText(std::string* text) {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  return IsValid() && chromeos::PopStringFromIBusText(
      contents_reader_.get(), text);
}

bool IBusObjectReader::PopIBusProperty(IBusProperty* property) {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  return IsValid() && chromeos::PopIBusProperty(contents_reader_.get(),
                                                      property);
}

bool IBusObjectReader::PopIBusPropertyList(IBusPropertyList* properties) {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  return IsValid() && chromeos::PopIBusPropertyList(
      contents_reader_.get(), properties);
}

const base::Value* IBusObjectReader::GetAttachment(const std::string& key) {
  DCHECK_NE(IBUS_OBJECT_NOT_CHECKED, check_result_);
  DCHECK(contents_reader_.get());
  if (!IsValid())
    return NULL;
  std::map<std::string, base::Value*>::iterator it = attachments_.find(key);
  if (it == attachments_.end())
    return NULL;
  return it->second;
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
      original_writer_(writer),
      state_(NOT_INITIALZED) {
  if (original_writer_)
    Init();
}

IBusObjectWriter::~IBusObjectWriter() {
}

void IBusObjectWriter::AppendString(const std::string& input) {
  DCHECK_EQ(state_, INITIALIZED);
  contents_writer_->AppendString(input);
}

void IBusObjectWriter::AppendUint32(uint32 input) {
  DCHECK_EQ(state_, INITIALIZED);
  contents_writer_->AppendUint32(input);
}

void IBusObjectWriter::AppendInt32(int32 input) {
  DCHECK_EQ(state_, INITIALIZED);
  contents_writer_->AppendInt32(input);
}

void IBusObjectWriter::AppendBool(bool input) {
  DCHECK_EQ(state_, INITIALIZED);
  contents_writer_->AppendBool(input);
}

void IBusObjectWriter::OpenArray(const std::string& signature,
                                 dbus::MessageWriter* writer) {
  DCHECK_EQ(state_, INITIALIZED);
  contents_writer_->OpenArray(signature, writer);
}

void IBusObjectWriter::AppendIBusText(const IBusText& text) {
  DCHECK_EQ(state_, INITIALIZED);
  chromeos::AppendIBusText(text, contents_writer_.get());
}

void IBusObjectWriter::AppendStringAsIBusText(const std::string& text) {
  DCHECK_EQ(state_, INITIALIZED);
  chromeos::AppendStringAsIBusText(text, contents_writer_.get());
}

void IBusObjectWriter::AppendIBusProperty(const IBusProperty& property) {
  DCHECK_EQ(state_, INITIALIZED);
  chromeos::AppendIBusProperty(property, contents_writer_.get());
}

void IBusObjectWriter::AppendIBusPropertyList(
    const IBusPropertyList& property_list) {
  DCHECK_EQ(state_, INITIALIZED);
  chromeos::AppendIBusPropertyList(property_list, contents_writer_.get());
}

void IBusObjectWriter::CloseContainer(dbus::MessageWriter* writer) {
  DCHECK_EQ(state_, INITIALIZED);
  contents_writer_->CloseContainer(writer);
}

void IBusObjectWriter::AppendIBusObject(IBusObjectWriter* writer) {
  DCHECK_EQ(state_, INITIALIZED);
  writer->InitWithParentWriter(contents_writer_.get());
}

void IBusObjectWriter::Init() {
  DCHECK(original_writer_);
  DCHECK_EQ(state_, NOT_INITIALZED);

  top_variant_writer_.reset(new dbus::MessageWriter(NULL));
  contents_writer_.reset(new dbus::MessageWriter(NULL));
  attachment_writer_.reset(new dbus::MessageWriter(NULL));

  const std::string ibus_signature = "(sa{sv}" + signature_ + ")";
  original_writer_->OpenVariant(ibus_signature, top_variant_writer_.get());
  top_variant_writer_->OpenStruct(contents_writer_.get());

  contents_writer_->AppendString(type_name_);

  contents_writer_->OpenArray("{sv}", attachment_writer_.get());
  state_ = HEADER_OPEN;
}

void IBusObjectWriter::InitWithParentWriter(dbus::MessageWriter* writer) {
  DCHECK_EQ(state_, NOT_INITIALZED) << "Already initialized.";
  original_writer_ = writer;
  Init();
}

void IBusObjectWriter::CloseAll() {
  DCHECK(original_writer_);
  DCHECK_NE(state_, NOT_INITIALZED);
  if (state_ == HEADER_OPEN)
    CloseHeader();

  top_variant_writer_->CloseContainer(contents_writer_.get());
  original_writer_->CloseContainer(top_variant_writer_.get());
  top_variant_writer_.reset();
  contents_writer_.reset();
}

void IBusObjectWriter::CloseHeader() {
  DCHECK_EQ(state_, HEADER_OPEN) << "Header is already closed.";
  contents_writer_->CloseContainer(attachment_writer_.get());
  state_ = INITIALIZED;
}

bool IBusObjectWriter::AddAttachment(const std::string& key,
                                     const base::Value& value) {
  DCHECK_NE(state_, NOT_INITIALZED) << "Do not call before Init();";
  DCHECK_NE(state_, INITIALIZED) << "Do not call after CloseHeader().";
  DCHECK(attachment_writer_.get());
  DCHECK(!key.empty());
  DCHECK(!value.IsType(base::Value::TYPE_NULL));

  dbus::MessageWriter dict_writer(NULL);
  attachment_writer_->OpenDictEntry(&dict_writer);
  dict_writer.AppendString(key);
  dbus::MessageWriter variant_writer(NULL);
  dict_writer.OpenVariant("v", &variant_writer);

  dbus::AppendBasicTypeValueDataAsVariant(&variant_writer, value);
  dict_writer.CloseContainer(&variant_writer);
  attachment_writer_->CloseContainer(&variant_writer);
  return true;
}

}  // namespace chromeos
