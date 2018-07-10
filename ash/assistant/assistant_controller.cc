// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller.h"

#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/new_window_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/unguessable_token.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/snapshot/snapshot.h"
#include "ui/snapshot/snapshot_aura.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// When screenshot's width or height is smaller than this size, we will stop
// downsampling.
constexpr int kScreenshotMaxWidth = 1366;
constexpr int kScreenshotMaxHeight = 768;

std::vector<uint8_t> DownsampleAndEncodeImage(gfx::Image image) {
  std::vector<uint8_t> res;
  // We'll downsample the screenshot to avoid exceeding max allowed size on
  // assistant server side if we are taking screenshot from high-res screen.
  gfx::JPEGCodec::Encode(
      SkBitmapOperations::DownsampleByTwoUntilSize(
          image.AsBitmap(), kScreenshotMaxWidth, kScreenshotMaxHeight),
      100 /* quality */, &res);
  return res;
}

void EncodeScreenshotAndRunCallback(
    AssistantController::RequestScreenshotCallback callback,
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

AssistantController::AssistantController()
    : assistant_interaction_controller_(
          std::make_unique<AssistantInteractionController>(this)),
      assistant_ui_controller_(std::make_unique<AssistantUiController>(this)),
      assistant_notification_controller_(
          std::make_unique<AssistantNotificationController>(this)),
      weak_factory_(this) {
  // Note that the sub-controllers have a circular dependency.
  // TODO(dmblack): Remove this circular dependency.
  assistant_interaction_controller_->SetAssistantUiController(
      assistant_ui_controller_.get());
  assistant_ui_controller_->SetAssistantInteractionController(
      assistant_interaction_controller_.get());

  assistant_ui_controller_->AddModelObserver(this);
  Shell::Get()->highlighter_controller()->AddObserver(this);
}

AssistantController::~AssistantController() {
  Shell::Get()->highlighter_controller()->RemoveObserver(this);
  assistant_ui_controller_->RemoveModelObserver(this);

  // Explicitly clean up the circular dependency in the sub-controllers.
  assistant_interaction_controller_->SetAssistantUiController(nullptr);
  assistant_ui_controller_->SetAssistantInteractionController(nullptr);
}

void AssistantController::BindRequest(
    mojom::AssistantControllerRequest request) {
  assistant_controller_bindings_.AddBinding(this, std::move(request));
}

void AssistantController::AddObserver(AssistantControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantController::RemoveObserver(
    AssistantControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantController::SetAssistant(
    chromeos::assistant::mojom::AssistantPtr assistant) {
  assistant_ = std::move(assistant);

  // Provide reference to sub-controllers.
  assistant_interaction_controller_->SetAssistant(assistant_.get());
  assistant_ui_controller_->SetAssistant(assistant_.get());
  assistant_notification_controller_->SetAssistant(assistant_.get());
}

void AssistantController::SetAssistantImageDownloader(
    mojom::AssistantImageDownloaderPtr assistant_image_downloader) {
  assistant_image_downloader_ = std::move(assistant_image_downloader);
}

void AssistantController::SetAssistantSetup(
    mojom::AssistantSetupPtr assistant_setup) {
  assistant_setup_ = std::move(assistant_setup);

  // Provide reference to UI controller.
  assistant_ui_controller_->SetAssistantSetup(assistant_setup_.get());
}

void AssistantController::SetWebContentsManager(
    mojom::WebContentsManagerPtr web_contents_manager) {
  web_contents_manager_ = std::move(web_contents_manager);
}

void AssistantController::RequestScreenshot(
    const gfx::Rect& rect,
    RequestScreenshotCallback callback) {
  // TODO(muyuanli): handle multi-display when assistant's behavior is defined.
  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  std::unique_ptr<ui::LayerTreeOwner> layer_owner =
      CreateLayerForAssistantSnapshot(root_window);

  ui::Layer* root_layer = layer_owner->root();

  gfx::Rect source_rect =
      rect.IsEmpty() ? gfx::Rect(root_window->bounds().size()) : rect;

  // The root layer might have a scaling transform applied (if the user has
  // changed the UI scale via Ctrl-Shift-Plus/Minus).
  // Clear the transform so that the snapshot is taken at 1:1 scale relative
  // to screen pixels.
  root_layer->SetTransform(gfx::Transform());
  root_window->layer()->Add(root_layer);
  root_window->layer()->StackAtBottom(root_layer);

  ui::GrabLayerSnapshotAsync(
      root_layer, source_rect,
      base::BindRepeating(&EncodeScreenshotAndRunCallback,
                          base::Passed(std::move(callback)),
                          base::Passed(std::move(layer_owner))));
}

void AssistantController::ManageWebContents(
    const base::UnguessableToken& id_token,
    mojom::ManagedWebContentsParamsPtr params,
    mojom::WebContentsManager::ManageWebContentsCallback callback) {
  DCHECK(web_contents_manager_);

  const mojom::UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(0);

  if (!user_session) {
    LOG(WARNING) << "Unable to retrieve active user session.";
    std::move(callback).Run(base::nullopt);
    return;
  }

  // Supply account ID.
  params->account_id = user_session->user_info->account_id;

  // Specify that we will handle top level browser requests.
  ash::mojom::ManagedWebContentsOpenUrlDelegatePtr ptr;
  web_contents_open_url_delegate_bindings_.AddBinding(this,
                                                      mojo::MakeRequest(&ptr));
  params->open_url_delegate_ptr_info = ptr.PassInterface();

  web_contents_manager_->ManageWebContents(id_token, std::move(params),
                                           std::move(callback));
}

void AssistantController::ReleaseWebContents(
    const base::UnguessableToken& id_token) {
  web_contents_manager_->ReleaseWebContents(id_token);
}

void AssistantController::ReleaseWebContents(
    const std::vector<base::UnguessableToken>& id_tokens) {
  web_contents_manager_->ReleaseAllWebContents(id_tokens);
}

void AssistantController::DownloadImage(
    const GURL& url,
    mojom::AssistantImageDownloader::DownloadCallback callback) {
  DCHECK(assistant_image_downloader_);

  const mojom::UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(0);

  if (!user_session) {
    LOG(WARNING) << "Unable to retrieve active user session.";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  AccountId account_id = user_session->user_info->account_id;
  assistant_image_downloader_->Download(account_id, url, std::move(callback));
}

void AssistantController::OnUiVisibilityChanged(bool visible,
                                                AssistantSource source) {
  // We don't initiate a contextual query for caching if the UI is being hidden.
  if (!visible)
    return;

  // We don't initiate a contextual query for caching if we are using stylus
  // input modality because we will do so on metalayer session complete.
  if (assistant_interaction_controller_->model()->input_modality() ==
      InputModality::kStylus) {
    return;
  }

  // TODO(dmblack): Activate the Assistant UI when the callback is run.
  assistant_->RequestScreenContext(gfx::Rect(), base::DoNothing());
}

void AssistantController::OnHighlighterSelectionRecognized(
    const gfx::Rect& rect) {
  // TODO(dmblack): Activate the Assistant UI when the callback is run.
  assistant_->RequestScreenContext(rect, base::DoNothing());
}

void AssistantController::OnOpenUrlFromTab(const GURL& url) {
  OpenUrl(url);
}

void AssistantController::OpenUrl(const GURL& url) {
  if (assistant::util::IsDeepLinkUrl(url)) {
    for (AssistantControllerObserver& observer : observers_)
      observer.OnDeepLinkReceived(url);
    return;
  }

  Shell::Get()->new_window_controller()->NewTabWithUrl(url);

  for (AssistantControllerObserver& observer : observers_)
    observer.OnUrlOpened(url);
}

base::WeakPtr<AssistantController> AssistantController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<ui::LayerTreeOwner>
AssistantController::CreateLayerForAssistantSnapshotForTest() {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  return CreateLayerForAssistantSnapshot(root_window);
}

}  // namespace ash
