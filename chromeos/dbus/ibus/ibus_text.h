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
// contains array of IBusAttribute object. The overview of each data structure
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
// Returns false if an error occurs.
bool CHROMEOS_EXPORT PopIBusText(dbus::MessageReader* reader,
                                 IBusText* ibus_text);
// Pops a IBusText from |reader| and stores it's text field into text. Use
// PopIBusText instead in the case of using any attribute entries in IBusText.
// Returns true on success.
bool CHROMEOS_EXPORT PopStringFromIBusText(dbus::MessageReader* reader,
                                           std::string* text);
// Appends a IBusText to |writer|. Annotation and description field is not
// filled with AppendIBusText.
// TODO(nona): Support annotation/description appending if necessary.
void CHROMEOS_EXPORT AppendIBusText(const IBusText& ibus_text,
                                    dbus::MessageWriter* writer);

// Appends a string to |writer| as IBusText without any attributes. Use
// AppendIBusText instead in the case of using any attribute entries.
// TODO(nona): Support annotation/description appending if necessary.
void CHROMEOS_EXPORT AppendStringAsIBusText(const std::string& text,
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

  const std::string& text() const { return text_; }
  void set_text(const std::string& text) { text_ = text; }

  const std::string& annotation() const { return annotation_; }
  void set_annotation(const std::string& annotation) {
    annotation_ = annotation;
  }

  const std::string& description_title() const { return description_title_; }
  void set_description_title(const std::string& title) {
    description_title_ = title;
  }

  const std::string& description_body() const { return description_body_; }
  void set_description_body(const std::string& body) {
    description_body_ = body;
  }

  const std::vector<UnderlineAttribute>& underline_attributes() const {
    return underline_attributes_;
  }

  std::vector<UnderlineAttribute>* mutable_underline_attributes() {
    return &underline_attributes_;
  }

  const std::vector<SelectionAttribute>& selection_attributes() const {
    return selection_attributes_;
  }
  std::vector<SelectionAttribute>* mutable_selection_attributes() {
    return &selection_attributes_;
  }

 private:
  std::string text_;
  std::string annotation_;
  std::string description_title_;
  std::string description_body_;
  std::vector<UnderlineAttribute> underline_attributes_;
  std::vector<SelectionAttribute> selection_attributes_;

  DISALLOW_COPY_AND_ASSIGN(IBusText);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_TEXT_H_
