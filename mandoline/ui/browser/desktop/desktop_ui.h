// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_BROWSER_DESKTOP_DESKTOP_UI_H_
#define MANDOLINE_UI_BROWSER_DESKTOP_DESKTOP_UI_H_

#include "mandoline/ui/aura/aura_init.h"
#include "mandoline/ui/browser/browser_ui.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/layout_manager.h"

namespace mojo {
class Shell;
class View;
}

namespace mandoline {

class Browser;

class DesktopUI : public BrowserUI,
                  public views::LayoutManager,
                  public views::TextfieldController {
 public:
  DesktopUI(Browser* browser, mojo::Shell* shell);
  ~DesktopUI() override;

 private:
  // Overridden from BrowserUI
  void Init(mojo::View* root, mojo::View* content) override;

  // Overridden from views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* view) const override;
  void Layout(views::View* host) override;

  // Overridden from views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  AuraInit aura_init_;
  Browser* browser_;
  mojo::Shell* shell_;
  views::Textfield* omnibox_;
  mojo::View* root_;
  mojo::View* content_;

  DISALLOW_COPY_AND_ASSIGN(DesktopUI);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_BROWSER_DESKTOP_DESKTOP_UI_H_
