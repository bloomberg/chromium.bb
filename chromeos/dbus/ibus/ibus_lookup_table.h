// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_LOOKUP_TABLE_H_
#define CHROMEOS_DBUS_IBUS_IBUS_LOOKUP_TABLE_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"

namespace dbus {
class MessageWriter;
class MessageReader;
}  // namespace dbus

namespace chromeos {
// TODO(nona): Remove ibus namespace after complete libibus removal.
namespace ibus {

// The IBusLookupTable is one of IBusObjects. IBusLookupTable contains IBusTexts
// but all of them are used as plain string. The overview of each data
// strucutres is as follows:
//
// DATA STRUCTURE OVERVIEW:
//  variant  struct {
//   string "IBusLookupTable"
//   array [
//     dict_entry (
//       string "mozc.candidates"
//       variant  variant  array of bytes [
//         08 01 10 16 ...
//       ]
//     )
//   ]
//   uint32 9  // Page size
//   uint32 1  // Cursor position
//   boolean true  // Cursor visibility.
//   boolean true  // Round lookup table or not. Not used in Chrome.
//   int32 1  // Orientation
//   array [  // Array of candidate text.
//    variant struct {
//      string "IBusText"
//      array []
//      string "Candidate Text"
//      variant struct {
//       string "IBusAttrList"
//       array []
//       array []
//       }
//     }
//     ... more IBusTexts
//   ]
//   array [  // Array of label text
//    variant struct {
//      string "IBusText"
//      array []
//      string "1"
//      variant struct {
//       string "IBusAttrList"
//       array []
//       array []
//       }
//     }
//     ... more IBusTexts
//   ]
//  }
//  TODO(nona): Clean up the structure.(crbug.com/129403)
class IBusLookupTable;

// Pops a IBusLookupTable from |reader|.
// Returns false if an error occures.
bool CHROMEOS_EXPORT PopIBusLookupTable(dbus::MessageReader* reader,
                                        IBusLookupTable* table);
// Appends a IBusLookupTable to |writer| except mozc_candidates_ in |table|.
void CHROMEOS_EXPORT AppendIBusLookupTable(const IBusLookupTable& table,
                                           dbus::MessageWriter* writer);

// An representation of IBusLookupTable object which is used in dbus
// communication with ibus-daemon.
class CHROMEOS_EXPORT IBusLookupTable {
 public:
  enum Orientation {
    IBUS_LOOKUP_TABLE_ORIENTATION_HORIZONTAL = 0,
    IBUS_LOOKUP_TABLE_ORIENTATION_VERTICAL = 1,
  };

  // Represents a candidate entry. In dbus communication, each
  // field is represented as IBusText, but attributes are not used in Chrome.
  // So just simple string is sufficient in this case.
  struct Entry {
    std::string value;
    std::string label;
  };

  IBusLookupTable();
  virtual ~IBusLookupTable();

  // Returns the number of candidates in one page.
  uint32 page_size() const { return page_size_; }
  void set_page_size(uint32 page_size) { page_size_ = page_size; }

  // Returns the cursor index of the currently selected candidate.
  uint32 cursor_position() const { return cursor_position_; }
  void set_cursor_position(uint32 cursor_position) {
    cursor_position_ = cursor_position;
  }

  // Returns true if the cusros is visible.
  bool is_cursor_visible() const { return is_cursor_visible_; }
  void set_is_cursor_visible(bool is_cursor_visible) {
    is_cursor_visible_ = is_cursor_visible;
  }

  // Returns the orientation of lookup table.
  Orientation orientation() const { return orientation_; }
  void set_orientation(Orientation orientation) {
    orientation_ = orientation;
  }

  const std::vector<Entry>& candidates() const { return candidates_; }
  std::vector<Entry>* mutable_candidates() { return &candidates_; }

  const std::string& serialized_mozc_candidates_data() {
    return serialized_mozc_candidates_data_;
  }
  void set_serialized_mozc_candidates_data(
      const std::string& serialized_mozc_candidates_data) {
    serialized_mozc_candidates_data_ = serialized_mozc_candidates_data;
  }

 private:
  uint32 page_size_;
  uint32 cursor_position_;
  bool is_cursor_visible_;
  Orientation orientation_;
  std::vector<Entry> candidates_;

  // TODO(nona): Refine data structure(crbug.com/129403).
  std::string serialized_mozc_candidates_data_;

  DISALLOW_COPY_AND_ASSIGN(IBusLookupTable);
};

}  // namespace ibus
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_LOOKUP_TABLE_H_
