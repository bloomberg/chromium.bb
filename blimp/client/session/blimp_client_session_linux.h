// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_LINUX_H_
#define BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_LINUX_H_

#include "base/macros.h"
#include "blimp/client/linux/blimp_display_manager.h"
#include "blimp/client/session/blimp_client_session.h"
#include "blimp/client/session/navigation_feature.h"

namespace ui {
class PlatformEventSource;
}

namespace blimp {
namespace client {

class BlimpClientSessionLinux : public BlimpClientSession,
                                public BlimpDisplayManagerDelegate {
 public:
  BlimpClientSessionLinux();
  ~BlimpClientSessionLinux() override;

  // BlimpDisplayManagerDelegate implementation.
  void OnClosed() override;

 private:
  scoped_ptr<ui::PlatformEventSource> event_source_;
  scoped_ptr<BlimpDisplayManager> blimp_display_manager_;
  scoped_ptr<NavigationFeature::NavigationFeatureDelegate>
      navigation_feature_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientSessionLinux);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_LINUX_H_
