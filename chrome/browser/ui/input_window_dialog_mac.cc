// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/input_window_dialog.h"

#include "chrome/browser/ui/webui/input_window_dialog_webui.h"

InputWindowDialog* InputWindowDialog::Create(
    gfx::NativeWindow parent,
    const string16& window_title,
    const LabelContentsPairs& label_contents_pairs,
    Delegate* delegate,
    ButtonType type) {
    return new InputWindowDialogWebUI(window_title,
                                      label_contents_pairs,
                                      delegate,
                                      type);
}
