// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_EVENT_CLIENT_IMPL_H_
#define ASH_WM_EVENT_CLIENT_IMPL_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/event_client.h"

namespace ash {

class EventClientImpl : public aura::client::EventClient {
 public:
  EventClientImpl();
  virtual ~EventClientImpl();

 private:
  // Overridden from aura::client::EventClient:
  virtual bool CanProcessEventsWithinSubtree(
      const aura::Window* window) const OVERRIDE;
  virtual ui::EventTarget* GetToplevelEventTarget() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(EventClientImpl);
};

}  // namespace ash

#endif  // ASH_WM_EVENT_CLIENT_IMPL_H_
