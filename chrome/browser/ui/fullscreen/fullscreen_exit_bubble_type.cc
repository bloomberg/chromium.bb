// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fullscreen/fullscreen_exit_bubble_type.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/base/l10n/l10n_util.h"

namespace fullscreen_bubble {

base::string16 GetLabelTextForType(FullscreenExitBubbleType type,
                                   const GURL& url,
                                   ExtensionService* extension_service) {
  base::string16 host(base::UTF8ToUTF16(url.host()));
  if (extension_service) {
    const extensions::ExtensionSet* extensions =
        extension_service->extensions();
    DCHECK(extensions);
    const extensions::Extension* extension =
        extensions->GetExtensionOrAppByURL(url);
    if (extension) {
      host = base::UTF8ToUTF16(extension->name());
    } else if (url.SchemeIs(extensions::kExtensionScheme)) {
      // In this case, |host| is set to an extension ID.
      // We are not going to show it because it's human-unreadable.
      host.clear();
    }
  }
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
      case FEB_TYPE_BROWSER_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION:
        return l10n_util::GetStringUTF16(
            IDS_FULLSCREEN_UNKNOWN_EXTENSION_TRIGGERED_FULLSCREEN);
      default:
        NOTREACHED();
        return base::string16();
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
    case FEB_TYPE_BROWSER_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION:
      return l10n_util::GetStringFUTF16(
          IDS_FULLSCREEN_EXTENSION_TRIGGERED_FULLSCREEN, host);
    default:
      NOTREACHED();
      return base::string16();
  }
}

base::string16 GetDenyButtonTextForType(FullscreenExitBubbleType type) {
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
    case FEB_TYPE_BROWSER_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION:
      NOTREACHED();  // No button in this case.
      return base::string16();
    default:
      NOTREACHED();
      return base::string16();
  }
}

bool ShowButtonsForType(FullscreenExitBubbleType type) {
  return type == FEB_TYPE_FULLSCREEN_BUTTONS ||
      type == FEB_TYPE_FULLSCREEN_MOUSELOCK_BUTTONS ||
      type == FEB_TYPE_MOUSELOCK_BUTTONS;
}

void PermissionRequestedByType(FullscreenExitBubbleType type,
                               bool* tab_fullscreen,
                               bool* mouse_lock) {
  if (tab_fullscreen) {
    *tab_fullscreen = type == FEB_TYPE_FULLSCREEN_BUTTONS ||
        type == FEB_TYPE_FULLSCREEN_MOUSELOCK_BUTTONS;
  }
  if (mouse_lock) {
    *mouse_lock = type == FEB_TYPE_FULLSCREEN_MOUSELOCK_BUTTONS ||
        type == FEB_TYPE_MOUSELOCK_BUTTONS;
  }
}

}  // namespace fullscreen_bubble
