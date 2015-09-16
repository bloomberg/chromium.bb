// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_AURA_AURA_INIT_H_
#define MANDOLINE_UI_AURA_AURA_INIT_H_

#include "skia/ext/refptr.h"
#include "ui/mojo/init/ui_init.h"

namespace font_service {
class FontLoader;
}

namespace mojo {
class Shell;
}

namespace mus {
class View;
}

namespace mandoline {

// Sets up necessary state for aura when run with the viewmanager.
class AuraInit {
 public:
  AuraInit(mus::View* root, mojo::Shell* shell);
  ~AuraInit();

 private:
  void InitializeResources(mojo::Shell* shell);

  ui::mojo::UIInit ui_init_;

#if defined(OS_LINUX) && !defined(OS_ANDROID)
  skia::RefPtr<font_service::FontLoader> font_loader_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AuraInit);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_AURA_AURA_INIT_H_
