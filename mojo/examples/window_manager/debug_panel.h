// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_WINDOW_MANAGER_DEBUG_PANEL_H_
#define MOJO_EXAMPLES_WINDOW_MANAGER_DEBUG_PANEL_H_

#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class Label;
class RadioButton;
}

namespace mojo {

namespace view_manager {
class Node;
}

namespace examples {

// A panel of controls intended to demonstrate the functionality of the window
// manager.
class DebugPanel : public views::LayoutManager {
 public:
  DebugPanel(view_manager::Node* node);
  virtual ~DebugPanel();

  navigation::Target navigation_target() const;

 private:
  // LayoutManager overrides:
  virtual gfx::Size GetPreferredSize(const views::View* view) const OVERRIDE;
  virtual void Layout(views::View* host) OVERRIDE;

  views::Label* navigation_target_label_;
  views::RadioButton* navigation_target_new_;
  views::RadioButton* navigation_target_source_;
  views::RadioButton* navigation_target_default_;

  DISALLOW_COPY_AND_ASSIGN(DebugPanel);
};

}  // examples
}  // mojo

#endif  //  MOJO_EXAMPLES_WINDOW_MANAGER_DEBUG_PANEL_H_
