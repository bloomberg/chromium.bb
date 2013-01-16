// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_lookup_table.h"

#include <string>
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/dbus/ibus/ibus_object.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "dbus/message.h"

namespace chromeos {
// TODO(nona): Remove ibus namespace after complete libibus removal.
namespace ibus {

namespace {
// The default entry number of a page in IBusLookupTable.
const int kDefaultPageSize = 9;
const char kShowWindowAtCompositionKey[] = "show_window_at_composition";
}  // namespace

void AppendIBusLookupTable(const IBusLookupTable& table,
                           dbus::MessageWriter* writer) {
  IBusObjectWriter ibus_lookup_table_writer("IBusLookupTable",
                                            "uubbiavav",
                                            writer);
  scoped_ptr<base::Value> show_position(
      base::Value::CreateBooleanValue(table.show_window_at_composition()));
  ibus_lookup_table_writer.AddAttachment(kShowWindowAtCompositionKey,
                                         *show_position.get());
  ibus_lookup_table_writer.CloseHeader();
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
    ibus::IBusText text;
    text.set_text(candidates[i].value);
    text.set_annotation(candidates[i].annotation);
    text.set_description_title(candidates[i].description_title);
    text.set_description_body(candidates[i].description_body);
    AppendIBusText(text, &text_writer);
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

  if (!ibus_object_reader.Init())
    return false;

  const base::Value* value =
      ibus_object_reader.GetAttachment(kShowWindowAtCompositionKey);
  if (value) {
    bool show_window_at_composition;
    if (value->GetAsBoolean(&show_window_at_composition))
      table->set_show_window_at_composition(show_window_at_composition);
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
               << "3rd argument should be boolean.";
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
               << "5th argument should be int32.";
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
    ibus::IBusText candidate_text;
    // The attributes in IBusText are not used in Chrome.
    if (!PopIBusText(&text_array_reader, &candidate_text)) {
      LOG(ERROR) << "Invalid variant structure[IBusLookupTable]: "
                 << "6th argument should be array of IBusText.";
      return false;
    }
    IBusLookupTable::Entry entry;
    entry.value = candidate_text.text();
    entry.annotation = candidate_text.annotation();
    entry.description_title = candidate_text.description_title();
    entry.description_body = candidate_text.description_body();
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
    : page_size_(kDefaultPageSize),
      cursor_position_(0),
      is_cursor_visible_(true),
      orientation_(HORIZONTAL),
      show_window_at_composition_(false) {
}

IBusLookupTable::~IBusLookupTable() {
}

bool IBusLookupTable::IsEqual(const IBusLookupTable& table) const {
  if (page_size_ != table.page_size_ ||
      cursor_position_ != table.cursor_position_ ||
      is_cursor_visible_ != table.is_cursor_visible_ ||
      orientation_ != table.orientation_ ||
      show_window_at_composition_ != table.show_window_at_composition_ ||
      candidates_.size() != table.candidates_.size())
    return false;

  for (size_t i = 0; i < candidates_.size(); ++i) {
    const Entry& left = candidates_[i];
    const Entry& right = table.candidates_[i];
    if (left.value != right.value ||
        left.label != right.label ||
        left.annotation != right.annotation ||
        left.description_title != right.description_title ||
        left.description_body != right.description_body)
      return false;
  }
  return true;
}

void IBusLookupTable::CopyFrom(const IBusLookupTable& table) {
  page_size_ = table.page_size_;
  cursor_position_ = table.cursor_position_;
  is_cursor_visible_ = table.is_cursor_visible_;
  orientation_ = table.orientation_;
  show_window_at_composition_ = table.show_window_at_composition_;
  candidates_.clear();
  candidates_ = table.candidates_;
}

IBusLookupTable::Entry::Entry() {
}

IBusLookupTable::Entry::~Entry() {
}

}  // namespace ibus
}  // namespace chromeos
