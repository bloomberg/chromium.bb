// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/platform_util.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util.h"
#include "app/l10n_util_mac.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/cocoa/tab_window_controller.h"
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

string16 GetWindowTitle(gfx::NativeWindow window) {
  NSString* title = nil;
  if ([[window delegate] isKindOfClass:[TabWindowController class]]) {
    TabWindowController* delegate =
        reinterpret_cast<TabWindowController*>([window delegate]);
    title = [delegate selectedTabTitle];
  } else {
    title = [window title];
  }
  // If we don't yet have a title, use "Untitled".
  if (![title length])
    return WideToUTF16(l10n_util::GetString(
        IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED));

  return base::SysNSStringToUTF16(title);
}

bool IsWindowActive(gfx::NativeWindow window) {
  return [window isKeyWindow] || [window isMainWindow];
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
  NSAlert* alert = [[[NSAlert alloc] init] autorelease];
  [alert addButtonWithTitle:l10n_util::GetNSString(IDS_OK)];
  [alert setMessageText:base::SysUTF16ToNSString(title)];
  [alert setInformativeText:base::SysUTF16ToNSString(message)];
  [alert setAlertStyle:NSWarningAlertStyle];
  [alert runModal];
}

string16 GetVersionStringModifier() {
#if defined(GOOGLE_CHROME_BUILD)
  NSBundle* bundle = mac_util::MainAppBundle();
  NSString* channel = [bundle objectForInfoDictionaryKey:@"KSChannelID"];
  if (!channel)
    channel = @"stable";
  return base::SysNSStringToUTF16(channel);
#else
  return string16();
#endif
}

}  // namespace platform_util
