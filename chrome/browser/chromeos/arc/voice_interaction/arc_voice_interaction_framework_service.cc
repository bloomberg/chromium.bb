// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"

#include <utility>
#include <vector>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_util.h"
#include "components/arc/instance_holder.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/snapshot/snapshot.h"
#include "ui/snapshot/snapshot_aura.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace arc {

namespace {

using LayerSet = base::flat_set<const ui::Layer*>;

// Time out for a context query from container since user initiated
// interaction. This must be strictly less than
// kMaxTimeSinceUserInteractionForHistogram so that the histogram
// could cover the range of normal operations.
constexpr base::TimeDelta kAllowedTimeSinceUserInteraction =
    base::TimeDelta::FromSeconds(2);
constexpr base::TimeDelta kMaxTimeSinceUserInteractionForHistogram =
    base::TimeDelta::FromSeconds(5);

constexpr int32_t kContextRequestMaxRemainingCount = 2;

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

bool IsMetalayerWindow(aura::Window* window) {
  exo::Surface* surface = exo::Surface::AsSurface(window);
  return surface != nullptr && surface->IsStylusOnly();
}

void CollectLayers(LayerSet& layers,
                   aura::Window* root_window,
                   base::Callback<bool(aura::Window*)> matcher_func) {
  if (matcher_func.Run(root_window))
    layers.insert(root_window->layer());

  aura::Window::Windows children = root_window->children();
  for (aura::Window::Windows::iterator iter = children.begin();
       iter != children.end(); ++iter) {
    CollectLayers(layers, *iter, matcher_func);
  }
}

std::unique_ptr<ui::LayerTreeOwner> CreateLayerTreeForSnapshot(
    aura::Window* root_window) {
  LayerSet blocked_layers;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->IsOffTheRecord())
      blocked_layers.insert(browser->window()->GetNativeWindow()->layer());
  }

  LayerSet excluded_layers;
  CollectLayers(excluded_layers, root_window, base::Bind(IsMetalayerWindow));

  auto layer_tree_owner = ::wm::RecreateLayersWithClosure(
      root_window, base::BindRepeating(
                       [](LayerSet blocked_layers, LayerSet excluded_layers,
                          ui::LayerOwner* owner) -> std::unique_ptr<ui::Layer> {
                         // Parent layer is excluded meaning that it's pointless
                         // to clone current child and all its descendants. This
                         // reduces the number of layers to create.
                         if (blocked_layers.count(owner->layer()->parent()))
                           return nullptr;
                         if (blocked_layers.count(owner->layer())) {
                           auto layer = base::MakeUnique<ui::Layer>(
                               ui::LayerType::LAYER_SOLID_COLOR);
                           layer->SetBounds(owner->layer()->bounds());
                           layer->SetColor(SK_ColorBLACK);
                           return layer;
                         }
                         if (excluded_layers.count(owner->layer()))
                           return nullptr;
                         return owner->RecreateLayer();
                       },
                       std::move(blocked_layers), std::move(excluded_layers)));

  // layer_tree_owner cannot be null since we are starting off from the root
  // window, which could never be incognito.
  DCHECK(layer_tree_owner);

  auto* root_layer = layer_tree_owner->root();
  root_window->layer()->Add(root_layer);
  root_window->layer()->StackAtBottom(root_layer);
  return layer_tree_owner;
}

void EncodeAndReturnImage(
    const ArcVoiceInteractionFrameworkService::CaptureFullscreenCallback&
        callback,
    std::unique_ptr<ui::LayerTreeOwner> old_layer_owner,
    const gfx::Image& image) {
  old_layer_owner.reset();
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      base::TaskTraits{base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::Bind(
          [](const gfx::Image& image) -> std::vector<uint8_t> {
            std::vector<uint8_t> res;
            gfx::JPEG1xEncodedDataFromImage(image, 100, &res);
            return res;
          },
          image),
      callback);
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
  mojom::VoiceInteractionFrameworkHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  framework_instance->Init(std::move(host_proxy));

  // Temporary shortcut added to enable the metalayer experiment.
  ash::Shell::Get()->accelerator_controller()->Register(
      {ui::Accelerator(ui::VKEY_A, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN)},
      this);
}

