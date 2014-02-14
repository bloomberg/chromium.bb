// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_COMPOSITION_TEXT_H_
#define CHROMEOS_IME_COMPOSITION_TEXT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

class CHROMEOS_EXPORT CompositionText {
 public:
  enum UnderlineType {
    COMPOSITION_TEXT_UNDERLINE_SINGLE = 1,
    COMPOSITION_TEXT_UNDERLINE_DOUBLE = 2,
    COMPOSITION_TEXT_UNDERLINE_ERROR  = 4,
  };

  struct UnderlineAttribute {
    UnderlineType type;
    uint32 start_index;  // The inclusive start index.
    uint32 end_index;  // The exclusive end index.
  };

  CompositionText();
  virtual ~CompositionText();

  // Accessors
  const base::string16& text() const { return text_; }
  void set_text(const base::string16& text) { text_ = text; }

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

  void CopyFrom(const CompositionText& obj);

 private:
  base::string16 text_;
  std::vector<UnderlineAttribute> underline_attributes_;
  uint32 selection_start_;
  uint32 selection_end_;

  DISALLOW_COPY_AND_ASSIGN(CompositionText);
};

}  // namespace chromeos

#endif  // CHROMEOS_IME_COMPOSITION_TEXT_H_
