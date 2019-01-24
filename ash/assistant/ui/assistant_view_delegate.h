// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_VIEW_DELEGATE_H_
#define ASH_ASSISTANT_UI_ASSISTANT_VIEW_DELEGATE_H_

#include <map>
#include <string>
#include <vector>

#include "ash/assistant/model/assistant_cache_model.h"
#include "ash/assistant/model/assistant_cache_model_observer.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/assistant_mini_view.h"
#include "ash/assistant/ui/caption_bar.h"
#include "ash/assistant/ui/dialog_plate/dialog_plate.h"
#include "ash/assistant/ui/main_stage/assistant_opt_in_view.h"
#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/interfaces/assistant_image_downloader.mojom.h"
#include "base/observer_list_types.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

namespace assistant {
namespace util {
enum class DeepLinkType;
}  // namespace util
}  // namespace assistant

class ASH_PUBLIC_EXPORT AssistantViewDelegateObserver
    : public base::CheckedObserver {
 public:
  // Invoked when Assistant has received a deep link of the specified |type|
  // with the given |params|.
  virtual void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) {}
};

// A delegate of views in assistant/ui that handles views related actions e.g.
// get models for the views, adding observers, closing the views, opening urls,
// etc.
class ASH_PUBLIC_EXPORT AssistantViewDelegate {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;

  virtual ~AssistantViewDelegate() {}

  // Gets the cache model associated with the view delegate.
  virtual const AssistantCacheModel* GetCacheModel() const = 0;

  // Gets the interaction model associated with the view delegate.
  virtual const AssistantInteractionModel* GetInteractionModel() const = 0;

  // Gets the ui model associated with the view delegate.
  virtual const AssistantUiModel* GetUiModel() const = 0;

  // Adds/removes the cache model observer associated with the view delegate.
  virtual void AddCacheModelObserver(AssistantCacheModelObserver* observer) = 0;
  virtual void RemoveCacheModelObserver(
      AssistantCacheModelObserver* observer) = 0;

  // Adds/removes the interaction model observer associated with the view
  // delegate.
  virtual void AddInteractionModelObserver(
      AssistantInteractionModelObserver* observer) = 0;
  virtual void RemoveInteractionModelObserver(
      AssistantInteractionModelObserver* observer) = 0;

  // Adds/removes the ui model observer associated with the view delegate.
  virtual void AddUiModelObserver(AssistantUiModelObserver* observer) = 0;
  virtual void RemoveUiModelObserver(AssistantUiModelObserver* observer) = 0;

  // Adds/removes the view delegate observer.
  virtual void AddViewDelegateObserver(
      AssistantViewDelegateObserver* observer) = 0;
  virtual void RemoveViewDelegateObserver(
      AssistantViewDelegateObserver* observer) = 0;

  // Adds/removes the voice interaction controller observer associated with the
  // view delegate.
  virtual void AddVoiceInteractionControllerObserver(
      DefaultVoiceInteractionObserver* observer) = 0;
  virtual void RemoveVoiceInteractionControllerObserver(
      DefaultVoiceInteractionObserver* observer) = 0;

  // Gets the caption bar delegate associated with the view delegate.
  virtual CaptionBarDelegate* GetCaptionBarDelegate() = 0;

  // Gets the dialog plate observers associated with the view delegate.
  virtual std::vector<DialogPlateObserver*> GetDialogPlateObservers() = 0;

  // Gets the mini view delegate associated with the view delegate.
  virtual AssistantMiniViewDelegate* GetMiniViewDelegate() = 0;

  // Gets the opt in delegate associated with the view delegate.
  virtual AssistantOptInDelegate* GetOptInDelegate() = 0;

  // Downloads the image found at the specified |url|. On completion, the
  // supplied |callback| will be run with the downloaded image. If the download
  // attempt is unsuccessful, a NULL image is returned.
  virtual void DownloadImage(
      const GURL& url,
      mojom::AssistantImageDownloader::DownloadCallback callback) = 0;

  // Returns the cursor_manager.
  virtual wm::CursorManager* GetCursorManager() = 0;

  // Acquires a NavigableContentsFactory from the Content Service to allow
  // Assistant to display embedded web contents.
  virtual void GetNavigableContentsFactoryForView(
      content::mojom::NavigableContentsFactoryRequest request) = 0;

  virtual aura::Window* GetRootWindowForNewWindows() = 0;

  // Returns true if in tablet mode.
  virtual bool IsTabletMode() const = 0;

  // Invoked when suggestion chip is pressed.
  virtual void OnSuggestionChipPressed(
      const AssistantSuggestion* suggestion) = 0;

  // Opens the specified |url| in a new browser tab. Special handling is applied
  // to deep links which may cause deviation from this behavior.
  virtual void OpenUrlFromView(const GURL& url) = 0;

  // Returns true if voice interaction controller setup completed.
  virtual bool VoiceInteractionControllerSetupCompleted() const = 0;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_VIEW_DELEGATE_H_
