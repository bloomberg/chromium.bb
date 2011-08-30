// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "aura/desktop.h"

#include "aura/desktop_host.h"
#include "aura/window.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ui/gfx/compositor/compositor.h"

namespace aura {

// static
Desktop* Desktop::instance_ = NULL;

Desktop::Desktop()
    : host_(aura::DesktopHost::Create(gfx::Rect(200, 200, 1024, 768))) {
  compositor_ = ui::Compositor::Create(host_->GetAcceleratedWidget(),
                                       host_->GetSize());
  host_->SetDesktop(this);
  DCHECK(compositor_.get());
  window_.reset(new Window(NULL));
}

Desktop::~Desktop() {
}

void Desktop::Run() {
  host_->Show();
  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
  MessageLoopForUI::current()->Run(host_);
}

void Desktop::Draw() {
  // Second pass renders the layers.
  compositor_->NotifyStart();
  window_->DrawTree();
  compositor_->NotifyEnd();
}

bool Desktop::OnMouseEvent(const MouseEvent& event) {
  return window_->OnMouseEvent(event);
}

// static
Desktop* Desktop::GetInstance() {
  if (!instance_) {
    instance_ = new Desktop;
    instance_->window_->Init();
  }
  return instance_;
}

}  // namespace aura
