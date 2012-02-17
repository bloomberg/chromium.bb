// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/property.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"

#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace dbus {

//
// PropertyBase implementation.
//

void PropertyBase::Init(PropertySet* property_set, const std::string& name) {
  DCHECK(!property_set_);
  property_set_ = property_set;
  name_ = name;
}


//
// PropertySet implementation.
//

PropertySet::PropertySet(ObjectProxy* object_proxy,
                         const std::string& interface,
                         PropertyChangedCallback property_changed_callback)
    : object_proxy_(object_proxy),
      interface_(interface),
      property_changed_callback_(property_changed_callback),
      weak_ptr_factory_(this) {}

PropertySet::~PropertySet() {
}

void PropertySet::RegisterProperty(const std::string& name,
                                   PropertyBase* property) {
  property->Init(this, name);
  properties_map_[name] = property;
}

void PropertySet::ConnectSignals() {
  DCHECK(object_proxy_);
  object_proxy_->ConnectToSignal(
      kPropertiesInterface,
      kPropertiesChanged,
      base::Bind(&PropertySet::ChangedReceived,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&PropertySet::ChangedConnected,
                 weak_ptr_factory_.GetWeakPtr()));
}


void PropertySet::ChangedReceived(Signal* signal) {
  DCHECK(signal);
  MessageReader reader(signal);

  std::string interface;
  if (!reader.PopString(&interface)) {
    LOG(WARNING) << "Property changed signal has wrong parameters: "
                 << "expected interface name: " << signal->ToString();
    return;
  }

  if (interface != this->interface())
    return;

  if (!UpdatePropertiesFromReader(&reader)) {
    LOG(WARNING) << "Property changed signal has wrong parameters: "
                 << "expected dictionary: " << signal->ToString();
  }

  // TODO(keybuk): dbus properties api has invalidated properties array
  // on the end, we don't handle this right now because I don't know of
  // any service that sends it - or what they expect us to do with it.
  // Add later when we need it.
}

void PropertySet::ChangedConnected(const std::string& interface_name,
                                   const std::string& signal_name,
                                   bool success) {
  LOG_IF(WARNING, !success) << "Failed to connect to " << signal_name
                            << "signal.";
}


void PropertySet::GetAll() {
  MethodCall method_call(kPropertiesInterface, kPropertiesGetAll);
  MessageWriter writer(&method_call);
  writer.AppendString(interface());

  DCHECK(object_proxy_);
  object_proxy_->CallMethod(&method_call,
                            ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::Bind(&PropertySet::OnGetAll,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void PropertySet::OnGetAll(Response* response) {
  if (!response) {
    LOG(WARNING) << "GetAll request failed.";
    return;
  }

  MessageReader reader(response);
  if (!UpdatePropertiesFromReader(&reader)) {
    LOG(WARNING) << "GetAll response has wrong parameters: "
                 << "expected dictionary: " << response->ToString();
  }
}


bool PropertySet::UpdatePropertiesFromReader(MessageReader* reader) {
  DCHECK(reader);
  MessageReader array_reader(NULL);
  if (!reader->PopArray(&array_reader))
    return false;

  while (array_reader.HasMoreData()) {
    MessageReader dict_entry_reader(NULL);
    if (!array_reader.PopDictEntry(&dict_entry_reader))
      continue;

    if (!UpdatePropertyFromReader(&dict_entry_reader))
      continue;
  }

  return true;
}

bool PropertySet::UpdatePropertyFromReader(MessageReader* reader) {
  DCHECK(reader);

  std::string name;
  if (!reader->PopString(&name))
    return false;

  PropertiesMap::iterator it = properties_map_.find(name);
  if (it == properties_map_.end())
    return false;

  PropertyBase* property = it->second;
  if (property->PopValueFromReader(reader)) {
    NotifyPropertyChanged(name);
    return true;
  } else {
    return false;
  }
}


void PropertySet::NotifyPropertyChanged(const std::string& name) {
  if (!property_changed_callback_.is_null())
    property_changed_callback_.Run(name);
}

//
// Property<Byte> specialization.
//

template <>
Property<uint8>::Property() : value_(0),
                              weak_ptr_factory_(this) {
}

template <>
bool Property<uint8>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfByte(&value_);
}

template <>
void Property<uint8>::AppendToWriter(MessageWriter* writer,
                                     const uint8& value) {
  writer->AppendVariantOfByte(value);
}

//
// Property<bool> specialization.
//

template <>
Property<bool>::Property() : value_(false),
                             weak_ptr_factory_(this) {
}

template <>
bool Property<bool>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfBool(&value_);
}

template <>
void Property<bool>::AppendToWriter(MessageWriter* writer,
                                    const bool& value) {
  writer->AppendVariantOfBool(value);
}

//
// Property<int16> specialization.
//

template <>
Property<int16>::Property() : value_(0),
                              weak_ptr_factory_(this) {
}

template <>
bool Property<int16>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfInt16(&value_);
}

