// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_component.h"

#include "base/logging.h"
#include "chromeos/dbus/ibus/ibus_object.h"
#include "dbus/message.h"

namespace chromeos {

namespace {

bool PopIBusEngineDesc(dbus::MessageReader* reader,
                       IBusComponent::EngineDescription* engine_desc) {
  IBusObjectReader ibus_object_reader("IBusEngineDesc", reader);
  if (!ibus_object_reader.Init())
    return false;
  if (!ibus_object_reader.PopString(&engine_desc->engine_id)) {
    LOG(ERROR) << "Invalid variant structure[IBusEngineDesc]: "
               << "The 1st argument should be string.";
    return false;
  }
  if (!ibus_object_reader.PopString(&engine_desc->display_name)) {
    LOG(ERROR) << "Invalid variant structure[IBusEngineDesc]: "
               << "The 2nd argument should be string.";
    return false;
  }
  if (!ibus_object_reader.PopString(&engine_desc->description)) {
    LOG(ERROR) << "Invalid variant structure[IBusEngineDesc]: "
               << "The 3rd argument should be string.";
    return false;
  }
  if (!ibus_object_reader.PopString(&engine_desc->language_code)) {
    LOG(ERROR) << "Invalid variant structure[IBusEngineDesc]: "
               << "The 4th argument should be string.";
    return false;
  }
  std::string unused_string_field;
  if (!ibus_object_reader.PopString(&unused_string_field)) {
    LOG(ERROR) << "Invalid variant structure[IBusEngineDesc]: "
               << "The 5th argument should be string.";
    return false;
  }
  if (!ibus_object_reader.PopString(&engine_desc->author)) {
    LOG(ERROR) << "Invalid variant structure[IBusEngineDesc]: "
               << "The 6th argument should be string.";
    return false;
  }
  if (!ibus_object_reader.PopString(&unused_string_field)) {
    LOG(ERROR) << "Invalid variant structure[IBusEngineDesc]: "
               << "The 7th argument should be string.";
    return false;
  }
  if (!ibus_object_reader.PopString(&engine_desc->layout)) {
    LOG(ERROR) << "Invalid variant structure[IBusEngineDesc]: "
               << "The 8th argument should be string.";
    return false;
  }
  uint32 unused_uint_field = 0;
  if (!ibus_object_reader.PopUint32(&unused_uint_field)) {
    LOG(ERROR) << "Invalid variant structure[IBusEngineDesc]: "
               << "The 9th argument should be unsigned integer.";
    return false;
  }
  if (!ibus_object_reader.PopString(&unused_string_field)) {
    LOG(ERROR) << "Invalid variant structure[IBusEngineDesc]: "
               << "The 10th argument should be string.";
    return false;
  }
  if (!ibus_object_reader.PopString(&unused_string_field)) {
    LOG(ERROR) << "Invalid variant structure[IBusEngineDesc]: "
               << "The 11th argument should be string.";
    return false;
  }
  if (!ibus_object_reader.PopString(&unused_string_field)) {
    LOG(ERROR) << "Invalid variant structure[IBusEngineDesc]: "
               << "The 12th argument should be string.";
    return false;
  }
  return true;
}

void AppendIBusEngineDesc(const IBusComponent::EngineDescription& engine_desc,
                          dbus::MessageWriter* writer) {
  IBusObjectWriter ibus_object_writer("IBusEngineDesc",
                                      "ssssssssusss",
                                      writer);
  ibus_object_writer.CloseHeader();
  ibus_object_writer.AppendString(engine_desc.engine_id);
  ibus_object_writer.AppendString(engine_desc.display_name);
  ibus_object_writer.AppendString(engine_desc.description);
  ibus_object_writer.AppendString(engine_desc.language_code);
  ibus_object_writer.AppendString("");  // The license field is not used.
  ibus_object_writer.AppendString(engine_desc.author);
  ibus_object_writer.AppendString("");  // The icon path field is not used.
  ibus_object_writer.AppendString(engine_desc.layout);
  ibus_object_writer.AppendUint32(0);  // The engine rank is not used.
  ibus_object_writer.AppendString("");  // The hotkey field is not used.
  ibus_object_writer.AppendString("");  // The symbol field is not used.
  ibus_object_writer.AppendString("");  // The command line field is not used.
  ibus_object_writer.CloseAll();
}

}  // namespace

bool CHROMEOS_EXPORT PopIBusComponent(dbus::MessageReader* reader,
                                      IBusComponent* ibus_component) {
  IBusObjectReader ibus_object_reader("IBusComponent", reader);
  if (!ibus_object_reader.Init())
    return false;
  std::string name;
  if (!ibus_object_reader.PopString(&name)) {
    LOG(ERROR) << "Invalid variant structure[IBusComponent]: "
               << "The 1st argument should be string.";
    return false;
  }
  ibus_component->set_name(name);

  std::string description;
  if (!ibus_object_reader.PopString(&description)) {
    LOG(ERROR) << "Invalid variant structure[IBusComponent]: "
               << "The 2nd argument should be string.";
    return false;
  }
  ibus_component->set_description(description);

  std::string unused_string_field;
  if (!ibus_object_reader.PopString(&unused_string_field)) {
    LOG(ERROR) << "Invalid variant structure[IBusComponent]: "
               << "The 3rd argument should be string.";
    return false;
  }
  if (!ibus_object_reader.PopString(&unused_string_field)) {
    LOG(ERROR) << "Invalid variant structure[IBusComponent]: "
               << "The 4th argument should be string.";
    return false;
  }

  std::string author;
  if (!ibus_object_reader.PopString(&author)) {
    LOG(ERROR) << "Invalid variant structure[IBusComponent]: "
               << "The 5th argument should be string.";
    return false;
  }
  ibus_component->set_author(author);

  if (!ibus_object_reader.PopString(&unused_string_field)) {
    LOG(ERROR) << "Invalid variant structure[IBusComponent]: "
               << "The 6th argument should be string.";
    return false;
  }
  if (!ibus_object_reader.PopString(&unused_string_field)) {
    LOG(ERROR) << "Invalid variant structure[IBusComponent]: "
               << "The 7th argument should be string.";
    return false;
  }
  if (!ibus_object_reader.PopString(&unused_string_field)) {
    LOG(ERROR) << "Invalid variant structure[IBusComponent]: "
               << "The 8th argument should be string.";
    return false;
  }
  dbus::MessageReader observer_reader(NULL);
  if (!ibus_object_reader.PopArray(&observer_reader)) {
    LOG(ERROR) << "Invalid variant structure[IBusComponent]: "
               << "The 9th argument should be array of variant.";
    return false;
  }

  dbus::MessageReader engine_array_reader(NULL);
  if (!ibus_object_reader.PopArray(&engine_array_reader)) {
    LOG(ERROR) << "Invalid variant structure[IBusComponent]: "
               << "The 10th argument should be array of IBusEngineDesc";
    return false;
  }
  std::vector<IBusComponent::EngineDescription>* engine_description =
      ibus_component->mutable_engine_description();
  engine_description->clear();
  while (engine_array_reader.HasMoreData()) {
    IBusComponent::EngineDescription engine_description_entry;
    if (!PopIBusEngineDesc(&engine_array_reader, &engine_description_entry)) {
      LOG(ERROR) << "Invalid variant structure[IBusComponent]: "
                 << "The 11th argument should be array of IBusEngineDesc";
      return false;
    }
    engine_description->push_back(engine_description_entry);
  }
  return true;
}

void CHROMEOS_EXPORT AppendIBusComponent(const IBusComponent& ibus_component,
                                         dbus::MessageWriter* writer) {
  IBusObjectWriter ibus_object_writer("IBusComponent", "ssssssssavav", writer);
  ibus_object_writer.CloseHeader();
  ibus_object_writer.AppendString(ibus_component.name());
  ibus_object_writer.AppendString(ibus_component.description());
  ibus_object_writer.AppendString("");  // The version string is not used.
  ibus_object_writer.AppendString("");  // The license field is not used.
  ibus_object_writer.AppendString(ibus_component.author());
  ibus_object_writer.AppendString("");  // The URL field is not used.
  ibus_object_writer.AppendString("");  // The exec path field is not used.
  ibus_object_writer.AppendString("");  // The text domain field is not used.
  // The observed object field is not used.
  dbus::MessageWriter empty_array_writer(NULL);
  ibus_object_writer.OpenArray("v", &empty_array_writer);
  ibus_object_writer.CloseContainer(&empty_array_writer);

  dbus::MessageWriter engine_descs_writer(NULL);
  const std::vector<IBusComponent::EngineDescription> engine_description =
      ibus_component.engine_description();
  ibus_object_writer.OpenArray("v", &engine_descs_writer);
  for (size_t i = 0; i < engine_description.size(); ++i) {
    AppendIBusEngineDesc(engine_description[i], &engine_descs_writer);
  }
  ibus_object_writer.CloseContainer(&engine_descs_writer);
  ibus_object_writer.CloseAll();
}

///////////////////////////////////////////////////////////////////////////////
// IBusComponent
IBusComponent::IBusComponent() {
}

IBusComponent::~IBusComponent() {
}

IBusComponent::EngineDescription::EngineDescription() {
}

IBusComponent::EngineDescription::EngineDescription(
    const std::string& engine_id,
    const std::string& display_name,
    const std::string& description,
    const std::string& language_code,
    const std::string& author,
    const std::string& layout)
    : engine_id(engine_id),
      display_name(display_name),
      description(description),
      language_code(language_code),
      author(author),
      layout(layout) {
}

IBusComponent::EngineDescription::~EngineDescription() {
}

}  // namespace chromeos
