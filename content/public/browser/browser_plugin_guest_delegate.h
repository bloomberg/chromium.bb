// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_

#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"

namespace content {

struct NativeWebKeyboardEvent;

// Objects implement this interface to get notified about changes in the guest
// WebContents and to provide necessary functionality.
class CONTENT_EXPORT BrowserPluginGuestDelegate {
 public:
  virtual ~BrowserPluginGuestDelegate() {}

  // Add a message to the console.
  virtual void AddMessageToConsole(int32 level,
                                   const string16& message,
                                   int32 line_no,
                                   const string16& source_id) {}

  // Request the delegate to close this guest, and do whatever cleanup it needs
  // to do.
  virtual void Close() {}

  // Informs the delegate that the guest render process is gone. |status|
  // indicates whether the guest was killed, crashed, or was terminated
  // gracefully.
  virtual void GuestProcessGone(base::TerminationStatus status) {}

  virtual bool HandleKeyboardEvent(const NativeWebKeyboardEvent& event);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
