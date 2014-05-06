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

  // Returns a Webcontents given a |guest_instance_id|. Returns NULL if the
  // guest wasn't found.  If the embedder is not permitted to access the given
  // |guest_instance_id|, the embedder is killed, and NULL is returned.
  virtual WebContents* GetGuestByInstanceID(int guest_instance_id,
                                            int embedder_render_process_id);

  // Returns whether the specified embedder is permitted to access the given
  // |guest_instance_id|.
  // TODO(fsamuel): Remove this.
  virtual bool CanEmbedderAccessInstanceID(int embedder_render_process_id,
                                           int guest_instance_id);

  // Returns whether the specified embedder is permitted to access the given
  // |guest_instance_id|, and kills the embedder if not.
  // TODO(fsamuel): Remove this.
  virtual bool CanEmbedderAccessInstanceIDMaybeKill(
      int embedder_render_process_id,
      int guest_instance_id);

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
