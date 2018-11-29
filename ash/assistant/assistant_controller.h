// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "ash/public/interfaces/assistant_image_downloader.mojom.h"
#include "ash/public/interfaces/assistant_setup.mojom.h"
#include "ash/public/interfaces/assistant_volume_control.mojom.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"

namespace ash {

class AssistantCacheController;
class AssistantInteractionController;
class AssistantNotificationController;
class AssistantScreenContextController;
class AssistantSetupController;
class AssistantUiController;

class ASH_EXPORT AssistantController
    : public mojom::AssistantController,
      public AssistantControllerObserver,
      public DefaultVoiceInteractionObserver,
      public mojom::AssistantVolumeControl,
      public chromeos::CrasAudioHandler::AudioObserver,
      public AccessibilityObserver {
 public:
  AssistantController();
  ~AssistantController() override;

  void BindRequest(mojom::AssistantControllerRequest request);
  void BindRequest(mojom::AssistantVolumeControlRequest request);

  // Adds/removes the specified |observer|.
  void AddObserver(AssistantControllerObserver* observer);
  void RemoveObserver(AssistantControllerObserver* observer);

  // Downloads the image found at the specified |url|. On completion, the
  // supplied |callback| will be run with the downloaded image. If the download
  // attempt is unsuccessful, a NULL image is returned.
  void DownloadImage(
      const GURL& url,
      mojom::AssistantImageDownloader::DownloadCallback callback);

  // mojom::AssistantController:
  // TODO(updowndota): Refactor Set() calls to use a factory pattern.
  void SetAssistant(
      chromeos::assistant::mojom::AssistantPtr assistant) override;
  void SetAssistantImageDownloader(
      mojom::AssistantImageDownloaderPtr assistant_image_downloader) override;
  void OpenAssistantSettings() override;

  // AssistantControllerObserver:
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // mojom::VolumeControl:
  void SetVolume(int volume, bool user_initiated) override;
  void SetMuted(bool muted) override;
  void AddVolumeObserver(mojom::VolumeObserverPtr observer) override;

  // chromeos::CrasAudioHandler::AudioObserver:
  void OnOutputMuteChanged(bool mute_on, bool system_adjust) override;
  void OnOutputNodeVolumeChanged(uint64_t node, int volume) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // Opens the specified |url| in a new browser tab. Special handling is applied
  // to deep links which may cause deviation from this behavior.
  void OpenUrl(const GURL& url, bool from_server = false);

  // Acquires a NavigableContentsFactory from the Content Service to allow
  // Assistant to display embedded web contents.
  void GetNavigableContentsFactory(
      content::mojom::NavigableContentsFactoryRequest request);

  AssistantCacheController* cache_controller() {
    DCHECK(assistant_cache_controller_);
    return assistant_cache_controller_.get();
  }

  AssistantInteractionController* interaction_controller() {
    DCHECK(assistant_interaction_controller_);
    return assistant_interaction_controller_.get();
  }

  AssistantNotificationController* notification_controller() {
    DCHECK(assistant_notification_controller_);
    return assistant_notification_controller_.get();
  }

  AssistantScreenContextController* screen_context_controller() {
    DCHECK(assistant_screen_context_controller_);
    return assistant_screen_context_controller_.get();
  }

  AssistantSetupController* setup_controller() {
    DCHECK(assistant_setup_controller_);
    return assistant_setup_controller_.get();
  }

  AssistantUiController* ui_controller() {
    DCHECK(assistant_ui_controller_);
    return assistant_ui_controller_.get();
  }

  base::WeakPtr<AssistantController> GetWeakPtr();

 private:
  void NotifyConstructed();
  void NotifyDestroying();
  void NotifyDeepLinkReceived(const GURL& deep_link);
  void NotifyUrlOpened(const GURL& url, bool from_server);

  // mojom::VoiceInteractionObserver:
  void OnVoiceInteractionStatusChanged(
      mojom::VoiceInteractionState state) override;

  // The observer list should be initialized early so that sub-controllers may
  // register as observers during their construction.
  base::ObserverList<AssistantControllerObserver> observers_;

  mojo::BindingSet<mojom::AssistantController> assistant_controller_bindings_;

  mojo::Binding<mojom::AssistantVolumeControl>
      assistant_volume_control_binding_;
  mojo::InterfacePtrSet<mojom::VolumeObserver> volume_observer_;

  chromeos::assistant::mojom::AssistantPtr assistant_;

  mojom::AssistantImageDownloaderPtr assistant_image_downloader_;

  std::unique_ptr<AssistantCacheController> assistant_cache_controller_;

  std::unique_ptr<AssistantInteractionController>
      assistant_interaction_controller_;

  std::unique_ptr<AssistantNotificationController>
      assistant_notification_controller_;

  std::unique_ptr<AssistantScreenContextController>
      assistant_screen_context_controller_;

  std::unique_ptr<AssistantSetupController> assistant_setup_controller_;

  std::unique_ptr<AssistantUiController> assistant_ui_controller_;

  base::WeakPtrFactory<AssistantController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_CONTROLLER_H_
