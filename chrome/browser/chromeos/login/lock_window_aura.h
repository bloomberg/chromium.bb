// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WINDOW_AURA_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WINDOW_AURA_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/lock_window.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

class LockWindowAura : public views::Widget,
                       public LockWindow {
 public:
  // LockWindow implementation:
  virtual void Grab(DOMView* dom_view) OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;

 private:
  friend class LockWindow;

  LockWindowAura();
  virtual ~LockWindowAura();

  // Initialize the Aura lock window.
  void Init();

  DISALLOW_COPY_AND_ASSIGN(LockWindowAura);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WINDOW_AURA_H_
