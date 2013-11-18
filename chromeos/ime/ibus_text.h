// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_IBUS_TEXT_H_
#define CHROMEOS_IME_IBUS_TEXT_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

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

  void CopyFrom(const IBusText& obj);

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

#endif  // CHROMEOS_IME_IBUS_TEXT_H_
