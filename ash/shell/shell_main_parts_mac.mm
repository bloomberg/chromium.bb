// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/shell_main_parts.h"

#import <Cocoa/Cocoa.h>

#include "base/i18n/icu_util.h"
#include "base/mac/bundle_locations.h"
#include "base/memory/scoped_nsobject.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace ash {
namespace shell {

void PreMainMessageLoopStart() {
  ui::RegisterPathProvider();
  icu_util::Initialize();
  ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);

  scoped_nsobject<NSNib>
      nib([[NSNib alloc] initWithNibNamed:@"MainMenu"
                                   bundle:base::mac::FrameworkBundle()]);
  [nib instantiateNibWithOwner:NSApp topLevelObjects:nil];
}

}  // namespace shell
}  // namespace ash
