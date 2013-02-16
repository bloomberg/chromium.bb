// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_NATIVE_WEB_CONTENTS_MODAL_DIALOG_H_
#define CHROME_BROWSER_UI_NATIVE_WEB_CONTENTS_MODAL_DIALOG_H_

#include "ui/gfx/native_widget_types.h"

#if defined(OS_MACOSX)
// Use a void* since none of the gfx::Native* types are suitable for
// representing the web contents modal dialog under Cocoa.
typedef void* NativeWebContentsModalDialog;
#else
typedef gfx::NativeView NativeWebContentsModalDialog;
#endif

#endif  // CHROME_BROWSER_UI_NATIVE_WEB_CONTENTS_MODAL_DIALOG_H_
