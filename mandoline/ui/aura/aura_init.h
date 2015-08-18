// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_AURA_AURA_INIT_H_
#define MANDOLINE_UI_AURA_AURA_INIT_H_

#include "ui/mojo/init/ui_init.h"

namespace mojo {
class Shell;
class View;
}

namespace mandoline {

// Sets up necessary state for aura when run with the viewmanager.
class AuraInit {
 public:
  AuraInit(mojo::View* root, mojo::Shell* shell);
  ~AuraInit();

 private:
  void InitializeResources(mojo::Shell* shell);

  ui::mojo::UIInit ui_init_;

  DISALLOW_COPY_AND_ASSIGN(AuraInit);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_AURA_AURA_INIT_H_
