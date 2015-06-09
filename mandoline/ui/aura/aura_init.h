// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_AURA_AURA_INIT_H_
#define MANDOLINE_UI_AURA_AURA_INIT_H_

#include "base/memory/scoped_ptr.h"

namespace mojo {
class Shell;
}

namespace mandoline {

class ScreenMojo;

// Sets up necessary state for aura when run with the viewmanager.
class AuraInit {
 public:
  explicit AuraInit(mojo::Shell* shell);
  ~AuraInit();

 private:
  void InitializeResources(mojo::Shell* shell);

  scoped_ptr<ScreenMojo> screen_;

  DISALLOW_COPY_AND_ASSIGN(AuraInit);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_AURA_AURA_INIT_H_
