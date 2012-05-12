// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_TEXT_H_
#define CHROMEOS_DBUS_IBUS_IBUS_TEXT_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"

namespace dbus {
class MessageWriter;
class MessageReader;
}  // dbus

namespace chromeos {

// The IBusText is one of IBusObjects and it contains IBusAttrList object which
// contains array of IBusAttribute object. The overview of each data strucutres
// is as follows:
//
// DATA STRUCTURE OVERVIEW:
//
// IBusAttribute: (signature is "uuuu")
// variant  struct {
//     string "IBusAttribute"
//     array[]
//     uint32 1  // Type of attribute.
//     uint32 1  // The value of attribute.
//     uint32 0  // The start index of the text.
//     uint32 1  // The end index of the text.
// }
//
// IBusAttrList: (signature is "av")
// variant  struct {
//     string "IBusAttrList"
//     array[]
//     array[  // The array of IBusAttribute.
//       variant  struct{
//           string "IBusAttribute"
//           ...
//       }
//       variant  struct{
//           string "IBusAttribute"
//           ...
//       }
//       variant  struct{
//           string "IBusAttribute"
//           ...
//       }
//    ]
// }
//
// IBusText: (signature is "sv")
// variant  struct {
//     string "IBusText"
//     array[]
//     string "A"
//     variant  struct {
//         string "IBusAttrList"
//         array[]
//         array[
//           variant  struct{
//               string "IBusAttribute"
//               ...
//             }
//           variant  struct{
//               string "IBusAttribute"
//               ...
//             }
//        ]
//      }
//  }
//
class IBusText;

// Pops a IBusText from |reader|.
// Returns false if an error occures.
bool CHROMEOS_EXPORT PopIBusText(dbus::MessageReader* reader,
                                 IBusText* ibus_text);
// Appends a IBusText to |writer|.
void CHROMEOS_EXPORT AppendIBusText(const IBusText& ibus_text,
                                    dbus::MessageWriter* writer);

// Handles IBusText object which is used in dbus communication with ibus-daemon.
// The IBusAttribute has four uint32 variables and the IBusAttributes represents
// three type of decoration based on it's values.
// 1. Underline decoration (corresponds to UnderlineAttribute structure)
//   1st value: indicates underline attribute.
//   2nd value: type of decoration. Chrome only support single and double
//              underline and error line.
//   3rd value: the start index of this attribute in multibyte.
//   4th value: the end index of this attribute in multibyte.
//
// 2. Background decoration (corresponds to SelectionAttribute structure)
//   NOTE: Background decoration is treated as selection in Chrome.
//   1st value: indicates background attribute.
//   2nd value: Represents color but not supported in Chrome.
//   3rd value: the start index of this attribute in multibyte.
//   4th value: the end index of this attribute in multibyte.
//
// 3. Forward decoration
//   Not supported in Chrome.
// TODO(nona): Support attachment for mozc infolist feature(crosbug.com/30653).
class CHROMEOS_EXPORT IBusText {
 public:
  enum IBusTextUnderlineType {
    IBUS_TEXT_UNDERLINE_SINGLE = 1,
    IBUS_TEXT_UNDERLINE_DOUBLE = 2,
    IBUS_TEXT_UNDERLINE_ERROR  = 4,
  };

  struct UnderlineAttribute {
    IBusTextUnderlineType type;
    uint32 start_index;  // The inclusive start index.
    uint32 end_index;  // The exclusive end index.
  };

  struct SelectionAttribute {
    uint32 start_index;  // The inclusive start index.
    uint32 end_index;  // The exclusive end index.
  };

  // Accessors
  IBusText();
  virtual ~IBusText();

  std::string text() const;
  void set_text(const std::string& text);

  const std::vector<UnderlineAttribute>& underline_attributes() const;
  std::vector<UnderlineAttribute>* mutable_underline_attributes();

  const std::vector<SelectionAttribute>& selection_attributes() const;
  std::vector<SelectionAttribute>* mutable_selection_attributes();

 private:
  std::string text_;
  std::vector<UnderlineAttribute> underline_attributes_;
  std::vector<SelectionAttribute> selection_attributes_;

  DISALLOW_COPY_AND_ASSIGN(IBusText);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_TEXT_H_
