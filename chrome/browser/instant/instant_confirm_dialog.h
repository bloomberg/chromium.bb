// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_CONFIRM_DIALOG_H_
#define CHROME_BROWSER_INSTANT_INSTANT_CONFIRM_DIALOG_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

class GURL;
class Profile;

namespace browser {

// URL for learning more about instant.
GURL InstantLearnMoreURL();

// Invoked from the opt-in and preferences when the user toggles instant. If the
// instant confirm dialog hasn't been shown, it's shown. If the instant dialog
// has already been shown the dialog is not shown but the preference is toggled.
void ShowInstantConfirmDialogIfNecessary(gfx::NativeWindow parent,
                                         Profile* profile);

// Shows the platform specific dialog to confirm if the user really wants to
// enable instant. If the user accepts the dialog invoke
// InstantController::Enable.
void ShowInstantConfirmDialog(gfx::NativeWindow parent,
                              Profile* profile);

}  // namespace browser

#endif  // CHROME_BROWSER_INSTANT_INSTANT_CONFIRM_DIALOG_H_
