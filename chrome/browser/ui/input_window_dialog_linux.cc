// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/input_window_dialog.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "chrome/browser/ui/webui/input_window_dialog_webui.h"

#if !defined(USE_AURA)
#include "chrome/browser/ui/input_window_dialog_gtk.h"
#endif

InputWindowDialog* InputWindowDialog::Create(
    gfx::NativeWindow parent,
    const string16& window_title,
    const LabelContentsPairs& label_contents_pairs,
    Delegate* delegate,
    ButtonType type) {
#if defined(USE_AURA)
  return new InputWindowDialogWebUI(window_title,
                                    label_contents_pairs,
                                    delegate,
                                    type);
#else
  if (ChromeWebUI::IsMoreWebUI()) {
    return new InputWindowDialogWebUI(window_title,
                                      label_contents_pairs,
                                      delegate,
                                      type);
  } else {
    DCHECK_EQ(1U, label_contents_pairs.size());
    return new InputWindowDialogGtk(parent,
                                    UTF16ToUTF8(window_title),
                                    UTF16ToUTF8(label_contents_pairs[0].first),
                                    UTF16ToUTF8(label_contents_pairs[0].second),
                                    delegate,
                                    type);
  }
#endif
}
