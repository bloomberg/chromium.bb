// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOCK_WINDOW_AURA_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOCK_WINDOW_AURA_H_

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/ui/lock_window.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace chromeos {

class LockWindowAura : public views::Widget,
                       public views::WidgetDelegate,
                       public LockWindow {
 public:
  // LockWindow implementation:
  virtual void Grab() override;
  virtual views::Widget* GetWidget() override;

  // views::WidgetDelegate implementation:
  virtual views::View* GetInitiallyFocusedView() override;
  virtual const views::Widget* GetWidget() const override;

 private:
  friend class LockWindow;

  LockWindowAura();
  virtual ~LockWindowAura();

  // Initialize the Aura lock window.
  void Init();

#if defined(USE_ATHENA)
  scoped_ptr<aura::Window> lock_screen_container_;
#endif

  DISALLOW_COPY_AND_ASSIGN(LockWindowAura);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOCK_WINDOW_AURA_H_