template <>
void Property<int16>::AppendToWriter(MessageWriter* writer,
                                     const int16& value) {
  writer->AppendVariantOfInt16(value);
}

//
// Property<uint16> specialization.
//

template <>
Property<uint16>::Property() : value_(0),
                               weak_ptr_factory_(this) {
}

template <>
bool Property<uint16>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfUint16(&value_);
}

template <>
void Property<uint16>::AppendToWriter(MessageWriter* writer,
                                      const uint16& value) {
  writer->AppendVariantOfUint16(value);
}

//
// Property<int32> specialization.
//

template <>
Property<int32>::Property() : value_(0),
                              weak_ptr_factory_(this) {
}

template <>
bool Property<int32>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfInt32(&value_);
}

template <>
void Property<int32>::AppendToWriter(MessageWriter* writer,
                                     const int32& value) {
  writer->AppendVariantOfInt32(value);
}

//
// Property<uint32> specialization.
//

template <>
Property<uint32>::Property() : value_(0),
                               weak_ptr_factory_(this) {
}

template <>
bool Property<uint32>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfUint32(&value_);
}

template <>
void Property<uint32>::AppendToWriter(MessageWriter* writer,
                                      const uint32& value) {
  writer->AppendVariantOfUint32(value);
}

//
// Property<int64> specialization.
//

template <>
Property<int64>::Property() : value_(0),
                              weak_ptr_factory_(this) {
}

template <>
bool Property<int64>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfInt64(&value_);
}

template <>
void Property<int64>::AppendToWriter(MessageWriter* writer,
                                     const int64& value) {
  writer->AppendVariantOfInt64(value);
}

//
// Property<uint64> specialization.
//

template <>
Property<uint64>::Property() : value_(0),
                               weak_ptr_factory_(this) {
}

template <>
bool Property<uint64>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfUint64(&value_);
}

template <>
void Property<uint64>::AppendToWriter(MessageWriter* writer,
                                      const uint64& value) {
  writer->AppendVariantOfUint64(value);
}

//
// Property<double> specialization.
//

template <>
Property<double>::Property() : value_(0.0),
                               weak_ptr_factory_(this) {
}

template <>
bool Property<double>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfDouble(&value_);
}

template <>
void Property<double>::AppendToWriter(MessageWriter* writer,
                                      const double& value) {
  writer->AppendVariantOfDouble(value);
}

//
// Property<std::string> specialization.
//

template <>
bool Property<std::string>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfString(&value_);
}

template <>
void Property<std::string>::AppendToWriter(MessageWriter* writer,
                                           const std::string& value) {
  writer->AppendVariantOfString(value);
}

//
// Property<ObjectPath> specialization.
//

template <>
bool Property<ObjectPath>::PopValueFromReader(MessageReader* reader) {
  return reader->PopVariantOfObjectPath(&value_);
}

template <>
void Property<ObjectPath>::AppendToWriter(MessageWriter* writer,
                                          const ObjectPath& value) {
  writer->AppendVariantOfObjectPath(value);
}

//
// Property<std::vector<std::string> > specialization.
//

template <>
bool Property<std::vector<std::string> >::PopValueFromReader(
    MessageReader* reader) {
  MessageReader variant_reader(NULL);
  if (!reader->PopVariant(&variant_reader))
    return false;

  return variant_reader.PopArrayOfStrings(&value_);
}

template <>
void Property<std::vector<std::string> >::AppendToWriter(
    MessageWriter* writer,
    const std::vector<std::string>& value) {
  MessageWriter variant_writer(NULL);
  writer->OpenVariant("as", &variant_writer);
  variant_writer.AppendArrayOfStrings(value);
  writer->CloseContainer(&variant_writer);
}

//
// Property<std::vector<ObjectPath> > specialization.
//

template <>
bool Property<std::vector<ObjectPath> >::PopValueFromReader(
    MessageReader* reader) {
  MessageReader variant_reader(NULL);
  if (!reader->PopVariant(&variant_reader))
    return false;

  return variant_reader.PopArrayOfObjectPaths(&value_);
}

template <>
void Property<std::vector<ObjectPath> >::AppendToWriter(
    MessageWriter* writer,
    const std::vector<ObjectPath>& value) {
  MessageWriter variant_writer(NULL);
  writer->OpenVariant("ao", &variant_writer);
  variant_writer.AppendArrayOfObjectPaths(value);
  writer->CloseContainer(&variant_writer);
}

}  // namespace dbus
