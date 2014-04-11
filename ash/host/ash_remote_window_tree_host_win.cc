// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_remote_window_tree_host_win.h"

#include "ash/host/root_window_transformer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/transform.h"

namespace ash {
namespace {
AshRemoteWindowTreeHostWin* g_instance = NULL;
}

// static
void AshRemoteWindowTreeHostWin::Init() {
  DCHECK(!g_instance);
  g_instance = new AshRemoteWindowTreeHostWin();
  aura::RemoteWindowTreeHostWin::SetInstance(g_instance);
  CHECK_EQ(g_instance, aura::RemoteWindowTreeHostWin::Instance());
}

// static
AshRemoteWindowTreeHostWin* AshRemoteWindowTreeHostWin::GetInstance() {
  if (!g_instance)
    Init();
  CHECK_EQ(g_instance, aura::RemoteWindowTreeHostWin::Instance());
  return g_instance;
}

AshRemoteWindowTreeHostWin::AshRemoteWindowTreeHostWin()
    : aura::RemoteWindowTreeHostWin(gfx::Rect()), transformer_helper_(this) {
  g_instance = this;
}

AshRemoteWindowTreeHostWin::~AshRemoteWindowTreeHostWin() { g_instance = NULL; }

void AshRemoteWindowTreeHostWin::ToggleFullScreen() {}

bool AshRemoteWindowTreeHostWin::ConfineCursorToRootWindow() { return false; }

void AshRemoteWindowTreeHostWin::UnConfineCursor() {}

void AshRemoteWindowTreeHostWin::SetRootWindowTransformer(
    scoped_ptr<RootWindowTransformer> transformer) {
  transformer_helper_.SetRootWindowTransformer(transformer.Pass());
}

aura::WindowTreeHost* AshRemoteWindowTreeHostWin::AsWindowTreeHost() {
  return this;
}

gfx::Transform AshRemoteWindowTreeHostWin::GetRootTransform() const {
  return transformer_helper_.GetTransform();
}

void AshRemoteWindowTreeHostWin::SetRootTransform(
    const gfx::Transform& transform) {
  transformer_helper_.SetTransform(transform);
}

gfx::Transform AshRemoteWindowTreeHostWin::GetInverseRootTransform() const {
  return transformer_helper_.GetInverseTransform();
}

void AshRemoteWindowTreeHostWin::UpdateRootWindowSize(
    const gfx::Size& host_size) {
  transformer_helper_.UpdateWindowSize(host_size);
}

}  // namespace ash
