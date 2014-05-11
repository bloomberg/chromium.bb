// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/mic_search_decoration.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

MicSearchDecoration::MicSearchDecoration(CommandUpdater* command_updater)
    : command_updater_(command_updater) {
  SetImage(OmniboxViewMac::ImageForResource(IDR_OMNIBOX_MIC_SEARCH));
}

MicSearchDecoration::~MicSearchDecoration() {
}

bool MicSearchDecoration::AcceptsMousePress() {
  return true;
}

bool MicSearchDecoration::OnMousePressed(NSRect frame, NSPoint location) {
  command_updater_->ExecuteCommand(IDC_TOGGLE_SPEECH_INPUT);
  return true;
}

NSString* MicSearchDecoration::GetToolTip() {
  return l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_MIC_SEARCH);
}
