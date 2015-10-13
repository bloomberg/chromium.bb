// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#include <CoreServices/CoreServices.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#import "base/mac/mac_util.h"
#include "base/mac/scoped_aedesc.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/platform_util_internal.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace platform_util {

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  DCHECK([NSThread isMainThread]);
  NSString* path_string = base::SysUTF8ToNSString(full_path.value());
  if (!path_string || ![[NSWorkspace sharedWorkspace] selectFile:path_string
                                        inFileViewerRootedAtPath:@""])
    LOG(WARNING) << "NSWorkspace failed to select file " << full_path.value();
}

void OpenFileOnMainThread(const base::FilePath& full_path) {
  DCHECK([NSThread isMainThread]);
  NSString* path_string = base::SysUTF8ToNSString(full_path.value());
  if (!path_string)
    return;

  // On Mavericks or later, NSWorkspaceLaunchWithErrorPresentation will
  // properly handle Finder activation for quarantined files
  // (http://crbug.com/32921) and unassociated file types
  // (http://crbug.com/50263).
  if (base::mac::IsOSMavericksOrLater()) {
    NSURL* url = [NSURL fileURLWithPath:path_string];
    if (!url)
      return;

    const NSWorkspaceLaunchOptions launch_options =
        NSWorkspaceLaunchAsync | NSWorkspaceLaunchWithErrorPresentation;
    [[NSWorkspace sharedWorkspace] openURLs:@[ url ]
                    withAppBundleIdentifier:nil
                                    options:launch_options
             additionalEventParamDescriptor:nil
                          launchIdentifiers:NULL];
    return;
  }

  // On older OSes, both LaunchServices and NSWorkspace will fail silently for
  // the two cases described above. On those platforms, use an AppleEvent to
  // instruct the Finder to open the file.

  // Create the target of this AppleEvent, the Finder.
  base::mac::ScopedAEDesc<AEAddressDesc> address;
  const OSType finderCreatorCode = 'MACS';
  OSErr status = AECreateDesc(typeApplSignature,  // type
                              &finderCreatorCode,  // data
                              sizeof(finderCreatorCode),  // dataSize
                              address.OutPointer());  // result
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status) << "Could not create OpenFile() AE target";
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
    OSSTATUS_LOG(WARNING, status) << "Could not create OpenFile() AE event";
    return;
  }

  // Create the list of files (only ever one) to open.
  base::mac::ScopedAEDesc<AEDescList> fileList;
  status = AECreateList(NULL,  // factoringPtr
                        0,  // factoredSize
                        false,  // isRecord
                        fileList.OutPointer());  // resultList
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status) << "Could not create OpenFile() AE file list";
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
      OSSTATUS_LOG(WARNING, status)
          << "Could not add file path to AE list in OpenFile()";
      return;
    }
  } else {
    LOG(WARNING) << "Could not get FSRef for path URL in OpenFile()";
    return;
  }

  // Attach the file list to the AppleEvent.
  status = AEPutParamDesc(theEvent.OutPointer(),  // theAppleEvent
                          keyDirectObject,  // theAEKeyword
                          fileList);  // theAEDesc
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status)
        << "Could not put the AE file list the path in OpenFile()";
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
    OSSTATUS_LOG(WARNING, status)
        << "Could not send AE to Finder in OpenFile()";
  }
}

namespace internal {

void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type) {
  switch (type) {
    case OPEN_FILE:
      content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                       base::Bind(&OpenFileOnMainThread, path));
      return;
    case OPEN_FOLDER:
      NSString* path_string = base::SysUTF8ToNSString(path.value());
      if (!path_string)
        return;
      // Note that there exists a TOCTOU race between the time that |path| was
      // verified as being a directory and when NSWorkspace invokes Finder (or
      // alternative) to open |path_string|.
      [[NSWorkspace sharedWorkspace] openFile:path_string];
      return;
  }
}

}  // namespace internal

void OpenExternal(Profile* profile, const GURL& url) {
  DCHECK([NSThread isMainThread]);
  NSString* url_string = base::SysUTF8ToNSString(url.spec());
  NSURL* ns_url = [NSURL URLWithString:url_string];
  if (!ns_url || ![[NSWorkspace sharedWorkspace] openURL:ns_url])
    LOG(WARNING) << "NSWorkspace failed to open URL " << url;
}

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  return [view window];
}

gfx::NativeView GetViewForWindow(gfx::NativeWindow window) {
  DCHECK(window);
  DCHECK([window contentView]);
  return [window contentView];
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

bool IsSwipeTrackingFromScrollEventsEnabled() {
  SEL selector = @selector(isSwipeTrackingFromScrollEventsEnabled);
  return [NSEvent respondsToSelector:selector]
      && [NSEvent performSelector:selector];
}

}  // namespace platform_util
