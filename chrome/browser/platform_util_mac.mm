// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util.h"
#include "app/l10n_util_mac.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"

namespace platform_util {

void ShowItemInFolder(const FilePath& full_path) {
  DCHECK_EQ([NSThread currentThread], [NSThread mainThread]);
  NSString* path_string = base::SysUTF8ToNSString(full_path.value());
  if (!path_string || ![[NSWorkspace sharedWorkspace] selectFile:path_string
                                        inFileViewerRootedAtPath:nil])
    LOG(WARNING) << "NSWorkspace failed to select file " << full_path.value();
}

void OpenItem(const FilePath& full_path) {
  DCHECK_EQ([NSThread currentThread], [NSThread mainThread]);
  NSString* path_string = base::SysUTF8ToNSString(full_path.value());
  if (!path_string || ![[NSWorkspace sharedWorkspace] openFile:path_string])
    LOG(WARNING) << "NSWorkspace failed to open file " << full_path.value();
}

void OpenExternal(const GURL& url) {
  DCHECK_EQ([NSThread currentThread], [NSThread mainThread]);
  NSString* url_string = base::SysUTF8ToNSString(url.spec());
  NSURL* ns_url = [NSURL URLWithString:url_string];
  if (!ns_url || ![[NSWorkspace sharedWorkspace] openURL:ns_url])
    LOG(WARNING) << "NSWorkspace failed to open URL " << url;
}

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  return [view window];
}

bool IsWindowActive(gfx::NativeWindow window) {
  return [window isKeyWindow] || [window isMainWindow];
}

void ActivateWindow(gfx::NativeWindow window) {
  [window makeKeyAndOrderFront:nil];
}

bool IsVisible(gfx::NativeView view) {
  // A reasonable approximation of how you'd expect this to behave.
  return (view &&
          ![view isHiddenOrHasHiddenAncestor] &&
          [view window] &&
          [[view window] isVisible]);
}

void SimpleErrorBox(gfx::NativeWindow parent,
                    const string16& title,
                    const string16& message) {
  // Ignore the title; it's the window title on other platforms and ignorable.
  NSAlert* alert = [[[NSAlert alloc] init] autorelease];
  [alert addButtonWithTitle:l10n_util::GetNSString(IDS_OK)];
  [alert setMessageText:base::SysUTF16ToNSString(message)];
  [alert setAlertStyle:NSWarningAlertStyle];
  [alert runModal];
}

bool SimpleYesNoBox(gfx::NativeWindow parent,
                    const string16& title,
                    const string16& message) {
  // Ignore the title; it's the window title on other platforms and ignorable.
  NSAlert* alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:base::SysUTF16ToNSString(message)];
  [alert setAlertStyle:NSWarningAlertStyle];

  [alert addButtonWithTitle:
      l10n_util::GetNSString(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL)];
  [alert addButtonWithTitle:
      l10n_util::GetNSString(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL)];

  NSInteger result = [alert runModal];
  return result == NSAlertFirstButtonReturn;
}

string16 GetVersionStringModifier() {
#if defined(GOOGLE_CHROME_BUILD)
  // Use the main application bundle and not the framework bundle. Keystone
  // keys don't live in the framework.
  NSBundle* bundle = [NSBundle mainBundle];
  NSString* channel = [bundle objectForInfoDictionaryKey:@"KSChannelID"];

  // Only ever return "", "unknown", "beta" or "dev" in a branded build.
  if (![bundle objectForInfoDictionaryKey:@"KSProductID"]) {
    // This build is not Keystone-enabled, it can't have a channel.
    channel = @"unknown";
  } else if (!channel) {
    // For the stable channel, KSChannelID is not set.
    channel = @"";
  } else if ([channel isEqual:@"beta"] || [channel isEqual:@"dev"]) {
    // do nothing.
  } else {
    channel = @"unknown";
  }

  return base::SysNSStringToUTF16(channel);
#else
  return string16();
#endif
}

}  // namespace platform_util
