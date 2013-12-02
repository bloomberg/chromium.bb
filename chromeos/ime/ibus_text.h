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

  // Accessors
  IBusText();
  virtual ~IBusText();

  const std::string& text() const { return text_; }
  void set_text(const std::string& text) { text_ = text; }

  const std::vector<UnderlineAttribute>& underline_attributes() const {
    return underline_attributes_;
  }

  std::vector<UnderlineAttribute>* mutable_underline_attributes() {
    return &underline_attributes_;
  }

  uint32 selection_start() const { return selection_start_; }
  void set_selection_start(uint32 selection_start) {
    selection_start_ = selection_start;
  }

  uint32 selection_end() const { return selection_end_; }
  void set_selection_end(uint32 selection_end) {
    selection_end_ = selection_end;
  }

  void CopyFrom(const IBusText& obj);

 private:
  std::string text_;
  std::vector<UnderlineAttribute> underline_attributes_;
  uint32 selection_start_;
  uint32 selection_end_;

  DISALLOW_COPY_AND_ASSIGN(IBusText);
};

}  // namespace chromeos

#endif  // CHROMEOS_IME_IBUS_TEXT_H_
