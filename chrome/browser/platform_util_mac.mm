// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac/scoped_aedesc.h"
#include "base/sys_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace platform_util {

void ShowItemInFolder(const FilePath& full_path) {
  DCHECK_EQ([NSThread currentThread], [NSThread mainThread]);
  NSString* path_string = base::SysUTF8ToNSString(full_path.value());
  if (!path_string || ![[NSWorkspace sharedWorkspace] selectFile:path_string
                                        inFileViewerRootedAtPath:nil])
    LOG(WARNING) << "NSWorkspace failed to select file " << full_path.value();
}

// This function opens a file.  This doesn't use LaunchServices or NSWorkspace
// because of two bugs:
//  1. Incorrect app activation with com.apple.quarantine:
//     http://crbug.com/32921
//  2. Silent no-op for unassociated file types: http://crbug.com/50263
// Instead, an AppleEvent is constructed to tell the Finder to open the
// document.
void OpenItem(const FilePath& full_path) {
  DCHECK_EQ([NSThread currentThread], [NSThread mainThread]);
  NSString* path_string = base::SysUTF8ToNSString(full_path.value());
  if (!path_string)
    return;

  OSErr status;

  // Create the target of this AppleEvent, the Finder.
  base::mac::ScopedAEDesc<AEAddressDesc> address;
  const OSType finderCreatorCode = 'MACS';
  status = AECreateDesc(typeApplSignature,  // type
                        &finderCreatorCode,  // data
                        sizeof(finderCreatorCode),  // dataSize
                        address.OutPointer());  // result
  if (status != noErr) {
    LOG(WARNING) << "Could not create OpenItem() AE target";
    return;
  }

  // Build the AppleEvent data structure that instructs Finder to open files.
  base::mac::ScopedAEDesc<AppleEvent> theEvent;
  status = AECreateAppleEvent(kCoreEventClass,  // theAEEventClass
                              kAEOpenDocuments,  // theAEEventID
                              address,  // target
                              kAutoGenerateReturnID,  // returnID
                              kAnyTransactionID,  // transactionID
                              theEvent.OutPointer());  // result
  if (status != noErr) {
    LOG(WARNING) << "Could not create OpenItem() AE event";
    return;
  }

  // Create the list of files (only ever one) to open.
  base::mac::ScopedAEDesc<AEDescList> fileList;
  status = AECreateList(NULL,  // factoringPtr
                        0,  // factoredSize
                        false,  // isRecord
                        fileList.OutPointer());  // resultList
  if (status != noErr) {
    LOG(WARNING) << "Could not create OpenItem() AE file list";
    return;
  }

  // Add the single path to the file list.  C-style cast to avoid both a
  // static_cast and a const_cast to get across the toll-free bridge.
  CFURLRef pathURLRef = (CFURLRef)[NSURL fileURLWithPath:path_string];
  FSRef pathRef;
  if (CFURLGetFSRef(pathURLRef, &pathRef)) {
    status = AEPutPtr(fileList.OutPointer(),  // theAEDescList
                      0,  // index
                      typeFSRef,  // typeCode
                      &pathRef,  // dataPtr
                      sizeof(pathRef));  // dataSize
    if (status != noErr) {
      LOG(WARNING) << "Could not add file path to AE list in OpenItem()";
      return;
    }
  } else {
    LOG(WARNING) << "Could not get FSRef for path URL in OpenItem()";
    return;
  }

  // Attach the file list to the AppleEvent.
  status = AEPutParamDesc(theEvent.OutPointer(),  // theAppleEvent
                          keyDirectObject,  // theAEKeyword
                          fileList);  // theAEDesc
  if (status != noErr) {
    LOG(WARNING) << "Could not put the AE file list the path in OpenItem()";
    return;
  }

  // Send the actual event.  Do not care about the reply.
  base::mac::ScopedAEDesc<AppleEvent> reply;
  status = AESend(theEvent,  // theAppleEvent
                  reply.OutPointer(),  // reply
                  kAENoReply + kAEAlwaysInteract,  // sendMode
                  kAENormalPriority,  // sendPriority
                  kAEDefaultTimeout,  // timeOutInTicks
                  NULL, // idleProc
                  NULL);  // filterProc
  if (status != noErr) {
    LOG(WARNING) << "Could not send AE to Finder in OpenItem()";
  }
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

gfx::NativeView GetParent(gfx::NativeView view) {
  return nil;
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

std::string GetVersionStringModifier() {
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

  return base::SysNSStringToUTF8(channel);
#else
  return std::string();
#endif
}

}  // namespace platform_util
