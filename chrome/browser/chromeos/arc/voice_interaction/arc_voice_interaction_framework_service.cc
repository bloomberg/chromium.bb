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
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_util.h"
#include "components/arc/instance_holder.h"
#include "components/session_manager/core/session_manager.h"
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

std::unique_ptr<ui::LayerTreeOwner> CreateLayerTreeForSnapshot(
    aura::Window* root_window) {
  LayerSet blocked_layers;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->IsOffTheRecord())
      blocked_layers.insert(browser->window()->GetNativeWindow()->layer());
  }

  LayerSet excluded_layers;
  // Exclude metalayer-related layers. This will also include other layers
  // under kShellWindowId_OverlayContainer which is fine.
  aura::Window* overlay_container = ash::Shell::GetContainer(
      root_window, ash::kShellWindowId_OverlayContainer);
  if (overlay_container != nullptr)
    excluded_layers.insert(overlay_container->layer());

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
  // The root layer might have a scaling transform applied (if the user has
  // changed the UI scale via Ctrl-Shift-Plus/Minus).
  // Clear the transform so that the snapshot is taken at 1:1 scale relative
  // to screen pixels.
  root_layer->SetTransform(gfx::Transform());
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

// Singleton factory for ArcVoiceInteractionFrameworkService.
class ArcVoiceInteractionFrameworkServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcVoiceInteractionFrameworkService,
          ArcVoiceInteractionFrameworkServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName =
      "ArcVoiceInteractionFrameworkServiceFactory";

  static ArcVoiceInteractionFrameworkServiceFactory* GetInstance() {
    return base::Singleton<ArcVoiceInteractionFrameworkServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<
      ArcVoiceInteractionFrameworkServiceFactory>;
  ArcVoiceInteractionFrameworkServiceFactory() = default;
  ~ArcVoiceInteractionFrameworkServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory override:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    if (!chromeos::switches::IsVoiceInteractionEnabled())
      return nullptr;
    return ArcBrowserContextKeyedServiceFactoryBase::BuildServiceInstanceFor(
        context);
  }
};

void SetVoiceInteractionPrefs(mojom::VoiceInteractionStatusPtr status) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kVoiceInteractionPrefSynced, status->configured);
  prefs->SetBoolean(prefs::kVoiceInteractionEnabled,
                    status->configured && status->voice_interaction_enabled);
  prefs->SetBoolean(prefs::kVoiceInteractionContextEnabled,
                    status->configured && status->context_enabled);
}

}  // namespace

// static
ArcVoiceInteractionFrameworkService*
ArcVoiceInteractionFrameworkService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcVoiceInteractionFrameworkServiceFactory::GetForBrowserContext(
      context);
}

KeyedServiceBaseFactory* ArcVoiceInteractionFrameworkService::GetFactory() {
  return ArcVoiceInteractionFrameworkServiceFactory::GetInstance();
}

ArcVoiceInteractionFrameworkService::ArcVoiceInteractionFrameworkService(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : context_(context), arc_bridge_service_(bridge_service), binding_(this) {
  arc_bridge_service_->voice_interaction_framework()->AddObserver(this);
  ArcSessionManager::Get()->AddObserver(this);
  session_manager::SessionManager::Get()->AddObserver(this);
}

ArcVoiceInteractionFrameworkService::~ArcVoiceInteractionFrameworkService() {
  ArcSessionManager::Get()->RemoveObserver(this);
  arc_bridge_service_->voice_interaction_framework()->RemoveObserver(this);
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void ArcVoiceInteractionFrameworkService::OnInstanceReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::VoiceInteractionFrameworkInstance* framework_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service_->voice_interaction_framework(), Init);
  DCHECK(framework_instance);
  mojom::VoiceInteractionFrameworkHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  framework_instance->Init(std::move(host_proxy));

  if (is_request_pending_) {
    is_request_pending_ = false;
    framework_instance->StartVoiceInteractionSession();
  }
}

void ArcVoiceInteractionFrameworkService::OnInstanceClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CallAndResetMetalayerCallback();
  metalayer_enabled_ = false;
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

void ArcVoiceInteractionFrameworkService::SetVoiceInteractionRunning(
    bool running) {
  ash::Shell::Get()->NotifyVoiceInteractionStatusChanged(
      running ? ash::VoiceInteractionState::RUNNING
              : ash::VoiceInteractionState::STOPPED);
}

void ArcVoiceInteractionFrameworkService::SetVoiceInteractionState(
    ash::VoiceInteractionState state) {
  DCHECK_NE(state_, state);
  state_ = state;
  ash::Shell::Get()->NotifyVoiceInteractionStatusChanged(state);
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
  NotifyMetalayerStatusChanged(true);
}

void ArcVoiceInteractionFrameworkService::HideMetalayer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (metalayer_closed_callback_.is_null()) {
    LOG(ERROR) << "Metalayer is already hidden";
    return;
  }
  metalayer_closed_callback_ = base::Closure();
  NotifyMetalayerStatusChanged(false);
}

