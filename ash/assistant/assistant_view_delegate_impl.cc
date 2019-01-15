// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_view_delegate_impl.h"

#include "ash/assistant/assistant_cache_controller.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/shell.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"

namespace ash {

AssistantViewDelegateImpl::AssistantViewDelegateImpl(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {}

AssistantViewDelegateImpl::~AssistantViewDelegateImpl() = default;

const AssistantCacheModel* AssistantViewDelegateImpl::GetCacheModel() const {
  return assistant_controller_->cache_controller()->model();
}

const AssistantInteractionModel*
AssistantViewDelegateImpl::GetInteractionModel() const {
  return assistant_controller_->interaction_controller()->model();
}

const AssistantUiModel* AssistantViewDelegateImpl::GetUiModel() const {
  return assistant_controller_->ui_controller()->model();
}

void AssistantViewDelegateImpl::AddCacheModelObserver(
    AssistantCacheModelObserver* observer) {
  assistant_controller_->cache_controller()->AddModelObserver(observer);
}
void AssistantViewDelegateImpl::RemoveCacheModelObserver(
    AssistantCacheModelObserver* observer) {
  assistant_controller_->cache_controller()->RemoveModelObserver(observer);
}

void AssistantViewDelegateImpl::AddInteractionModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_controller_->interaction_controller()->AddModelObserver(observer);
}
void AssistantViewDelegateImpl::RemoveInteractionModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_controller_->interaction_controller()->RemoveModelObserver(
      observer);
}

void AssistantViewDelegateImpl::AddUiModelObserver(
    AssistantUiModelObserver* observer) {
  assistant_controller_->ui_controller()->AddModelObserver(observer);
}
void AssistantViewDelegateImpl::RemoveUiModelObserver(
    AssistantUiModelObserver* observer) {
  assistant_controller_->ui_controller()->RemoveModelObserver(observer);
}

void AssistantViewDelegateImpl::AddViewDelegateObserver(
    AssistantViewDelegateObserver* observer) {
  view_delegate_observers_.AddObserver(observer);
}
void AssistantViewDelegateImpl::RemoveViewDelegateObserver(
    AssistantViewDelegateObserver* observer) {
  view_delegate_observers_.RemoveObserver(observer);
}

void AssistantViewDelegateImpl::AddVoiceInteractionControllerObserver(
    DefaultVoiceInteractionObserver* observer) {
  Shell::Get()->voice_interaction_controller()->AddLocalObserver(observer);
}
void AssistantViewDelegateImpl::RemoveVoiceInteractionControllerObserver(
    DefaultVoiceInteractionObserver* observer) {
  Shell::Get()->voice_interaction_controller()->RemoveLocalObserver(observer);
}

CaptionBarDelegate* AssistantViewDelegateImpl::GetCaptionBarDelegate() {
  return assistant_controller_->ui_controller();
}

std::vector<DialogPlateObserver*>
AssistantViewDelegateImpl::GetDialogPlateObservers() {
  return {assistant_controller_->interaction_controller(),
          assistant_controller_->ui_controller()};
}

AssistantMiniViewDelegate* AssistantViewDelegateImpl::GetMiniViewDelegate() {
  return assistant_controller_->ui_controller();
}

AssistantOptInDelegate* AssistantViewDelegateImpl::GetOptInDelegate() {
  return assistant_controller_->setup_controller();
}

void AssistantViewDelegateImpl::DownloadImage(
    const GURL& url,
    mojom::AssistantImageDownloader::DownloadCallback callback) {
  assistant_controller_->DownloadImage(url, std::move(callback));
}

wm::CursorManager* AssistantViewDelegateImpl::GetCursorManager() {
  return Shell::Get()->cursor_manager();
}

void AssistantViewDelegateImpl::GetNavigableContentsFactoryForView(
    content::mojom::NavigableContentsFactoryRequest request) {
  assistant_controller_->GetNavigableContentsFactory(std::move(request));
}

aura::Window* AssistantViewDelegateImpl::GetRootWindowForNewWindows() {
  return Shell::Get()->GetRootWindowForNewWindows();
}

bool AssistantViewDelegateImpl::IsTabletMode() const {
  return Shell::Get()
      ->tablet_mode_controller()
      ->IsTabletModeWindowManagerEnabled();
}

void AssistantViewDelegateImpl::OnSuggestionChipPressed(
    const AssistantSuggestion* suggestion) {
  assistant_controller_->interaction_controller()->OnSuggestionChipPressed(
      suggestion);
}

void AssistantViewDelegateImpl::OpenUrlFromView(const GURL& url) {
  assistant_controller_->OpenUrl(url);
}

void AssistantViewDelegateImpl::NotifyDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  for (AssistantViewDelegateObserver& observer : view_delegate_observers_)
    observer.OnDeepLinkReceived(type, params);
}

bool AssistantViewDelegateImpl::VoiceInteractionControllerSetupCompleted()
    const {
  return Shell::Get()
      ->voice_interaction_controller()
      ->setup_completed()
      .value_or(false);
}

}  // namespace ash
