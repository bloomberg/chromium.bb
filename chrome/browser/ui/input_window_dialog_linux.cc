// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/input_window_dialog.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/input_window_dialog_gtk.h"

InputWindowDialog* InputWindowDialog::Create(
    gfx::NativeWindow parent,
    const string16& window_title,
    const LabelContentsPairs& label_contents_pairs,
    Delegate* delegate,
    ButtonType type) {
    return new InputWindowDialogGtk(parent,
                                    UTF16ToUTF8(window_title),
                                    UTF16ToUTF8(label_contents_pairs[0].first),
                                    UTF16ToUTF8(label_contents_pairs[0].second),
                                    delegate,
                                    type);
}
