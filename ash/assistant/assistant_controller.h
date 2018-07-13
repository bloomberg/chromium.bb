// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "ash/public/interfaces/assistant_image_downloader.mojom.h"
#include "ash/public/interfaces/assistant_setup.mojom.h"
#include "ash/public/interfaces/web_contents_manager.mojom.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/gfx/geometry/rect.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace ash {

class AssistantInteractionController;
class AssistantNotificationController;
class AssistantScreenContextController;
class AssistantUiController;

class ASH_EXPORT AssistantController
    : public mojom::AssistantController,
      public AssistantControllerObserver,
      public mojom::ManagedWebContentsOpenUrlDelegate {
 public:
  AssistantController();
  ~AssistantController() override;

  void BindRequest(mojom::AssistantControllerRequest request);

  // Adds/removes the specified |observer|.
  void AddObserver(AssistantControllerObserver* observer);
  void RemoveObserver(AssistantControllerObserver* observer);

  // Requests that WebContents, uniquely identified by |id_token|, be created
  // and managed according to the specified |params|. When the WebContents is
  // ready for embedding, the supplied |callback| is run with an embed token. In
  // the event that an error occurs, the supplied callback will still be run but
  // no embed token will be provided.
  void ManageWebContents(
      const base::UnguessableToken& id_token,
      mojom::ManagedWebContentsParamsPtr params,
      mojom::WebContentsManager::ManageWebContentsCallback callback);

  // Releases resources for the WebContents uniquely identified by |id_token|.
  void ReleaseWebContents(const base::UnguessableToken& id_token);

  // Releases resources for any WebContents uniquely identified in
  // |id_token_list|.
  void ReleaseWebContents(const std::vector<base::UnguessableToken>& id_tokens);

  // Downloads the image found at the specified |url|. On completion, the
  // supplied |callback| will be run with the downloaded image. If the download
  // attempt is unsuccessful, a NULL image is returned.
  void DownloadImage(
      const GURL& url,
      mojom::AssistantImageDownloader::DownloadCallback callback);

  // mojom::AssistantController:
  // TODO(updowndota): Refactor Set() calls to use a factory pattern.
  // TODO(dmblack): Expose RequestScreenshot(...) over mojo through
  // AssistantScreenContextController.
  void SetAssistant(
      chromeos::assistant::mojom::AssistantPtr assistant) override;
  void SetAssistantImageDownloader(
      mojom::AssistantImageDownloaderPtr assistant_image_downloader) override;
  void SetAssistantSetup(mojom::AssistantSetupPtr assistant_setup) override;
  void SetWebContentsManager(
      mojom::WebContentsManagerPtr web_contents_manager) override;
  void RequestScreenshot(const gfx::Rect& rect,
                         RequestScreenshotCallback callback) override;

  // AssistantControllerObserver:
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // mojom::ManagedWebContentsOpenUrlDelegate:
  void OnOpenUrlFromTab(const GURL& url) override;

  // Opens the specified |url| in a new browser tab.
  // TODO(dmblack): Support opening specific URLs in the Assistant container.
  void OpenUrl(const GURL& url);

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

  AssistantUiController* ui_controller() {
    DCHECK(assistant_ui_controller_);
    return assistant_ui_controller_.get();
  }

  base::WeakPtr<AssistantController> GetWeakPtr();

 private:
  void NotifyConstructed();
  void NotifyDestroying();
  void NotifyDeepLinkReceived(const GURL& deep_link);
  void NotifyUrlOpened(const GURL& url);

  // The observer list should be initialized early so that sub-controllers may
  // register as observers during their construction.
  base::ObserverList<AssistantControllerObserver> observers_;

  mojo::BindingSet<mojom::AssistantController> assistant_controller_bindings_;

  mojo::BindingSet<mojom::ManagedWebContentsOpenUrlDelegate>
      web_contents_open_url_delegate_bindings_;

  chromeos::assistant::mojom::AssistantPtr assistant_;

  mojom::AssistantImageDownloaderPtr assistant_image_downloader_;

  mojom::AssistantSetupPtr assistant_setup_;

  mojom::WebContentsManagerPtr web_contents_manager_;

  std::unique_ptr<AssistantInteractionController>
      assistant_interaction_controller_;

  std::unique_ptr<AssistantNotificationController>
      assistant_notification_controller_;

  std::unique_ptr<AssistantScreenContextController>
      assistant_screen_context_controller_;

  std::unique_ptr<AssistantUiController> assistant_ui_controller_;

  base::WeakPtrFactory<AssistantController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_CONTROLLER_H_