void ArcVoiceInteractionFrameworkService::OnInstanceClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ash::Shell::Get()->accelerator_controller()->UnregisterAll(this);
  CallAndResetMetalayerCallback();
  metalayer_enabled_ = false;
}

bool ArcVoiceInteractionFrameworkService::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::RecordAction(base::UserMetricsAction(
      "VoiceInteraction.MetalayerStarted.Search_Shift_A"));

  // Temporary, used for debugging.
  // Does not take into account or update the palette state.
  // Explicitly call ShowMetalayer() to take into account user interaction
  // initiations.
  ShowMetalayer(base::Bind([]() {}));
  return true;
}

bool ArcVoiceInteractionFrameworkService::CanHandleAccelerators() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return true;
}

void ArcVoiceInteractionFrameworkService::OnTouchEvent(ui::TouchEvent* event) {
  if (event->pointer_details().pointer_type !=
      ui::EventPointerType::POINTER_TYPE_PEN)
    return;

  if (event->type() == ui::ET_TOUCH_RELEASED) {
    // Stylus gesture has just completed. This handler is only active while
    // the metalayer is on. Looks like a legitimate metalayer action.
    InitiateUserInteraction();
  }
}

void ArcVoiceInteractionFrameworkService::CaptureFocusedWindow(
    const CaptureFocusedWindowCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!ValidateTimeSinceUserInteraction()) {
    callback.Run(std::vector<uint8_t>{});
    return;
  }

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

  if (!ValidateTimeSinceUserInteraction()) {
    callback.Run(std::vector<uint8_t>{});
    return;
  }

  // Since ARC currently only runs in primary display, we restrict
  // the screenshot to it.
  aura::Window* window = ash::Shell::GetPrimaryRootWindow();
  DCHECK(window);

  auto old_layer_owner = CreateLayerTreeForSnapshot(window);
  ui::GrabLayerSnapshotAsync(
      old_layer_owner->root(), gfx::Rect(window->bounds().size()),
      base::Bind(&EncodeAndReturnImage, callback,
                 base::Passed(std::move(old_layer_owner))));
}

void ArcVoiceInteractionFrameworkService::OnMetalayerClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CallAndResetMetalayerCallback();
}

void ArcVoiceInteractionFrameworkService::SetMetalayerEnabled(bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  metalayer_enabled_ = enabled;
  if (!metalayer_enabled_)
    CallAndResetMetalayerCallback();
}

bool ArcVoiceInteractionFrameworkService::IsMetalayerSupported() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return metalayer_enabled_;
}

void ArcVoiceInteractionFrameworkService::ShowMetalayer(
    const base::Closure& closed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!metalayer_closed_callback_.is_null()) {
    LOG(ERROR) << "Metalayer is already enabled";
    return;
  }
  metalayer_closed_callback_ = closed;
  // Hanlde all stylus events while the metalayer is on.
  exo::WMHelper::GetInstance()->AddPreTargetHandler(this);
  SetMetalayerVisibility(true);
}

void ArcVoiceInteractionFrameworkService::HideMetalayer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (metalayer_closed_callback_.is_null()) {
    LOG(ERROR) << "Metalayer is already hidden";
    return;
  }
  metalayer_closed_callback_ = base::Closure();
  exo::WMHelper::GetInstance()->RemovePreTargetHandler(this);
  SetMetalayerVisibility(false);
}

void ArcVoiceInteractionFrameworkService::StartVoiceInteractionSetupWizard() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc::mojom::VoiceInteractionFrameworkInstance* framework_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service()->voice_interaction_framework(),
          StartVoiceInteractionSetupWizard);
  if (!framework_instance)
    return;
  framework_instance->StartVoiceInteractionSetupWizard();
}

void ArcVoiceInteractionFrameworkService::SetMetalayerVisibility(bool visible) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::VoiceInteractionFrameworkInstance* framework_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service()->voice_interaction_framework(),
          SetMetalayerVisibility);
  if (!framework_instance) {
    CallAndResetMetalayerCallback();
    return;
  }
  framework_instance->SetMetalayerVisibility(visible);
}

