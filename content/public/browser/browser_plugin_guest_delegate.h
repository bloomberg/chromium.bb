// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/process/kill.h"
#include "content/common/content_export.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace content {

class WebContents;

// Objects implement this interface to get notified about changes in the guest
// WebContents and to provide necessary functionality.
class CONTENT_EXPORT BrowserPluginGuestDelegate {
 public:
  virtual ~BrowserPluginGuestDelegate() {}

  // Notification that the embedder has completed attachment.
  virtual void DidAttach() {}

  // Informs the delegate that the embedder has been destroyed.
  virtual void EmbedderDestroyed() {}

  // Requests setting the zoom level to the provided |zoom_level|.
  virtual void SetZoom(double zoom_factor) {}

  virtual bool IsDragAndDropEnabled();

  // Notifies that the content size of the guest has changed in autosize mode.
  virtual void SizeChanged(const gfx::Size& old_size,
                           const gfx::Size& new_size) {}

  // Asks the delegate if the given guest can lock the pointer.
  // Invoking the |callback| synchronously is OK.
  virtual void RequestPointerLockPermission(
      bool user_gesture,
      bool last_unlocked_by_target,
      const base::Callback<void(bool)>& callback) {}

  // Request navigating the guest to the provided |src| URL.
  virtual void NavigateGuest(const std::string& src) {}

  // Requests that the delegate destroy itself along with its associated
  // WebContents.
  virtual void Destroy() {}

  // Registers a |callback| with the delegate that the delegate would call when
  // it is about to be destroyed.
  typedef base::Callback<void(WebContents*)> DestructionCallback;
  virtual void RegisterDestructionCallback(
      const DestructionCallback& callback) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
