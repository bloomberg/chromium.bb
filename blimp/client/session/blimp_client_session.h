// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_H_
#define BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/client/blimp_client_export.h"

namespace blimp {

class BrowserConnectionHandler;
class NavigationFeature;
class RenderWidgetFeature;
class TabControlFeature;

// BlimpClientSession represents a single active session of Blimp on the client
// regardless of whether or not the client application is in the background or
// foreground.  The only time this session is invalid is during initialization
// and shutdown of this particular client process (or Activity on Android).
//
// This session glues together the feature proxy components and the network
// layer.  The network components must be interacted with on the IO thread.  The
// feature proxies must be interacted with on the UI thread.
class BLIMP_CLIENT_EXPORT BlimpClientSession {
 public:
  BlimpClientSession();

  TabControlFeature* GetTabControlFeature() const;
  NavigationFeature* GetNavigationFeature() const;
  RenderWidgetFeature* GetRenderWidgetFeature() const;

 protected:
  virtual ~BlimpClientSession();

 private:
  // The BrowserConnectionHandler is here so that the BlimpClientSession can
  // glue the feature-specific handlers to the actual network connection.
  scoped_ptr<BrowserConnectionHandler> connection_handler_;

  scoped_ptr<TabControlFeature> tab_control_feature_;
  scoped_ptr<NavigationFeature> navigation_feature_;
  scoped_ptr<RenderWidgetFeature> render_widget_feature_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientSession);
};

}  // namespace blimp

#endif  // BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_H_
