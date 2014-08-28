// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_H_

#include "base/callback.h"
#include "content/common/content_export.h"

namespace content {

class WebContents;

// A BrowserPluginGuestManager offloads guest management and routing
// operations outside of the content layer.
class CONTENT_EXPORT BrowserPluginGuestManager {
 public:
  virtual ~BrowserPluginGuestManager() {}

  // Requests a guest WebContents associated with the provided
  // |browser_plugin_instance_id|.
  // Returns the guest associated with the provided ID if one exists.
  virtual WebContents* GetGuestByInstanceID(WebContents* embedder_web_contents,
                                            int browser_plugin_instance_id);

  // Iterates over all WebContents belonging to a given |embedder_web_contents|,
  // calling |callback| for each. If one of the callbacks returns true, then
  // the iteration exits early.
  typedef base::Callback<bool(WebContents*)> GuestCallback;
  virtual bool ForEachGuest(WebContents* embedder_web_contents,
                            const GuestCallback& callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_H_
