// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_INFOLIST_WINDOW_H_
#define ASH_IME_INFOLIST_WINDOW_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "ui/base/ime/infolist_entry.h"
#include "ui/gfx/font_list.h"
#include "ui/views/bubble/bubble_delegate.h"

namespace ash {
namespace ime {

class InfolistEntryView;

// A widget delegate representing the infolist window UI.
class ASH_EXPORT InfolistWindow : public views::BubbleDelegateView {
 public:
  InfolistWindow(views::View* candidate_window,
                 const std::vector<ui::InfolistEntry>& entries);
  virtual ~InfolistWindow();
  void InitWidget();

  // Updates infolist contents with |entries|.
  void Relayout(const std::vector<ui::InfolistEntry>& entries);

  // Show/hide itself with a delay.
  void ShowWithDelay();
  void HideWithDelay();

  // Show/hide without delays.
  void ShowImmediately();
  void HideImmediately();

 private:
  // views::WidgetDelegate implementation.
  virtual void WindowClosing() OVERRIDE;

  // The list of visible entries. Owned by views hierarchy.
  std::vector<InfolistEntryView*> entry_views_;

  // Information title font.
  gfx::FontList title_font_list_;

  // Information description font.
  gfx::FontList description_font_list_;

  base::OneShotTimer<views::Widget> show_hide_timer_;

  DISALLOW_COPY_AND_ASSIGN(InfolistWindow);
};

}  // namespace ime
}  // namespace ash

#endif  // ASH_IME_INFOLIST_WINDOW_H_
