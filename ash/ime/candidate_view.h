// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_CANDIDATE_VIEW_H_
#define ASH_IME_CANDIDATE_VIEW_H_

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {
namespace ime {

// CandidateView renderes a row of a candidate.
class ASH_EXPORT CandidateView : public views::CustomButton {
 public:
  CandidateView(views::ButtonListener* listener,
                ui::CandidateWindow::Orientation orientation);
  virtual ~CandidateView() {}

  void GetPreferredWidths(int* shortcut_width,
                          int* candidate_width);

  void SetWidths(int shortcut_width,
                 int candidate_width);

  void SetEntry(const ui::CandidateWindow::Entry& entry);

  // Sets infolist icon.
  void SetInfolistIcon(bool enable);

  void SetHighlighted(bool highlighted);

 private:
  friend class CandidateWindowViewTest;
  FRIEND_TEST_ALL_PREFIXES(CandidateWindowViewTest, ShortcutSettingTest);

  // Overridden from views::CustomButton:
  virtual void StateChanged() OVERRIDE;

  // Overridden from View:
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

  // The orientation of the candidate view.
  ui::CandidateWindow::Orientation orientation_;

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

  int shortcut_width_;
  int candidate_width_;
  bool highlighted_;

  DISALLOW_COPY_AND_ASSIGN(CandidateView);
};

}  // namespace ime
}  // namespace ash

#endif  // ASH_IME_CANDIDATE_VIEW_H_
