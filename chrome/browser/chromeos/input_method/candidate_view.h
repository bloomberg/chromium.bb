// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_VIEW_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/chromeos/input_method/ibus_ui_controller.h"
#include "chromeos/dbus/ibus/ibus_lookup_table.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace chromeos {
namespace input_method {

class CandidateWindowView;

// CandidateView renderes a row of a candidate.
class CandidateView : public views::View {
 public:
  CandidateView(CandidateWindowView* parent_candidate_window,
                int index_in_page,
                ibus::IBusLookupTable::Orientation orientation);
  virtual ~CandidateView() {}
  // Initializes the candidate view with the given column widths.
  // A width of 0 means that the column is resizable.
  void Init(int shortcut_column_width,
            int candidate_column_width,
            int annotation_column_width,
            int column_height);

  // Sets candidate text to the given text.
  void SetCandidateText(const string16& text);

  // Sets shortcut text to the given text.
  void SetShortcutText(const string16& text);

  // Sets annotation text to the given text.
  void SetAnnotationText(const string16& text);

  // Sets infolist icon.
  void SetInfolistIcon(bool enable);

  // Selects the candidate row. Changes the appearance to make it look
  // like a selected candidate.
  void Select();

  // Unselects the candidate row. Changes the appearance to make it look
  // like an unselected candidate.
  void Unselect();

  // Enables or disables the candidate row based on |enabled|. Changes the
  // appearance to make it look like unclickable area.
  void SetRowEnabled(bool enabled);

  // Returns the relative position of the candidate label.
  gfx::Point GetCandidateLabelPosition() const;

 private:
  friend class CandidateWindowViewTest;
  FRIEND_TEST_ALL_PREFIXES(CandidateWindowViewTest, ShortcutSettingTest);
  // Overridden from View:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;

  // Notifies labels of their new background colors.  Called whenever the view's
  // background color changes.
  void UpdateLabelBackgroundColors();

  // Zero-origin index in the current page.
  int index_in_page_;

  // The orientation of the candidate view.
  ibus::IBusLookupTable::Orientation orientation_;

  // The parent candidate window that contains this view.
  CandidateWindowView* parent_candidate_window_;

  // Views created in the class will be part of tree of |this|, so these
  // child views will be deleted when |this| is deleted.

  // The shortcut label renders shortcut numbers like 1, 2, and 3.
  views::Label* shortcut_label_;
  // The candidate label renders candidates.
  views::Label* candidate_label_;
  // The annotation label renders annotations.
  views::Label* annotation_label_;

  // The infolist icon.
  views::View* infolist_icon_;
  bool infolist_icon_enabled_;

  DISALLOW_COPY_AND_ASSIGN(CandidateView);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_VIEW_H_
