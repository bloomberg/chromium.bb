// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_HOST_FACTORY_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_HOST_FACTORY_H_

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "content/common/content_export.h"

struct BrowserPluginHostMsg_CreateGuest_Params;

namespace content {

class BrowserPluginEmbedder;
class BrowserPluginGuest;
class RenderViewHost;
class WebContentsImpl;

// Factory to create BrowserPlugin embedder and guest.
class CONTENT_EXPORT BrowserPluginHostFactory {
 public:
  virtual BrowserPluginGuestManager* CreateBrowserPluginGuestManager() = 0;

  virtual BrowserPluginGuest* CreateBrowserPluginGuest(
      int instance_id,
      WebContentsImpl* web_contents) = 0;

  virtual BrowserPluginEmbedder* CreateBrowserPluginEmbedder(
      WebContentsImpl* web_contents) = 0;

 protected:
  virtual ~BrowserPluginHostFactory() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_HOST_FACTORY_H_
