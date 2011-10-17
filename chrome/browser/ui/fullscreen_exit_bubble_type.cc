// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fullscreen_exit_bubble_type.h"

#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace fullscreen_bubble {

string16 GetLabelTextForType(FullscreenExitBubbleType type,
                             const GURL& url) {
  string16 message_label_text;
  string16 host(UTF8ToUTF16(url.host()));
  if (host.empty()) {
    switch (type) {
      case FEB_TYPE_FULLSCREEN_BUTTONS:
      case FEB_TYPE_FULLSCREEN_EXIT_INSTRUCTION:
        return l10n_util::GetStringUTF16(IDS_FULLSCREEN_ENTERED_FULLSCREEN);
      case FEB_TYPE_FULLSCREEN_MOUSELOCK_BUTTONS:
        return l10n_util::GetStringUTF16(
            IDS_FULLSCREEN_REQUEST_FULLSCREEN_MOUSELOCK);
      case FEB_TYPE_MOUSELOCK_BUTTONS:
        return l10n_util::GetStringUTF16(IDS_FULLSCREEN_REQUEST_MOUSELOCK);
      case FEB_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION:
        return l10n_util::GetStringUTF16(
            IDS_FULLSCREEN_ENTERED_FULLSCREEN_MOUSELOCK);
      case FEB_TYPE_MOUSELOCK_EXIT_INSTRUCTION:
        return l10n_util::GetStringUTF16(
            IDS_FULLSCREEN_ENTERED_MOUSELOCK);
      case FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION:
        return l10n_util::GetStringUTF16(
            IDS_FULLSCREEN_USER_ENTERED_FULLSCREEN);
      default:
        NOTREACHED();
        return string16();
    }
  }
  switch (type) {
    case FEB_TYPE_FULLSCREEN_BUTTONS:
      return l10n_util::GetStringFUTF16(
          IDS_FULLSCREEN_SITE_ENTERED_FULLSCREEN, host);
    case FEB_TYPE_FULLSCREEN_MOUSELOCK_BUTTONS:
      return l10n_util::GetStringFUTF16(
            IDS_FULLSCREEN_SITE_REQUEST_FULLSCREEN_MOUSELOCK, host);
    case FEB_TYPE_MOUSELOCK_BUTTONS:
      return l10n_util::GetStringFUTF16(
            IDS_FULLSCREEN_SITE_REQUEST_MOUSELOCK, host);
    case FEB_TYPE_FULLSCREEN_EXIT_INSTRUCTION:
      return l10n_util::GetStringFUTF16(
            IDS_FULLSCREEN_SITE_ENTERED_FULLSCREEN, host);
    case FEB_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION:
      return l10n_util::GetStringFUTF16(
          IDS_FULLSCREEN_SITE_ENTERED_FULLSCREEN_MOUSELOCK, host);
    case FEB_TYPE_MOUSELOCK_EXIT_INSTRUCTION:
      return l10n_util::GetStringFUTF16(
            IDS_FULLSCREEN_SITE_ENTERED_MOUSELOCK, host);
    case FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION:
      return l10n_util::GetStringUTF16(
          IDS_FULLSCREEN_USER_ENTERED_FULLSCREEN);
    default:
      NOTREACHED();
      return string16();
  }
}

string16 GetDenyButtonTextForType(FullscreenExitBubbleType type) {
  switch (type) {
    case FEB_TYPE_FULLSCREEN_BUTTONS:
      return l10n_util::GetStringUTF16(IDS_FULLSCREEN_EXIT_FULLSCREEN);
    case FEB_TYPE_FULLSCREEN_MOUSELOCK_BUTTONS:
      return l10n_util::GetStringUTF16(IDS_FULLSCREEN_EXIT);
    case FEB_TYPE_MOUSELOCK_BUTTONS:
      return l10n_util::GetStringUTF16(IDS_FULLSCREEN_DENY);
    case FEB_TYPE_FULLSCREEN_EXIT_INSTRUCTION:
    case FEB_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION:
    case FEB_TYPE_MOUSELOCK_EXIT_INSTRUCTION:
    case FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION:
      NOTREACHED();  // No button in this case.
      return string16();
    default:
      NOTREACHED();
      return string16();
  }
}

bool ShowButtonsForType(FullscreenExitBubbleType type) {
  return type == FEB_TYPE_FULLSCREEN_BUTTONS ||
         type == FEB_TYPE_FULLSCREEN_MOUSELOCK_BUTTONS ||
         type == FEB_TYPE_MOUSELOCK_BUTTONS;
}

}  // namespace
