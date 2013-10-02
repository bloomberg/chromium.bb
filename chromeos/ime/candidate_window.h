// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_CANDIDATE_WINDOW_H_
#define CHROMEOS_IME_CANDIDATE_WINDOW_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace input_method {

// CandidateWindow represents the structure of candidates generated from IME.
class CHROMEOS_EXPORT CandidateWindow {
 public:
  enum Orientation {
    HORIZONTAL = 0,
    VERTICAL = 1,
  };

  struct CandidateWindowProperty {
    CandidateWindowProperty();
    virtual ~CandidateWindowProperty();
    int page_size;
    int cursor_position;
    bool is_cursor_visible;
    bool is_vertical;
    bool show_window_at_composition;
  };

  // Represents a candidate entry.
  struct Entry {
    Entry();
    virtual ~Entry();
    std::string value;
    std::string label;
    std::string annotation;
    std::string description_title;
    std::string description_body;
  };

  CandidateWindow();
  virtual ~CandidateWindow();

  // Returns true if the given |candidate_window| is equal to myself.
  bool IsEqual(const CandidateWindow& candidate_window) const;

  // Copies |candidate_window| to myself.
  void CopyFrom(const CandidateWindow& candidate_window);

  const CandidateWindowProperty& GetProperty() const {
    return *property_;
  }
  void SetProperty(const CandidateWindowProperty& property) {
    *property_ = property;
  }

  // Returns the number of candidates in one page.
  uint32 page_size() const { return property_->page_size; }
  void set_page_size(uint32 page_size) { property_->page_size = page_size; }

  // Returns the cursor index of the currently selected candidate.
  uint32 cursor_position() const { return property_->cursor_position; }
  void set_cursor_position(uint32 cursor_position) {
    property_->cursor_position = cursor_position;
  }

  // Returns true if the cursor is visible.
  bool is_cursor_visible() const { return property_->is_cursor_visible; }
  void set_is_cursor_visible(bool is_cursor_visible) {
    property_->is_cursor_visible = is_cursor_visible;
  }

  // Returns the orientation of the candidate window.
  Orientation orientation() const {
    return property_->is_vertical ? VERTICAL : HORIZONTAL;
  }
  void set_orientation(Orientation orientation) {
    property_->is_vertical = (orientation == VERTICAL);
  }

  const std::vector<Entry>& candidates() const { return candidates_; }
  std::vector<Entry>* mutable_candidates() { return &candidates_; }

  bool show_window_at_composition() const {
    return property_->show_window_at_composition;
  }
  void set_show_window_at_composition(bool show_window_at_composition) {
    property_->show_window_at_composition = show_window_at_composition;
  }

 private:
  scoped_ptr<CandidateWindowProperty> property_;
  std::vector<Entry> candidates_;

  DISALLOW_COPY_AND_ASSIGN(CandidateWindow);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROMEOS_IME_CANDIDATE_WINDOW_H_
