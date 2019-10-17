// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/manifest_update_task.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/common/web_application_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace web_app {

ManifestUpdateTask::ManifestUpdateTask(const GURL& url,
                                       const AppId& app_id,
                                       content::WebContents* web_contents,
                                       StoppedCallback stopped_callback,
                                       bool hang_for_testing,
                                       const AppRegistrar& registrar,
                                       WebAppUiManager* ui_manager,
                                       InstallManager* install_manager)
    : content::WebContentsObserver(web_contents),
      registrar_(registrar),
      ui_manager_(*ui_manager),
      install_manager_(*install_manager),
      url_(url),
      app_id_(app_id),
      stopped_callback_(std::move(stopped_callback)),
      hang_for_testing_(hang_for_testing) {
  // Task starts by waiting for DidFinishLoad() to be called.
  stage_ = Stage::kPendingPageLoad;
}

ManifestUpdateTask::~ManifestUpdateTask() {
#if DCHECK_IS_ON()
  if (destructor_called_ptr_) {
    DCHECK(!(*destructor_called_ptr_));
    *destructor_called_ptr_ = true;
  }
#endif
}

// content::WebContentsObserver:
void ManifestUpdateTask::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (stage_ != Stage::kPendingPageLoad || hang_for_testing_)
    return;

  if (render_frame_host->GetParent() != nullptr)
    return;

  stage_ = Stage::kPendingInstallableData;
  InstallableParams params;
  params.valid_primary_icon = true;
  params.valid_manifest = true;
  InstallableManager::FromWebContents(web_contents())
      ->GetData(params, base::Bind(&ManifestUpdateTask::OnDidGetInstallableData,
                                   AsWeakPtr()));
}

// content::WebContentsObserver:
void ManifestUpdateTask::WebContentsDestroyed() {
  switch (stage_) {
    case Stage::kPendingPageLoad:
    case Stage::kPendingInstallableData:
      DestroySelf(ManifestUpdateResult::kWebContentsDestroyed);
      return;
    case Stage::kPendingWindowsClosed:
    case Stage::kPendingInstallation:
      NOTREACHED();
      Observe(nullptr);
      break;
  }
}

void ManifestUpdateTask::OnDidGetInstallableData(const InstallableData& data) {
  DCHECK_EQ(stage_, Stage::kPendingInstallableData);

  if (!data.errors.empty()) {
    DestroySelf(ManifestUpdateResult::kAppDataInvalid);
    return;
  }

  DCHECK(data.manifest);
  if (!IsUpdateNeededForManifest(*data.manifest)) {
    DestroySelf(ManifestUpdateResult::kAppUpToDate);
    return;
  }

  stage_ = Stage::kPendingWindowsClosed;
  Observe(nullptr);
  ui_manager_.NotifyOnAllAppWindowsClosed(
      app_id_, base::Bind(&ManifestUpdateTask::OnAllAppWindowsClosed,
                          AsWeakPtr(), *data.manifest));
}

bool ManifestUpdateTask::IsUpdateNeededForManifest(
    const blink::Manifest& manifest) const {
  if (app_id_ != GenerateAppIdFromURL(manifest.start_url))
    return false;

  if (manifest.theme_color != registrar_.GetAppThemeColor(app_id_))
    return true;

  // TODO(crbug.com/926083): Check more manifest fields.
  return false;
}

void ManifestUpdateTask::OnAllAppWindowsClosed(blink::Manifest manifest) {
  DCHECK_EQ(stage_, Stage::kPendingWindowsClosed);

  auto web_application_info = std::make_unique<WebApplicationInfo>();
  UpdateWebAppInfoFromManifest(manifest, web_application_info.get(),
                               ForInstallableSite::kYes);

  // The app's name must not change due to an automatic update.
  web_application_info->title =
      base::UTF8ToUTF16(registrar_.GetAppShortName(app_id_));

  // Preserve the user's choice of launch container (excluding fullscreen).
  switch (registrar_.GetAppDisplayMode(app_id_)) {
    case blink::mojom::DisplayMode::kBrowser:
      web_application_info->open_as_window = false;
      break;
    case blink::mojom::DisplayMode::kMinimalUi:
    case blink::mojom::DisplayMode::kStandalone:
    case blink::mojom::DisplayMode::kFullscreen:
      web_application_info->open_as_window = true;
      break;
    case blink::mojom::DisplayMode::kUndefined:
      NOTREACHED();
      break;
  }

  stage_ = Stage::kPendingInstallation;
  install_manager_.UpdateWebAppFromInfo(
      app_id_, std::move(web_application_info),
      base::Bind(&ManifestUpdateTask::OnInstallationComplete, AsWeakPtr(),
                 std::move(manifest)));
}

void ManifestUpdateTask::OnInstallationComplete(blink::Manifest manifest,
                                                const AppId& app_id,
                                                InstallResultCode code) {
  DCHECK_EQ(stage_, Stage::kPendingInstallation);

  if (!IsSuccess(code)) {
    DestroySelf(ManifestUpdateResult::kAppUpdateFailed);
    return;
  }

  DCHECK_EQ(app_id_, app_id);
  DCHECK(!IsUpdateNeededForManifest(manifest));
  DCHECK_EQ(code, InstallResultCode::kSuccessAlreadyInstalled);

  DestroySelf(ManifestUpdateResult::kAppUpdated);
}

void ManifestUpdateTask::DestroySelf(ManifestUpdateResult result) {
#if DCHECK_IS_ON()
  bool destructor_called = false;
  destructor_called_ptr_ = &destructor_called;
#endif
  std::move(stopped_callback_).Run(*this, result);
#if DCHECK_IS_ON()
  DCHECK(destructor_called);
#endif
}

}  // namespace web_app
