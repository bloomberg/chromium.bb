// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INFOLIST_WINDOW_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INFOLIST_WINDOW_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/view.h"
#include "third_party/mozc/session/candidates_lite.pb.h"
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
  InfolistWindowView(views::Widget* parent_frame,
                     views::Widget* candidate_window_frame);
  virtual ~InfolistWindowView();
  void Init();
  void Show();
  void DelayShow(unsigned int milliseconds);
  void Hide();
  void DelayHide(unsigned int milliseconds);
  void UpdateCandidates(const InputMethodLookupTable& lookup_table);
  void ResizeAndMoveParentFrame();
  gfx::Font GetTitleFont() const;
  gfx::Font GetDescriptionFont() const;

 protected:
  // Override View::VisibilityChanged()
  virtual void VisibilityChanged(View* starting_from, bool is_visible) OVERRIDE;

  // Override View::OnBoundsChanged()
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(InfolistWindowViewTest, ShouldUpdateViewTest);
  // Called by show_hide_timer_
  void OnShowHideTimer();

  // Information list
  scoped_ptr<mozc::commands::InformationList> usages_;

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

  bool visible_;

  base::OneShotTimer<InfolistWindowView> show_hide_timer_;

  static bool ShouldUpdateView(
    const mozc::commands::InformationList* old_usages,
    const mozc::commands::InformationList* new_usages);

  // Information title font
  scoped_ptr<gfx::Font> title_font_;
  // Information description font
  scoped_ptr<gfx::Font> description_font_;

  DISALLOW_COPY_AND_ASSIGN(InfolistWindowView);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INFOLIST_WINDOW_VIEW_H_
