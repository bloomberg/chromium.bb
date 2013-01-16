// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AURA_STACKING_CLIENT_AURA_H_
#define CHROME_BROWSER_UI_AURA_STACKING_CLIENT_AURA_H_

#include "ui/aura/client/stacking_client.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/host_desktop.h"

namespace ash {
class StackingController;
}

namespace views {
class DesktopStackingClient;
class Window;
}

// A stacking client for the two worlds aura, dispatches to either a
// DesktopStackingClient or an ash::StackingController.
class StackingClientAura : public aura::client::StackingClient {
 public:
  StackingClientAura();
  virtual ~StackingClientAura();

  // Overridden from client::StackingClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE;

 private:
  scoped_ptr<views::DesktopStackingClient> desktop_stacking_client_;

  DISALLOW_COPY_AND_ASSIGN(StackingClientAura);
};

#endif  // CHROME_BROWSER_UI_AURA_STACKING_CLIENT_AURA_H_
