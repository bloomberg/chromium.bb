// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INFOLIST_WINDOW_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INFOLIST_WINDOW_VIEW_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/view.h"
#include "base/timer.h"

namespace gfx {
class Font;
}

namespace chromeos {
namespace input_method {

class InfolistView;
struct InputMethodLookupTable;

// InfolistWindowView is the main container of the infolist window UI.
class InfolistWindowView : public views::View {
 public:
  // Represents an infolist entry.
  struct Entry {
    std::string title;
    std::string body;
  };

  // InfolistWindowView does not take the ownership of |parent_frame| or
  // |candidate_window_frame|.
  InfolistWindowView(views::Widget* parent_frame,
                     views::Widget* candidate_window_frame);

  virtual ~InfolistWindowView();
  void Init();

  // Shows infolsit window.
  void Show();

  // Shows infolist window after |milliseconds| milli secs. This function can be
  // called in the DelayShow/DelayHide delay duration. In such case, previous
  // operation will be cancelled.
  void DelayShow(unsigned int milliseconds);

  // Hides infolist window.
  void Hide();

  // Hides infolist window after |milliseconds| milli secs. This function can be
  // called in the DelayShow/DelayHide delay duration. In such case, previous
  // operation will be cancelled.
  void DelayHide(unsigned int milliseconds);

  // Updates infolist contents with |entries| and |focused_index|. If you want
  // unselect all entry, pass |focused_index| as -1.
  void UpdateCandidates(const std::vector<Entry>& entries,
                        size_t focused_index);

  // Resizes to preferred size and moves to appropriate position.
  void ResizeAndMoveParentFrame();

 protected:
  // Override View::VisibilityChanged()
  virtual void VisibilityChanged(View* starting_from, bool is_visible) OVERRIDE;

  // Override View::OnBoundsChanged()
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

 private:
  // Called by |show_hide_timer_|
  void OnShowHideTimer();

  // The parent frame.
  views::Widget* parent_frame_;

  // The candidate window frame.
  views::Widget* candidate_window_frame_;

  // The infolist area is where the meanings and the usages of the words are
  // rendered.
  views::View* infolist_area_;
  // The infolist views are used for rendering the meanings and the usages of
  // the words.
  std::vector<InfolistView*> infolist_views_;

  // True if infolist window is visible.
  bool visible_;

  base::OneShotTimer<InfolistWindowView> show_hide_timer_;

  // Information title font
  scoped_ptr<gfx::Font> title_font_;

  // Information description font
  scoped_ptr<gfx::Font> description_font_;

  DISALLOW_COPY_AND_ASSIGN(InfolistWindowView);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INFOLIST_WINDOW_VIEW_H_
