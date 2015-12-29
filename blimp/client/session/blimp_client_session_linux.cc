// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/session/blimp_client_session_linux.h"

#include "base/message_loop/message_loop.h"
#include "blimp/client/linux/blimp_display_manager.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/size.h"

namespace blimp {

BlimpClientSessionLinux::BlimpClientSessionLinux()
    : event_source_(ui::PlatformEventSource::CreateDefault()) {
  blimp_display_manager_.reset(new BlimpDisplayManager(gfx::Size(800, 600),
                                                       this,
                                                       GetRenderWidgetFeature(),
                                                       GetTabControlFeature()));
}

BlimpClientSessionLinux::~BlimpClientSessionLinux() {}

void BlimpClientSessionLinux::OnClosed() {
  base::MessageLoop::current()->QuitNow();
}

}  // namespace blimp
