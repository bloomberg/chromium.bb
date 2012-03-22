// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_DIALOG_H_
#define CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_DIALOG_H_
#pragma once

#include "base/callback_forward.h"
#include "ui/gfx/native_widget_types.h"

typedef base::Callback<void(bool /* use_default_settings */)>
    OneClickAcceptCallback;

// Shows a dialog with more information about one click signin and
// waits for confirmation from the user.  |parent_window| is the
// window from which the dialog was invoked.  If the user confirms,
// |accept_callback| is called.  The implementation of this function
// is platform specific.
void ShowOneClickSigninDialog(
    gfx::NativeWindow parent_window,
    const OneClickAcceptCallback& accept_callback);

#endif  // CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_DIALOG_H_
