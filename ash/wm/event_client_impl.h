// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_EVENT_CLIENT_IMPL_H_
#define ASH_WM_EVENT_CLIENT_IMPL_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/client/event_client.h"

namespace ash {

class ASH_EXPORT EventClientImpl : public aura::client::EventClient {
 public:
  EventClientImpl();
  ~EventClientImpl() override;

 private:
  // Overridden from aura::client::EventClient:
  bool CanProcessEventsWithinSubtree(const aura::Window* window) const override;
  ui::EventTarget* GetToplevelEventTarget() override;

  DISALLOW_COPY_AND_ASSIGN(EventClientImpl);
};

}  // namespace ash

#endif  // ASH_WM_EVENT_CLIENT_IMPL_H_
