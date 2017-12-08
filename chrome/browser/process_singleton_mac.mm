// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton.h"

#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>

#include "base/mac/scoped_aedesc.h"

namespace {

// Forwards |event| to an already-running Chromium process.
OSErr HandleGURLEvent(const AppleEvent* event,
                      AppleEvent* reply,
                      SRefCon handler_refcon) {
  // Forwarding the message without |kAENoReply| hangs us, so assume that a
  // reply is unnecessary as an invariant.
  DCHECK_EQ(reply->descriptorType, typeNull);

  pid_t forwarding_pid = *(reinterpret_cast<pid_t*>(handler_refcon));
  base::mac::ScopedAEDesc<> other_process_pid;
  // Create an address descriptor for the running process.
  AECreateDesc(typeKernelProcessID, &forwarding_pid, sizeof(forwarding_pid),
               other_process_pid.OutPointer());

  // Change the event's address to the running process.
  AppleEvent* event_copy = const_cast<AppleEvent*>(event);
  if (AEPutAttributeDesc(event_copy, keyAddressAttr, other_process_pid) ==
      noErr) {
    // Send the message back out.
    AESendMessage(event_copy, reply, kAENoReply, kNoTimeOut);
    return noErr;
  }

  return noErr;
}

}  //  namespace

bool ProcessSingleton::WaitForAndForwardOpenURLEvent(
    pid_t event_destination_pid) {
  AEEventHandlerProcPtr handler = NewAEEventHandlerUPP(HandleGURLEvent);
  if (AEInstallEventHandler(kInternetEventClass, kAEGetURL, handler,
                            &event_destination_pid, false) != noErr) {
    DisposeAEEventHandlerUPP(handler);
    return false;
  }
  bool result = false;
  const EventTypeSpec spec = {kEventClassAppleEvent, kEventAppleEvent};
  EventRef event_ref;
  if (ReceiveNextEvent(1, &spec, kEventDurationNanosecond, true, &event_ref) ==
      noErr) {
    OSStatus processed = AEProcessEvent(event_ref);
    ReleaseEvent(event_ref);
    result = processed == noErr;
  }
  AERemoveEventHandler(kInternetEventClass, kAEGetURL, handler, false);
  DisposeAEEventHandlerUPP(handler);
  return result;
}
