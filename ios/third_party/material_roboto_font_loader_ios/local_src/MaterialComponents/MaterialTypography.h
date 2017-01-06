// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_THIRD_PARTY_MATERIAL_ROBOTO_FONT_LOADER_IOS_LOCAL_SRC_MATERIALCOMPONENTS_MATERIALTYPOGRAPHY_H_
#define IOS_THIRD_PARTY_MATERIAL_ROBOTO_FONT_LOADER_IOS_LOCAL_SRC_MATERIALCOMPONENTS_MATERIALTYPOGRAPHY_H_

// Version v1.1.1 of material_components_ios uses a CocoaPods path to access
// MaterialTypography.h which breaks Chromium build. This file should be
// deleted once upstream code has been fixed.

// Use #import <> to force lookup in the directories listed in include_dirs
// thus allowing importing the file from material_components_ios instead of
// the current file (as using #import "" will start the file lookup in the
// current directory and will resolve to the current file, aka auto-import).

#import <MaterialTypography.h>

#endif  // IOS_THIRD_PARTY_MATERIAL_ROBOTO_FONT_LOADER_IOS_LOCAL_SRC_MATERIALCOMPONENTS_MATERIALTYPOGRAPHY_H_
