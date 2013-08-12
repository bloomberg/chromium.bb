// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/common/browser_plugin_permission_type.h"

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

  // Notification that the guest is no longer hung.
  virtual void RendererResponsive() {}

  // Notification that the guest is hung.
  virtual void RendererUnresponsive() {}

  typedef base::Callback<void(bool /* allow */,
                              const std::string& /* user_input */)>
      PermissionResponseCallback;

  // Request permission from the delegate to perform an action of the provided
  // |permission_type|. Details of the permission request are found in
  // |request_info|. A |callback| is provided to make the decision.
  // Returns whether the delegate has, or will handle the permission request.
  virtual bool RequestPermission(
      BrowserPluginPermissionType permission_type,
      const base::DictionaryValue& request_info,
      const PermissionResponseCallback& callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
