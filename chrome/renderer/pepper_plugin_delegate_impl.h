// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
#define CHROME_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_

#include <set>

#include "base/basictypes.h"
#include "base/weak_ptr.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"

class RenderView;

namespace pepper {
class PluginInstance;
}

class PepperPluginDelegateImpl
    : public pepper::PluginDelegate,
      public base::SupportsWeakPtr<PepperPluginDelegateImpl> {
 public:
  explicit PepperPluginDelegateImpl(RenderView* render_view);

  // Called by RenderView to tell us about painting events, these two functions
  // just correspond to the DidInitiatePaint and DidFlushPaint in R.V..
  void ViewInitiatedPaint();
  void ViewFlushedPaint();

  // pepper::PluginDelegate implementation.
  virtual void InstanceCreated(pepper::PluginInstance* instance);
  virtual void InstanceDeleted(pepper::PluginInstance* instance);
  virtual PlatformImage2D* CreateImage2D(int width, int height);

 private:
  // Pointer to the RenderView that owns us.
  RenderView* render_view_;

  std::set<pepper::PluginInstance*> active_instances_;

  DISALLOW_COPY_AND_ASSIGN(PepperPluginDelegateImpl);
};

#endif  // CHROME_RENDERER_PEPPER_PLUGIN_DELEGATE_IMPL_H_