void ArcVoiceInteractionFrameworkService::OnArcPlayStoreEnabledChanged(
    bool enabled) {
  if (enabled)
    return;
  mojom::VoiceInteractionStatusPtr status =
      mojom::VoiceInteractionStatus::New();
  status->configured = false;
  status->voice_interaction_enabled = false;
  status->context_enabled = false;
  SetVoiceInteractionPrefs(std::move(status));
}

void ArcVoiceInteractionFrameworkService::OnSessionStateChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  if (session_state != session_manager::SessionState::ACTIVE)
    return;

  // TODO(crbug.com/757012): Avoid using ash::Shell here so that it can work in
  // mash.
  bool enabled = ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kVoiceInteractionEnabled);
  ash::Shell::Get()->NotifyVoiceInteractionEnabled(enabled);

  // We only want notify the status change on first user signed in.
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void ArcVoiceInteractionFrameworkService::StartVoiceInteractionSetupWizard() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc::mojom::VoiceInteractionFrameworkInstance* framework_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service_->voice_interaction_framework(),
          StartVoiceInteractionSetupWizard);
  if (!framework_instance)
    return;
  framework_instance->StartVoiceInteractionSetupWizard();
}

void ArcVoiceInteractionFrameworkService::NotifyMetalayerStatusChanged(
    bool visible) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::VoiceInteractionFrameworkInstance* framework_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service_->voice_interaction_framework(),
          SetMetalayerVisibility);
  if (!framework_instance)
    return;
  framework_instance->SetMetalayerVisibility(visible);
}

void ArcVoiceInteractionFrameworkService::CallAndResetMetalayerCallback() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (metalayer_closed_callback_.is_null())
    return;
  base::ResetAndReturn(&metalayer_closed_callback_).Run();
}

void ArcVoiceInteractionFrameworkService::SetVoiceInteractionEnabled(
    bool enable) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ash::Shell::Get()->NotifyVoiceInteractionEnabled(enable);

  mojom::VoiceInteractionFrameworkInstance* framework_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service_->voice_interaction_framework(),
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
          arc_bridge_service_->voice_interaction_framework(),
          SetVoiceInteractionContextEnabled);
  if (!framework_instance)
    return;
  framework_instance->SetVoiceInteractionContextEnabled(enable);
}

void ArcVoiceInteractionFrameworkService::UpdateVoiceInteractionPrefs() {
  if (ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          prefs::kVoiceInteractionPrefSynced)) {
    return;
  }
  mojom::VoiceInteractionFrameworkInstance* framework_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service_->voice_interaction_framework(),
          GetVoiceInteractionSettings);
  if (!framework_instance)
    return;
  framework_instance->GetVoiceInteractionSettings(
      base::Bind(&SetVoiceInteractionPrefs));
}

void ArcVoiceInteractionFrameworkService::StartSessionFromUserInteraction(
    const gfx::Rect& rect) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VLOG(1) << "Start voice interaction.";
  if (!Profile::FromBrowserContext(context_)->GetPrefs()->GetBoolean(
          prefs::kArcVoiceInteractionValuePropAccepted)) {
    VLOG(1) << "Voice interaction feature not accepted.";
    // If voice interaction value prop already showing, return.
    if (chromeos::LoginDisplayHost::default_host())
      return;
    // If voice interaction value prop has not been accepted, show the value
    // prop OOBE page again.
    gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));
    // The display host will be destructed at the end of OOBE flow.
    chromeos::LoginDisplayHostImpl* display_host =
        new chromeos::LoginDisplayHostImpl(screen_bounds);
    display_host->StartVoiceInteractionOobe();
    return;
  }

  if (state_ == ash::VoiceInteractionState::NOT_READY) {
    // If the container side is not ready, we will be waiting for a while.
    ash::Shell::Get()->NotifyVoiceInteractionStatusChanged(
        ash::VoiceInteractionState::NOT_READY);
  }

  if (!arc_bridge_service_->voice_interaction_framework()->has_instance()) {
    VLOG(1) << "Instance not ready.";
    SetArcCpuRestriction(false);
    is_request_pending_ = true;
    return;
  }

  if (!InitiateUserInteraction())
    return;

  if (rect.IsEmpty()) {
    mojom::VoiceInteractionFrameworkInstance* framework_instance =
        ARC_GET_INSTANCE_FOR_METHOD(
            arc_bridge_service_->voice_interaction_framework(),
            StartVoiceInteractionSession);
    DCHECK(framework_instance);
    framework_instance->StartVoiceInteractionSession();
  } else {
    mojom::VoiceInteractionFrameworkInstance* framework_instance =
        ARC_GET_INSTANCE_FOR_METHOD(
            arc_bridge_service_->voice_interaction_framework(),
            StartVoiceInteractionSessionForRegion);
    DCHECK(framework_instance);
    framework_instance->StartVoiceInteractionSessionForRegion(rect);
  }
  VLOG(1) << "Sent voice interaction request.";
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
    VLOG(1) << "Rejected voice interaction request.";
    return false;
  }
  user_interaction_start_time_ = start_time;
  context_request_remaining_count_ = kContextRequestMaxRemainingCount;
  return true;
}

}  // namespace arc
