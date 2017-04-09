// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"

#include <utility>
#include <vector>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ash/wm_shell.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task_scheduler/post_task.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/instance_holder.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"
#include "ui/snapshot/snapshot.h"
#include "ui/wm/public/activation_client.h"

namespace arc {

namespace {

void ScreenshotCallback(
    const mojom::VoiceInteractionFrameworkHost::CaptureFocusedWindowCallback&
        callback,
    scoped_refptr<base::RefCountedMemory> png_data) {
  if (png_data.get() == nullptr) {
    callback.Run(std::vector<uint8_t>{});
    return;
  }
  std::vector<uint8_t> result(png_data->front(),
                              png_data->front() + png_data->size());
  callback.Run(result);
}

}  // namespace

// static
bool ArcVoiceInteractionFrameworkService::IsVoiceInteractionEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kEnableVoiceInteraction);
}

ArcVoiceInteractionFrameworkService::ArcVoiceInteractionFrameworkService(
    ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->voice_interaction_framework()->AddObserver(this);
}

ArcVoiceInteractionFrameworkService::~ArcVoiceInteractionFrameworkService() {
  arc_bridge_service()->voice_interaction_framework()->RemoveObserver(this);
}

void ArcVoiceInteractionFrameworkService::OnInstanceReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::VoiceInteractionFrameworkInstance* framework_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service()->voice_interaction_framework(), Init);
  DCHECK(framework_instance);
  framework_instance->Init(binding_.CreateInterfacePtrAndBind());

  ash::Shell::Get()->accelerator_controller()->Register(
      {ui::Accelerator(ui::VKEY_A, ui::EF_COMMAND_DOWN)}, this);
}

void ArcVoiceInteractionFrameworkService::OnInstanceClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ash::Shell::Get()->accelerator_controller()->UnregisterAll(this);
}

bool ArcVoiceInteractionFrameworkService::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::VoiceInteractionFrameworkInstance* framework_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service()->voice_interaction_framework(),
          StartVoiceInteractionSession);
  DCHECK(framework_instance);
  framework_instance->StartVoiceInteractionSession();
  return true;
}

bool ArcVoiceInteractionFrameworkService::CanHandleAccelerators() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return true;
}

void ArcVoiceInteractionFrameworkService::CaptureFocusedWindow(
    const CaptureFocusedWindowCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  aura::Window* window =
      ash::Shell::Get()->activation_client()->GetActiveWindow();

  if (window == nullptr) {
    callback.Run(std::vector<uint8_t>{});
    return;
  }
  ui::GrabWindowSnapshotAsyncPNG(window, gfx::Rect(window->bounds().size()),
                                 base::CreateTaskRunnerWithTraits(
                                     base::TaskTraits().MayBlock().WithPriority(
                                         base::TaskPriority::USER_BLOCKING)),
                                 base::Bind(&ScreenshotCallback, callback));
}

}  // namespace arc
