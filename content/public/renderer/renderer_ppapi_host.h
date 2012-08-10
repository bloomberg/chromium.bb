// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDERER_PPAPI_HOST_H_
#define CONTENT_PUBLIC_RENDERER_RENDERER_PPAPI_HOST_H_

#include "ppapi/c/pp_instance.h"

namespace ppapi {
namespace host {
class PpapiHost;
}
}

namespace content {

class RenderView;

// Interface that allows components in the embedder app to talk to the
// PpapiHost in the renderer process.
//
// There will be one of these objects in the renderer per plugin module.
class RendererPpapiHost {
 public:
  // Returns the PpapiHost object.
  virtual ppapi::host::PpapiHost* GetPpapiHost() = 0;

  // Returns true if the given PP_Instance is valid and belongs to the
  // plugin associated with this host.
  virtual bool IsValidInstance(PP_Instance instance) const = 0;

  // Returns the RenderView for the given plugin instance, or NULL if the
  // instance is invalid.
  virtual RenderView* GetRenderViewForInstance(PP_Instance instance) const = 0;

  // Returns true if the given instance is considered to be currently
  // processing a user gesture or the plugin module has the "override user
  // gesture" flag set (in which case it can always do things normally
  // restricted by user gestures). Returns false if the instance is invalid or
  // if there is no current user gesture.
  virtual bool HasUserGesture(PP_Instance instance) const = 0;

 protected:
  virtual ~RendererPpapiHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDERER_PPAPI_HOST_H_

