// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_screen_context_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/task_scheduler/post_task.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/snapshot/snapshot.h"
#include "ui/snapshot/snapshot_aura.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// When the screenshot's dimensions are smaller than this size, we will stop
// downsampling.
constexpr int kScreenshotMaxWidth = 1366;
constexpr int kScreenshotMaxHeight = 768;

std::vector<uint8_t> DownsampleAndEncodeImage(gfx::Image image) {
  // We'll downsample the screenshot to avoid exceeding max allowed size on
  // Assistant server side if we are taking screenshot from high-res screen.
  std::vector<uint8_t> res;
  gfx::JPEGCodec::Encode(
      SkBitmapOperations::DownsampleByTwoUntilSize(
          image.AsBitmap(), kScreenshotMaxWidth, kScreenshotMaxHeight),
      /*quality=*/100, &res);
  return res;
}

void EncodeScreenshotAndRunCallback(
    mojom::AssistantController::RequestScreenshotCallback callback,
    std::unique_ptr<ui::LayerTreeOwner> layer_owner,
    gfx::Image image) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      base::TaskTraits{base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&DownsampleAndEncodeImage, std::move(image)),
      std::move(callback));
}

std::unique_ptr<ui::LayerTreeOwner> CreateLayerForAssistantSnapshot(
    aura::Window* root_window) {
  using LayerSet = base::flat_set<const ui::Layer*>;
  LayerSet excluded_layers;
  LayerSet blocked_layers;

  aura::Window* overlay_container =
      ash::Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);

  if (overlay_container)
    excluded_layers.insert(overlay_container->layer());

  MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList();

  for (aura::Window* window : windows) {
    if (window->GetProperty(kBlockedForAssistantSnapshotKey))
      blocked_layers.insert(window->layer());
  }

  return ::wm::RecreateLayersWithClosure(
      root_window,
      base::BindRepeating(
          [](LayerSet blocked_layers, LayerSet excluded_layers,
             ui::LayerOwner* owner) -> std::unique_ptr<ui::Layer> {
            // Parent layer is excluded meaning that it's pointless to clone
            // current child and all its descendants. This reduces the number
            // of layers to create.
            if (base::ContainsKey(blocked_layers, owner->layer()->parent()))
              return nullptr;

            if (base::ContainsKey(blocked_layers, owner->layer())) {
              // Blocked layers are replaced with solid black layers so that
              // they won't be included in the screenshot but still preserve
              // the window stacking.
              auto layer =
                  std::make_unique<ui::Layer>(ui::LayerType::LAYER_SOLID_COLOR);
              layer->SetBounds(owner->layer()->bounds());
              layer->SetColor(SK_ColorBLACK);
              return layer;
            }

            if (excluded_layers.count(owner->layer()))
              return nullptr;

            return owner->RecreateLayer();
          },
          std::move(blocked_layers), std::move(excluded_layers)));
}

}  // namespace

AssistantScreenContextController::AssistantScreenContextController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      screen_context_request_factory_(this) {
  assistant_controller_->AddObserver(this);
  Shell::Get()->highlighter_controller()->AddObserver(this);
}

AssistantScreenContextController::~AssistantScreenContextController() {
  Shell::Get()->highlighter_controller()->RemoveObserver(this);
  assistant_controller_->RemoveObserver(this);
}

void AssistantScreenContextController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = std::move(assistant);
}

void AssistantScreenContextController::AddModelObserver(
    AssistantScreenContextModelObserver* observer) {
  assistant_screen_context_model_.AddObserver(observer);
}

void AssistantScreenContextController::RemoveModelObserver(
    AssistantScreenContextModelObserver* observer) {
  assistant_screen_context_model_.RemoveObserver(observer);
}

void AssistantScreenContextController::RequestScreenshot(
    const gfx::Rect& rect,
    mojom::AssistantController::RequestScreenshotCallback callback) {
  // TODO(muyuanli): Handle multi-display when Assistant's behavior is defined.
  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  std::unique_ptr<ui::LayerTreeOwner> layer_owner =
      CreateLayerForAssistantSnapshot(root_window);

  ui::Layer* root_layer = layer_owner->root();

  gfx::Rect source_rect =
      rect.IsEmpty() ? gfx::Rect(root_window->bounds().size()) : rect;

  // The root layer might have a scaling transform applied (if the user has
  // changed the UI scale via Ctrl-Shift-Plus/Minus). Clear the transform so
  // that the snapshot is taken at 1:1 scale relative to screen pixels.
  root_layer->SetTransform(gfx::Transform());
  root_window->layer()->Add(root_layer);
  root_window->layer()->StackAtBottom(root_layer);

  ui::GrabLayerSnapshotAsync(
      root_layer, source_rect,
      base::BindRepeating(&EncodeScreenshotAndRunCallback,
                          base::Passed(std::move(callback)),
                          base::Passed(std::move(layer_owner))));
}

void AssistantScreenContextController::RequestScreenContext(
    const gfx::Rect& rect) {
  // Abort any request in progress and update request state.
  screen_context_request_factory_.InvalidateWeakPtrs();
  assistant_screen_context_model_.SetRequestState(
      ScreenContextRequestState::kInProgress);

  // Request screen context for the entire screen.
  assistant_->RequestScreenContext(
      gfx::Rect(),
      base::BindOnce(
          &AssistantScreenContextController::OnScreenContextRequestFinished,
          screen_context_request_factory_.GetWeakPtr()));
}

void AssistantScreenContextController::OnAssistantControllerConstructed() {
  assistant_controller_->ui_controller()->AddModelObserver(this);
}

void AssistantScreenContextController::OnAssistantControllerDestroying() {
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
}

void AssistantScreenContextController::OnUiVisibilityChanged(
    bool visible,
    AssistantSource source) {
  // We don't initiate a contextual query for caching if the UI is being hidden.
  // Instead, we abort any requests in progress and reset state.
  if (!visible) {
    screen_context_request_factory_.InvalidateWeakPtrs();
    assistant_screen_context_model_.SetRequestState(
        ScreenContextRequestState::kIdle);
    return;
  }

  InputModality input_modality = assistant_controller_->interaction_controller()
                                     ->model()
                                     ->input_modality();

  // We don't initiate a contextual query for caching if we are using stylus
  // input modality because we will do so on metalayer session complete.
  if (input_modality == InputModality::kStylus)
    return;

  // Request screen context for the entire screen.
  RequestScreenContext(gfx::Rect());
}

void AssistantScreenContextController::OnHighlighterSelectionRecognized(
    const gfx::Rect& rect) {
  // Request screen context for the selected region.
  RequestScreenContext(rect);
}

void AssistantScreenContextController::OnScreenContextRequestFinished() {
  assistant_screen_context_model_.SetRequestState(
      ScreenContextRequestState::kIdle);
}

std::unique_ptr<ui::LayerTreeOwner>
AssistantScreenContextController::CreateLayerForAssistantSnapshotForTest() {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  return CreateLayerForAssistantSnapshot(root_window);
}

}  // namespace ash