void ArcVoiceInteractionFrameworkService::CallAndResetMetalayerCallback() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (metalayer_closed_callback_.is_null())
    return;
  base::ResetAndReturn(&metalayer_closed_callback_).Run();
  exo::WMHelper::GetInstance()->RemovePreTargetHandler(this);
}

void ArcVoiceInteractionFrameworkService::SetVoiceInteractionEnabled(
    bool enable) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojom::VoiceInteractionFrameworkInstance* framework_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service()->voice_interaction_framework(),
          SetVoiceInteractionEnabled);
  if (!framework_instance)
    return;
  framework_instance->SetVoiceInteractionEnabled(enable);
}

void ArcVoiceInteractionFrameworkService::SetVoiceInteractionContextEnabled(
    bool enable) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojom::VoiceInteractionFrameworkInstance* framework_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service()->voice_interaction_framework(),
          SetVoiceInteractionContextEnabled);
  if (!framework_instance)
    return;
  framework_instance->SetVoiceInteractionContextEnabled(enable);
}

void ArcVoiceInteractionFrameworkService::StartSessionFromUserInteraction(
    const gfx::Rect& rect) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          prefs::kArcVoiceInteractionValuePropAccepted)) {
    // If voice interaction value prop already showing, return.
    if (chromeos::LoginDisplayHost::default_host())
      return;
    // If voice interaction value prop has not been accepted, show the value
    // prop OOBE page again.
    gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));
    // The display host will be destructed at the end of OOBE flow.
    chromeos::LoginDisplayHostImpl* display_host =
        new chromeos::LoginDisplayHostImpl(screen_bounds);
    display_host->StartWizard(
        chromeos::OobeScreen::SCREEN_VOICE_INTERACTION_VALUE_PROP);
    return;
  }

  if (!arc_bridge_service()->voice_interaction_framework()->has_instance()) {
    SetArcCpuRestriction(false);
    return;
  }

  if (!InitiateUserInteraction())
    return;

  if (rect.IsEmpty()) {
    mojom::VoiceInteractionFrameworkInstance* framework_instance =
        ARC_GET_INSTANCE_FOR_METHOD(
            arc_bridge_service()->voice_interaction_framework(),
            StartVoiceInteractionSession);
    DCHECK(framework_instance);
    framework_instance->StartVoiceInteractionSession();
  } else {
    mojom::VoiceInteractionFrameworkInstance* framework_instance =
        ARC_GET_INSTANCE_FOR_METHOD(
            arc_bridge_service()->voice_interaction_framework(),
            StartVoiceInteractionSessionForRegion);
    DCHECK(framework_instance);
    framework_instance->StartVoiceInteractionSessionForRegion(rect);
  }
}

bool ArcVoiceInteractionFrameworkService::ValidateTimeSinceUserInteraction() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!context_request_remaining_count_) {
    // Allowed number of requests used up. But we still have additional
    // requests. It's likely that there is something malicious going on.
    LOG(ERROR) << "Illegal context request from container.";
    UMA_HISTOGRAM_BOOLEAN("VoiceInteraction.IllegalContextRequest", true);
    return false;
  }
  auto elapsed = base::TimeTicks::Now() - user_interaction_start_time_;
  elapsed = elapsed > kMaxTimeSinceUserInteractionForHistogram
                ? kMaxTimeSinceUserInteractionForHistogram
                : elapsed;

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "VoiceInteraction.UserInteractionToRequestArrival",
      elapsed.InMilliseconds(), 1,
      kMaxTimeSinceUserInteractionForHistogram.InMilliseconds(), 20);

  if (elapsed > kAllowedTimeSinceUserInteraction) {
    LOG(ERROR) << "Timed out since last user interaction.";
    context_request_remaining_count_ = 0;
    return false;
  }

  context_request_remaining_count_--;
  return true;
}

bool ArcVoiceInteractionFrameworkService::InitiateUserInteraction() {
  auto start_time = base::TimeTicks::Now();
  if ((start_time - user_interaction_start_time_) <
          kAllowedTimeSinceUserInteraction &&
      context_request_remaining_count_) {
    // If next request starts too soon and there is an active session in action,
    // we should drop it.
    return false;
  }
  user_interaction_start_time_ = start_time;
  context_request_remaining_count_ = kContextRequestMaxRemainingCount;
  return true;
}

}  // namespace arc
