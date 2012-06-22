// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_lookup_table.h"

#include <string>
#include "base/logging.h"
#include "dbus/message.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "chromeos/dbus/ibus/ibus_object.h"

namespace chromeos {
// TODO(nona): Remove ibus namespace after complete libibus removal.
namespace ibus {

void AppendIBusLookupTable(const IBusLookupTable& table,
                           dbus::MessageWriter* writer) {
  IBusObjectWriter ibus_lookup_table_writer("IBusLookupTable",
                                            "uubbiavav",
                                            writer);
  ibus_lookup_table_writer.AppendUint32(table.page_size());
  ibus_lookup_table_writer.AppendUint32(table.cursor_position());
  ibus_lookup_table_writer.AppendBool(table.is_cursor_visible());
  ibus_lookup_table_writer.AppendBool(false);  // Not used in Chrome.
  ibus_lookup_table_writer.AppendInt32(static_cast<int32>(table.orientation()));

  const std::vector<IBusLookupTable::Entry>& candidates = table.candidates();
  dbus::MessageWriter text_writer(NULL);
  ibus_lookup_table_writer.OpenArray("v", &text_writer);
  bool have_labels = false;
  for (size_t i = 0; i < candidates.size(); ++i) {
    // Write candidate string as IBusText.
    AppendStringAsIBusText(candidates[i].value, &text_writer);
    if (!candidates[i].label.empty())
      have_labels = true;
  }
  ibus_lookup_table_writer.CloseContainer(&text_writer);

  dbus::MessageWriter label_writer(NULL);
  ibus_lookup_table_writer.OpenArray("v", &label_writer);

  // If there are not any labels, don't append labels.
  if (have_labels) {
    for (size_t i = 0; i < candidates.size(); ++i) {
      // Write label string as IBusText.
      AppendStringAsIBusText(candidates[i].label, &label_writer);
    }
  }
  ibus_lookup_table_writer.CloseContainer(&label_writer);

  ibus_lookup_table_writer.CloseAll();
}

bool PopIBusLookupTable(dbus::MessageReader* reader, IBusLookupTable* table) {
  IBusObjectReader ibus_object_reader("IBusLookupTable", reader);

  dbus::MessageReader attachment_reader(NULL);
  if (!ibus_object_reader.InitWithAttachmentReader(&attachment_reader))
    return false;

  while (attachment_reader.HasMoreData()) {
    dbus::MessageReader dictionary_reader(NULL);
    if (!attachment_reader.PopDictEntry(&dictionary_reader)) {
      LOG(ERROR) << "Invalid attachment structure: "
                 << "The attachment field is array of dictionary entry.";
      return false;
    }

    std::string key;
    if (!dictionary_reader.PopString(&key)) {
      LOG(ERROR) << "Invalid attachement structure: "
                 << "The 1st dictionary entry should be string.";
      return false;
    }
    if (key != "mozc.candidates")
      continue;

    dbus::MessageReader variant_reader(NULL);
    if (!dictionary_reader.PopVariant(&variant_reader)) {
      LOG(ERROR) << "Invalid attachment structure: "
                 << "The 2nd dictionary entry shuold be variant.";
      return false;
    }

    dbus::MessageReader sub_variant_reader(NULL);
    if (!variant_reader.PopVariant(&sub_variant_reader)) {
      LOG(ERROR) << "Invalid attachment structure: "
                 << "The 2nd variant entry should contain variant.";
      return false;
    }

    uint8* bytes = NULL;
    size_t length = 0;
    if (!sub_variant_reader.PopArrayOfBytes(&bytes, &length)) {
      LOG(ERROR) << "Invalid mozc.candidates structure: "
                 << "The mozc.candidates contains array of bytes.";
      return false;
    }

    table->set_serialized_mozc_candidates_data(
        std::string(reinterpret_cast<char*>(bytes), length));
  }

  uint32 page_size = 0;
  if (!ibus_object_reader.PopUint32(&page_size)) {
    LOG(ERROR) << "Invalid variant structure[IBusLookupTable]: "
               << "1st argument should be uint32.";
    return false;
  }
  table->set_page_size(page_size);

  uint32 cursor_position = 0;
  if (!ibus_object_reader.PopUint32(&cursor_position)) {
    LOG(ERROR) << "Invalid variant structure[IBusLookupTable]: "
               << "2nd argument should be uint32.";
    return false;
  }
  table->set_cursor_position(cursor_position);

  bool cursor_visible = true;
  if (!ibus_object_reader.PopBool(&cursor_visible)) {
    LOG(ERROR) << "Invalid variant structure[IBusLookupTable]: "
               << "3rd arugment should be boolean.";
    return false;
  }
  table->set_is_cursor_visible(cursor_visible);

  bool unused_round_value = true;
  if (!ibus_object_reader.PopBool(&unused_round_value)) {
    LOG(ERROR) << "Invalid variant structure[IBusLookupTable]: "
               << "4th argument should be boolean.";
    return false;
  }

  int32 orientation = 0;
  if (!ibus_object_reader.PopInt32(&orientation)) {
    LOG(ERROR) << "Invalid variant structure[IBusLookupTable]: "
               << "5th arguemnt should be int32.";
    return false;
  }
  table->set_orientation(
      static_cast<IBusLookupTable::Orientation>(orientation));

  dbus::MessageReader text_array_reader(NULL);
  if (!ibus_object_reader.PopArray(&text_array_reader)) {
    LOG(ERROR) << "Invalid variant structure[IBusLookupTable]: "
               << "6th argument should be array.";
    return false;
  }

  std::vector<IBusLookupTable::Entry>* candidates = table->mutable_candidates();
  while (text_array_reader.HasMoreData()) {
    std::string candidate_text;
    // The attributes in IBusText are not used in Chrome.
    if (!PopStringFromIBusText(&text_array_reader, &candidate_text)) {
      LOG(ERROR) << "Invalid variant structure[IBusLookupTable]: "
                 << "6th argument should be array of IBusText.";
      return false;
    }
    IBusLookupTable::Entry entry;
    entry.value = candidate_text;
    candidates->push_back(entry);
  }

  dbus::MessageReader label_array_reader(NULL);
  if (!ibus_object_reader.PopArray(&label_array_reader)) {
    LOG(ERROR) << "Invalid variant structure[IBusLookupTable]: "
               << "7th argument should be array.";
    return false;
  }

  if (!label_array_reader.HasMoreData()) {
    return true;
  }

  for (size_t i = 0; i < candidates->size(); ++i) {
    if (!label_array_reader.HasMoreData()) {
      LOG(ERROR) << "Invalid variant structure[IBusLookupTable]: "
                 << "The number of label entry does not match with candidate "
                 << "text. Same length or no label entry can be accepted.";
      return false;
    }

    std::string label_text;
    // The attributes in IBusText are not used in Chrome.
    if (!PopStringFromIBusText(&label_array_reader, &label_text)) {
      LOG(ERROR) << "Invalid variant structure[IBusLookupTable]: "
                 << "7th argument should be array of IBusText.";
      return false;
    }
    (*candidates)[i].label = label_text;
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// IBusLookupTable
IBusLookupTable::IBusLookupTable()
    : page_size_(0),
      cursor_position_(0),
      is_cursor_visible_(true),
      orientation_(IBUS_LOOKUP_TABLE_ORIENTATION_HORIZONTAL) {
}

IBusLookupTable::~IBusLookupTable() {
}

}  // namespace ibus
}  // namespace chromeos
