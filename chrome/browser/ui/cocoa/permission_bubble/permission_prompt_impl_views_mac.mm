// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/permission_bubble/permission_bubble_cocoa.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_impl.h"
#include "ui/base/material_design/material_design_controller.h"

namespace {

constexpr base::Feature kCocoaPermissionBubbles = {
    "CocoaPermissionBubbles", base::FEATURE_DISABLED_BY_DEFAULT};

bool UseViewsBubbles() {
  return ui::MaterialDesignController::IsSecondaryUiMaterial() ||
         !base::FeatureList::IsEnabled(kCocoaPermissionBubbles);
}

}  // namespace

// static
std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (UseViewsBubbles())
    return base::MakeUnique<PermissionPromptImpl>(browser, delegate);
  return base::MakeUnique<PermissionBubbleCocoa>(browser, delegate);
}
