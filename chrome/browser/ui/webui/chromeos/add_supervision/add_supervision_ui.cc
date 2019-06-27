// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_ui.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/views/chrome_web_dialog_view.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/confirm_signout_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace chromeos {

namespace {

constexpr int kDialogHeightPx = 608;
constexpr int kDialogWidthPx = 768;
// Id of System Dialog used to show the Add Supervision flow.
std::string& GetDialogId() {
  static base::NoDestructor<std::string> dialog_id;
  return *dialog_id;
}

// Shows the dialog indicating that user has to sign out if supervision has been
// enabled for their account.  Returns a boolean indicating whether the
// ConfirmSignoutDialog is being shown.
bool MaybeShowConfirmSignoutDialog() {
  SupervisedUserService* service = SupervisedUserServiceFactory::GetForProfile(
      ProfileManager::GetPrimaryUserProfile());
  if (service->signout_required_after_supervision_enabled()) {
    ConfirmSignoutDialog::Show();
    return true;
  }
  return false;
}

const char kAddSupervisionURL[] =
    "https://families.google.com/supervision/setup";
const char kAddSupervisionEventOriginFilter[] = "https://families.google.com";
const char kAddSupervisionFlowType[] = "1";

}  // namespace

// AddSupervisionDialog implementations.

// static
void AddSupervisionDialog::Show(gfx::NativeView parent) {
  // Get the system singleton instance of the AddSupervisionDialog.
  SystemWebDialogDelegate* current_instance = GetInstance();
  if (current_instance) {
    // Focus the dialog if it is already there.  Currently, this is
    // effectively a no-op, since the dialog is system-modal, but
    // it's here nonethless so that if the dialog becomes non-modal
    // at some point, the correct focus behavior occurs.
    current_instance->Focus();
    return;
  }
  // Note:  |current_instance|'s memory is freed when
  // SystemWebDialogDelegate::OnDialogClosed() is called.
  current_instance = new AddSupervisionDialog();

  GetDialogId() = current_instance->Id();

  current_instance->ShowSystemDialogForBrowserContext(
      ProfileManager::GetPrimaryUserProfile(), parent);
}

// static
void AddSupervisionDialog::Close() {
  SystemWebDialogDelegate* current_instance = GetInstance();
  if (current_instance) {
    current_instance->Close();
    GetDialogId() = std::string();
  }
}

ui::ModalType AddSupervisionDialog::GetDialogModalType() const {
  return ui::ModalType::MODAL_TYPE_WINDOW;
}

void AddSupervisionDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidthPx, kDialogHeightPx);
}

bool AddSupervisionDialog::OnDialogCloseRequested() {
  bool showing_confirm_dialog = MaybeShowConfirmSignoutDialog();
  return !showing_confirm_dialog;
}

void AddSupervisionDialog::OnCloseContents(content::WebContents* source,
                                           bool* out_close_dialog) {
  // This code gets called by a different path that OnDialogCloseRequested(),
  // and actually masks the call to OnDialogCloseRequested() the first time the
  // user clicks on the [x].  Because the first [x] click comes here, we need to
  // show the confirmation dialog here and signal the caller to possibly close
  // the dialog.  Subsequent clicks on [x] during the lifetime of the dialog
  // will result in calls to OnDialogCloseRequested().
  *out_close_dialog = OnDialogCloseRequested();
}

AddSupervisionDialog::AddSupervisionDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIAddSupervisionURL),
                              base::string16()) {}

AddSupervisionDialog::~AddSupervisionDialog() = default;

// static
SystemWebDialogDelegate* AddSupervisionDialog::GetInstance() {
  return SystemWebDialogDelegate::FindInstance(GetDialogId());
}

// AddSupervisionUI implementations.

AddSupervisionUI::AddSupervisionUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Register the Mojo API handler.
  AddHandlerToRegistry(base::BindRepeating(
      &AddSupervisionUI::BindAddSupervisionHandler, base::Unretained(this)));

  // Set up the basic page framework.
  SetupResources();
}

void AddSupervisionUI::SetupResources() {
  Profile* profile = Profile::FromWebUI(web_ui());
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIAddSupervisionHost));

  source->AddResourcePath("post_message_api.js",
                          IDR_ADD_SUPERVISION_POST_MESSAGE_API_JS);
  source->AddResourcePath("add_supervision_api_server.js",
                          IDR_ADD_SUPERVISION_API_SERVER_JS);
  source->AddResourcePath("add_supervision.js", IDR_ADD_SUPERVISION_JS);
  source->AddLocalizedString("pageTitle", IDS_ADD_SUPERVISION_PAGE_TITLE);

  // Full paths (relative to src) are important for Mojom generated files.
  source->AddResourcePath(
      "chrome/browser/ui/webui/chromeos/add_supervision/"
      "add_supervision.mojom-lite.js",
      IDR_ADD_SUPERVISION_MOJOM_LITE_JS);

  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_ADD_SUPERVISION_HTML);
  source->AddString("webviewUrl", kAddSupervisionURL);
  source->AddString("eventOriginFilter", kAddSupervisionEventOriginFilter);
  source->AddString("platformVersion", base::SysInfo::OperatingSystemVersion());
  source->AddString("flowType", kAddSupervisionFlowType);

  content::WebUIDataSource::Add(profile, source.release());
}

AddSupervisionUI::~AddSupervisionUI() = default;

bool AddSupervisionUI::CloseDialog() {
  bool showing_confirm_dialog = MaybeShowConfirmSignoutDialog();
  if (!showing_confirm_dialog) {
    // We aren't showing the confirm dialog, so close the AddSupervisionDialog.
    AddSupervisionDialog::Close();
  }
  return !showing_confirm_dialog;
}

void AddSupervisionUI::BindAddSupervisionHandler(
    add_supervision::mojom::AddSupervisionHandlerRequest request) {
  mojo_api_handler_ = std::make_unique<AddSupervisionHandler>(
      std::move(request), web_ui(), this);
}

}  // namespace chromeos
