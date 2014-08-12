// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_WINDOW_MANAGER_DEBUG_PANEL_H_
#define MOJO_EXAMPLES_WINDOW_MANAGER_DEBUG_PANEL_H_

#include <string>

#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class Label;
class RadioButton;
}

namespace mojo {

class View;

namespace examples {

namespace {
const SkColor kColors[] = { SK_ColorYELLOW,
                            SK_ColorRED,
                            SK_ColorGREEN,
                            SK_ColorMAGENTA };
}

// A panel of controls intended to demonstrate the functionality of the window
// manager.
class DebugPanel : public views::LayoutManager, public views::ButtonListener {
 public:
  class Delegate {
   public:
    virtual void CloseTopWindow() = 0;
    virtual void RequestNavigate(
        uint32 source_view_id, Target target,
        NavigationDetailsPtr nav_details) = 0;
   protected:
    virtual ~Delegate(){}
  };

  DebugPanel(Delegate* delegate, View* view);
  virtual ~DebugPanel();

  Target navigation_target() const;

 private:
  // LayoutManager overrides:
  virtual gfx::Size GetPreferredSize(const views::View* view) const OVERRIDE;
  virtual void Layout(views::View* host) OVERRIDE;
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  void Navigate(const std::string& url);

  Delegate* delegate_;
  View* view_;

  views::Label* navigation_target_label_;
  views::RadioButton* navigation_target_new_;
  views::RadioButton* navigation_target_source_;
  views::RadioButton* navigation_target_default_;

  size_t next_color_;
  views::Button* colored_square_;
  views::Button* close_last_;
  views::Button* cross_app_;

  DISALLOW_COPY_AND_ASSIGN(DebugPanel);
};

}  // examples
}  // mojo

#endif  //  MOJO_EXAMPLES_WINDOW_MANAGER_DEBUG_PANEL_H_
