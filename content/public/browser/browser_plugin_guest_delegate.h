// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/process/kill.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace gfx {
class Size;
}  // namespace gfx

namespace content {

// Objects implement this interface to get notified about changes in the guest
// WebContents and to provide necessary functionality.
class CONTENT_EXPORT BrowserPluginGuestDelegate {
 public:
  virtual ~BrowserPluginGuestDelegate() {}

  // Notification that the embedder will begin attachment. This is called
  // prior to resuming resource loads.
  virtual void WillAttach(content::WebContents* embedder_web_contents,
                          const base::DictionaryValue& extra_params) {}

  virtual WebContents* CreateNewGuestWindow(
      const WebContents::CreateParams& create_params);

  // Notification that the embedder has completed attachment.
  virtual void DidAttach() {}

  // Requests the instance ID associated with the delegate.
  virtual int GetGuestInstanceID() const;

  // Notification that the BrowserPlugin has resized.
  virtual void ElementSizeChanged(const gfx::Size& old_size,
                                  const gfx::Size& new_size) {}

  // Notifies that the content size of the guest has changed.
  // Note: In autosize mode, it si possible that the guest size may not match
  // the element size.
  virtual void GuestSizeChanged(const gfx::Size& old_size,
                                const gfx::Size& new_size) {}

  // Asks the delegate if the given guest can lock the pointer.
  // Invoking the |callback| synchronously is OK.
  virtual void RequestPointerLockPermission(
      bool user_gesture,
      bool last_unlocked_by_target,
      const base::Callback<void(bool)>& callback) {}

  // Requests that the delegate destroy itself along with its associated
  // WebContents.
  virtual void Destroy() {}

  // Registers a |callback| with the delegate that the delegate would call when
  // it is about to be destroyed.
  typedef base::Callback<void()> DestructionCallback;
  virtual void RegisterDestructionCallback(
      const DestructionCallback& callback) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
