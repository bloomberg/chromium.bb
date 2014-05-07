// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_DELEGATE_H_

#include "base/callback.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

class SiteInstance;
class WebContents;

// A BrowserPluginGuestManagerDelegate offloads guest management and routing
// operations outside of the content layer.
class CONTENT_EXPORT BrowserPluginGuestManagerDelegate {
 public:
  virtual ~BrowserPluginGuestManagerDelegate() {}

  // Return a new instance ID.
  // TODO(fsamuel): Remove this. Once the instance ID concept is moved
  // entirely out of content and into chrome, this API will be unnecessary.
  virtual int GetNextInstanceID();

  // Adds a new WebContents |guest_web_contents| as a guest.
  // TODO(fsamuel): Remove this. Once guest WebContents allocation
  // moves outside of content, this API will be unnecessary.
  virtual void AddGuest(int guest_instance_id,
                        WebContents* guest_web_contents) {}

  // Removes a |guest_instance_id| as a valid guest.
  // TODO(fsamuel): Remove this. Once guest WebContents allocation
  // moves outside of content, this API will be unnecessary.
  virtual void RemoveGuest(int guest_instance_id) {}

  typedef base::Callback<void(WebContents*)> GuestByInstanceIDCallback;
  // Requests a guest WebContents associated with the provided
  // |guest_instance_id|. If a guest associated with the provided ID
  // does not exist, then the |callback| will be called with a NULL
  // WebContents. If the provided |embedder_render_process_id| does
  // not own the requested guest, then the embedder will be killed,
  // and the |callback| will not be called.
  virtual void MaybeGetGuestByInstanceIDOrKill(
      int guest_instance_id,
      int embedder_render_process_id,
      const GuestByInstanceIDCallback& callback) {}

  // Returns an existing SiteInstance if the current profile has a guest of the
  // given |guest_site|.
  // TODO(fsamuel): Remove this. Once guest WebContents allocation
  // moves outside of content, this API will be unnecessary.
  virtual content::SiteInstance* GetGuestSiteInstance(const GURL& guest_site);

  // Iterates over all WebContents belonging to a given |embedder_web_contents|,
  // calling |callback| for each. If one of the callbacks returns true, then
  // the iteration exits early.
  typedef base::Callback<bool(WebContents*)> GuestCallback;
  virtual bool ForEachGuest(WebContents* embedder_web_contents,
                            const GuestCallback& callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_DELEGATE_H_
