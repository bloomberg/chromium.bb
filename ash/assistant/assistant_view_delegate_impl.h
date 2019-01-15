// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_VIEW_DELEGATE_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_VIEW_DELEGATE_IMPL_H_

#include "ash/assistant/ui/assistant_view_delegate.h"
#include "base/macros.h"

namespace ash {

class AssistantController;

class AssistantViewDelegateImpl : public AssistantViewDelegate {
 public:
  AssistantViewDelegateImpl(AssistantController* assistant_controller);
  ~AssistantViewDelegateImpl() override;

  void NotifyDeepLinkReceived(assistant::util::DeepLinkType type,
                              const std::map<std::string, std::string>& params);

  // AssistantViewDelegate:
  const AssistantCacheModel* GetCacheModel() const override;
  const AssistantInteractionModel* GetInteractionModel() const override;
  const AssistantUiModel* GetUiModel() const override;
  void AddCacheModelObserver(AssistantCacheModelObserver* observer) override;
  void RemoveCacheModelObserver(AssistantCacheModelObserver* observer) override;
  void AddInteractionModelObserver(
      AssistantInteractionModelObserver* observer) override;
  void RemoveInteractionModelObserver(
      AssistantInteractionModelObserver* observer) override;
  void AddUiModelObserver(AssistantUiModelObserver* observer) override;
  void RemoveUiModelObserver(AssistantUiModelObserver* observer) override;
  void AddViewDelegateObserver(
      AssistantViewDelegateObserver* observer) override;
  void RemoveViewDelegateObserver(
      AssistantViewDelegateObserver* observer) override;
  void AddVoiceInteractionControllerObserver(
      DefaultVoiceInteractionObserver* observer) override;
  void RemoveVoiceInteractionControllerObserver(
      DefaultVoiceInteractionObserver* observer) override;
  CaptionBarDelegate* GetCaptionBarDelegate() override;
  std::vector<DialogPlateObserver*> GetDialogPlateObservers() override;
  AssistantMiniViewDelegate* GetMiniViewDelegate() override;
  AssistantOptInDelegate* GetOptInDelegate() override;
  void DownloadImage(
      const GURL& url,
      mojom::AssistantImageDownloader::DownloadCallback callback) override;
  wm::CursorManager* GetCursorManager() override;
  void GetNavigableContentsFactoryForView(
      content::mojom::NavigableContentsFactoryRequest request) override;
  aura::Window* GetRootWindowForNewWindows() override;
  bool IsTabletMode() const override;
  void OnSuggestionChipPressed(const AssistantSuggestion* suggestion) override;
  void OpenUrlFromView(const GURL& url) override;
  bool VoiceInteractionControllerSetupCompleted() const override;

 private:
  AssistantController* assistant_controller_;
  base::ObserverList<AssistantViewDelegateObserver> view_delegate_observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantViewDelegateImpl);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_VIEW_DELEGATE_IMPL_H_
