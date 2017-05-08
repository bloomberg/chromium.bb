// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"

#include <utility>
#include <vector>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
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
    scoped_refptr<base::RefCountedMemory> data) {
  if (data.get() == nullptr) {
    callback.Run(std::vector<uint8_t>{});
    return;
  }
  std::vector<uint8_t> result(data->front(), data->front() + data->size());
  callback.Run(result);
}

}  // namespace

// static
const char ArcVoiceInteractionFrameworkService::kArcServiceName[] =
    "arc::ArcVoiceInteractionFrameworkService";

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

  // TODO(updowndota): Move the dynamic shortcuts to accelerator_controller.cc
  // to prevent several issues.
  ash::Shell::Get()->accelerator_controller()->Register(
      {ui::Accelerator(ui::VKEY_A, ui::EF_COMMAND_DOWN)}, this);
  // Temporary shortcut added to enable the metalayer experiment.
  ash::Shell::Get()->accelerator_controller()->Register(
      {ui::Accelerator(ui::VKEY_A, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN)},
      this);
  // Temporary shortcut added for UX/PM exploration.
  ash::Shell::Get()->accelerator_controller()->Register(
      {ui::Accelerator(ui::VKEY_SPACE, ui::EF_COMMAND_DOWN)}, this);
}

void ArcVoiceInteractionFrameworkService::OnInstanceClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ash::Shell::Get()->accelerator_controller()->UnregisterAll(this);
  if (!metalayer_closed_callback_.is_null())
    base::ResetAndReturn(&metalayer_closed_callback_).Run();
}

bool ArcVoiceInteractionFrameworkService::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (accelerator.IsShiftDown()) {
    // Temporary, used for debugging.
    // Does not take into account or update the palette state.
    mojom::VoiceInteractionFrameworkInstance* framework_instance =
        ARC_GET_INSTANCE_FOR_METHOD(
            arc_bridge_service()->voice_interaction_framework(),
            SetMetalayerVisibility);
    DCHECK(framework_instance);
    framework_instance->SetMetalayerVisibility(true);
  } else {
    mojom::VoiceInteractionFrameworkInstance* framework_instance =
        ARC_GET_INSTANCE_FOR_METHOD(
            arc_bridge_service()->voice_interaction_framework(),
            StartVoiceInteractionSession);
    DCHECK(framework_instance);
    framework_instance->StartVoiceInteractionSession();
  }

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
  ui::GrabWindowSnapshotAsyncJPEG(
      window, gfx::Rect(window->bounds().size()),
      base::CreateTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING}),
      base::Bind(&ScreenshotCallback, callback));
}

void ArcVoiceInteractionFrameworkService::CaptureFullscreen(
    const CaptureFullscreenCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Since ARC currently only runs in primary display, we restrict
  // the screenshot to it.
  aura::Window* window = ash::Shell::GetPrimaryRootWindow();
  DCHECK(window);
  ui::GrabWindowSnapshotAsyncJPEG(
      window, gfx::Rect(window->bounds().size()),
      base::CreateTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING}),
      base::Bind(&ScreenshotCallback, callback));
}

void ArcVoiceInteractionFrameworkService::OnMetalayerClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!metalayer_closed_callback_.is_null())
    base::ResetAndReturn(&metalayer_closed_callback_).Run();
}

bool ArcVoiceInteractionFrameworkService::IsMetalayerSupported() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::VoiceInteractionFrameworkInstance* framework_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service()->voice_interaction_framework(),
          SetMetalayerVisibility);
  return framework_instance;
}

void ArcVoiceInteractionFrameworkService::ShowMetalayer(
    const base::Closure& closed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!metalayer_closed_callback_.is_null()) {
    LOG(ERROR) << "Metalayer is already enabled";
    return;
  }
  metalayer_closed_callback_ = closed;
  SetMetalayerVisibility(true);
}

void ArcVoiceInteractionFrameworkService::HideMetalayer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (metalayer_closed_callback_.is_null()) {
    LOG(ERROR) << "Metalayer is already hidden";
    return;
  }
  metalayer_closed_callback_ = base::Closure();
  SetMetalayerVisibility(false);
}

void ArcVoiceInteractionFrameworkService::SetMetalayerVisibility(bool visible) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::VoiceInteractionFrameworkInstance* framework_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service()->voice_interaction_framework(),
          SetMetalayerVisibility);
  if (!framework_instance) {
    if (!metalayer_closed_callback_.is_null())
      base::ResetAndReturn(&metalayer_closed_callback_).Run();
    return;
  }
  framework_instance->SetMetalayerVisibility(visible);
}

}  // namespace arc
