// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/views/chrome_web_dialog_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "net/base/url_util.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

bool is_active = false;

constexpr int kAssistantOptInDialogWidth = 768;
constexpr int kAssistantOptInDialogHeight = 640;
constexpr char kFlowTypeParamKey[] = "flow-type";

GURL CreateAssistantOptInURL(ash::mojom::FlowType type) {
  // TODO(updowndota): Directly use mojom enum types in js.
  auto gurl = net::AppendOrReplaceQueryParameter(
      GURL(chrome::kChromeUIAssistantOptInURL), kFlowTypeParamKey,
      std::to_string(static_cast<int>(type)));
  return gurl;
}

void DisablePolymer2(content::URLDataSource* shared_source) {
  if (shared_source)
    shared_source->DisablePolymer2ForHost(chrome::kChromeUIAssistantOptInHost);
}

}  // namespace

AssistantOptInUI::AssistantOptInUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui), weak_factory_(this) {
  // Set up the chrome://assistant-optin source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAssistantOptInHost);

  auto assistant_handler = std::make_unique<AssistantOptInFlowScreenHandler>();
  auto* assistant_handler_ptr = assistant_handler.get();
  web_ui->AddMessageHandler(std::move(assistant_handler));
  assistant_handler_ptr->SetupAssistantConnection();

  base::DictionaryValue localized_strings;
  assistant_handler_ptr->GetLocalizedStrings(&localized_strings);
  source->AddLocalizedStrings(localized_strings);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("assistant_optin.js", IDR_ASSISTANT_OPTIN_JS);
  source->AddResourcePath("assistant_logo.png", IDR_ASSISTANT_LOGO_PNG);
  source->SetDefaultResource(IDR_ASSISTANT_OPTIN_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);

  // Do not zoom for Assistant opt-in web contents.
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetForWebContents(web_ui->GetWebContents());
  DCHECK(zoom_map);
  zoom_map->SetZoomLevelForHost(web_ui->GetWebContents()->GetURL().host(), 0);

  // If allowed, request that the shared resources send this page Polymer 1
  // resources instead of Polymer 2.
  // TODO (https://crbug.com/739611): Remove this exception by migrating to
  // Polymer 2.
  if (base::FeatureList::IsEnabled(features::kWebUIPolymer2Exceptions)) {
    content::URLDataSource::GetSourceForURL(
        Profile::FromWebUI(web_ui),
        GURL("chrome://resources/polymer/v1_0/polymer/polymer.html"),
        base::BindOnce(DisablePolymer2));
  }
}

AssistantOptInUI::~AssistantOptInUI() = default;

// AssistantOptInDialog

// static
void AssistantOptInDialog::Show(
    ash::mojom::FlowType type,
    ash::mojom::AssistantSetup::StartAssistantOptInFlowCallback callback) {
  DCHECK(!is_active);
  AssistantOptInDialog* dialog =
      new AssistantOptInDialog(type, std::move(callback));

  views::Widget::InitParams extra_params = ash_util::GetFramelessInitParams();
  chrome::ShowWebDialogWithParams(nullptr /* parent */,
                                  ProfileManager::GetActiveUserProfile(),
                                  dialog, &extra_params);
}

// static
bool AssistantOptInDialog::IsActive() {
  return is_active;
}

AssistantOptInDialog::AssistantOptInDialog(
    ash::mojom::FlowType type,
    ash::mojom::AssistantSetup::StartAssistantOptInFlowCallback callback)
    : SystemWebDialogDelegate(CreateAssistantOptInURL(type), base::string16()),
      callback_(std::move(callback)) {
  DCHECK(!is_active);
  is_active = true;
}

AssistantOptInDialog::~AssistantOptInDialog() {
  is_active = false;
}

void AssistantOptInDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kAssistantOptInDialogWidth, kAssistantOptInDialogHeight);
}

std::string AssistantOptInDialog::GetDialogArgs() const {
  return std::string();
}

bool AssistantOptInDialog::ShouldShowDialogTitle() const {
  return false;
}

void AssistantOptInDialog::OnDialogClosed(const std::string& json_retval) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  const bool completed =
      prefs->GetBoolean(arc::prefs::kVoiceInteractionEnabled) &&
      prefs->GetBoolean(arc::prefs::kVoiceInteractionActivityControlAccepted);
  std::move(callback_).Run(completed);
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

}  // namespace chromeos
