// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/shell_main_parts.h"

#include "base/i18n/icu_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace ash {
namespace shell {

void PreMainMessageLoopStart() {
  ui::RegisterPathProvider();
  base::i18n::InitializeICU();
  ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
}

}  // namespace ash
}  // namespace shell
