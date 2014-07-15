// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_NATIVE_WEB_CONTENTS_MODAL_DIALOG_H_
#define COMPONENTS_WEB_MODAL_NATIVE_WEB_CONTENTS_MODAL_DIALOG_H_

#include "ui/gfx/native_widget_types.h"

namespace web_modal {

// TODO(gbillock): rename this file

#if defined(OS_MACOSX)
// Use a void* since none of the gfx::Native* types are suitable for
// representing the web contents modal dialog under Cocoa.
typedef void* NativeWebContentsModalDialog;
#else
typedef gfx::NativeView NativeWebContentsModalDialog;
#endif

#if defined(OS_MACOSX)
// Use a void* since none of the gfx::Native* types are suitable for
// representing a popup window under Cocoa.
typedef void* NativePopup;
#else
typedef gfx::NativeView NativePopup;
#endif

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_NATIVE_WEB_CONTENTS_MODAL_DIALOG_H_
