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

InputWindowDialog* InputWindowDialog::Create(gfx::NativeWindow parent,
                                             const string16& window_title,
                                             const string16& label,
                                             const string16& contents,
                                             Delegate* delegate) {
#if defined(USE_AURA)
  return new InputWindowDialogWebUI(window_title,
                                    label,
                                    contents,
                                    delegate);
#else
  if (ChromeWebUI::IsMoreWebUI()) {
    return new InputWindowDialogWebUI(window_title,
                                      label,
                                      contents,
                                      delegate);
  } else {
    return new InputWindowDialogGtk(parent,
                                    UTF16ToUTF8(window_title),
                                    UTF16ToUTF8(label),
                                    UTF16ToUTF8(contents),
                                    delegate);
  }
#endif
}
