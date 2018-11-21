// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_accessory_controller_impl.h"

#include <utility>

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/preferences/preferences_launcher.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/password_manager/password_accessory_metrics_util.h"
#include "chrome/browser/password_manager/password_generation_dialog_view_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

using autofill::AccessorySheetData;
using autofill::FooterCommand;
using autofill::PasswordForm;
using autofill::UserInfo;

namespace {

void RecordGenerationDialogDismissal(bool accepted) {
  UMA_HISTOGRAM_BOOLEAN("KeyboardAccessory.GeneratedPasswordDialog", accepted);
}

}  // namespace

PasswordAccessoryControllerImpl::~PasswordAccessoryControllerImpl() = default;

// static
bool PasswordAccessoryController::AllowedForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  if (vr::VrTabHelper::IsInVr(web_contents)) {
    return false;  // TODO(crbug.com/865749): Reenable if works for VR keyboard.
  }
  // Either #passwords-keyboards-accessory or #experimental-ui must be enabled.
  return base::FeatureList::IsEnabled(
             password_manager::features::kPasswordsKeyboardAccessory) ||
         base::FeatureList::IsEnabled(features::kExperimentalUi);
}

// static
PasswordAccessoryController* PasswordAccessoryController::GetOrCreate(
    content::WebContents* web_contents) {
  DCHECK(PasswordAccessoryController::AllowedForWebContents(web_contents));

  PasswordAccessoryControllerImpl::CreateForWebContents(web_contents);
  return PasswordAccessoryControllerImpl::FromWebContents(web_contents);
}

// static
PasswordAccessoryController* PasswordAccessoryController::GetIfExisting(
    content::WebContents* web_contents) {
  return PasswordAccessoryControllerImpl::FromWebContents(web_contents);
}

struct PasswordAccessoryControllerImpl::GenerationElementData {
  GenerationElementData(autofill::PasswordForm form,
                        autofill::FormSignature form_signature,
                        autofill::FieldSignature field_signature,
                        uint32_t max_password_length)
      : form(std::move(form)),
        form_signature(form_signature),
        field_signature(field_signature),
        max_password_length(max_password_length) {}

  // Form for which password generation is triggered.
  autofill::PasswordForm form;

  // Signature of the form for which password generation is triggered.
  autofill::FormSignature form_signature;

  // Signature of the field for which password generation is triggered.
  autofill::FieldSignature field_signature;

  // Maximum length of the generated password.
  uint32_t max_password_length;
};

struct PasswordAccessoryControllerImpl::SuggestionElementData {
  SuggestionElementData(base::string16 password,
                        base::string16 username,
                        bool username_selectable)
      : password(password),
        username(username),
        username_selectable(username_selectable) {}

  // Password string to be used for this credential.
  base::string16 password;

  // Username string to be used for this credential.
  base::string16 username;

  // Decides whether the username is interactive (i.e. empty ones are not).
  bool username_selectable;
};

struct PasswordAccessoryControllerImpl::FaviconRequestData {
  // List of requests waiting for favicons to be available.
  std::vector<base::OnceCallback<void(const gfx::Image&)>> pending_requests;

  // Cached image for this origin. |IsEmpty()| unless a favicon was found.
  gfx::Image cached_icon;
};

// static
void PasswordAccessoryControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    CreateDialogFactory create_dialog_factory,
    favicon::FaviconService* favicon_service) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(mf_controller);

  web_contents->SetUserData(
      UserDataKey(), base::WrapUnique(new PasswordAccessoryControllerImpl(
                         web_contents, std::move(mf_controller),
                         create_dialog_factory, favicon_service)));
}

void PasswordAccessoryControllerImpl::SavePasswordsForOrigin(
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const url::Origin& origin) {
  std::vector<SuggestionElementData>* suggestions =
      &origin_suggestions_[origin];
  suggestions->clear();
  for (const auto& pair : best_matches) {
    const PasswordForm* form = pair.second;
    suggestions->emplace_back(form->password_value, GetDisplayUsername(*form),
                              /*username_selectable*/ false);
  }
}

void PasswordAccessoryControllerImpl::OnAutomaticGenerationStatusChanged(
    bool available,
    const base::Optional<
        autofill::password_generation::PasswordGenerationUIData>& ui_data,
    const base::WeakPtr<password_manager::PasswordManagerDriver>& driver) {
  target_frame_driver_ = driver;
  if (available) {
    DCHECK(ui_data.has_value());
    generation_element_data_ = std::make_unique<GenerationElementData>(
        ui_data.value().password_form,
        autofill::CalculateFormSignature(
            ui_data.value().password_form.form_data),
        autofill::CalculateFieldSignatureByNameAndType(
            ui_data.value().generation_element, "password"),
        ui_data.value().max_length);
  } else {
    generation_element_data_.reset();
  }

  GetManualFillingController()->OnAutomaticGenerationStatusChanged(available);
}

void PasswordAccessoryControllerImpl::OnFilledIntoFocusedField(
    autofill::FillingStatus status) {
  GetManualFillingController()->OnFilledIntoFocusedField(status);
}

void PasswordAccessoryControllerImpl::OnOptionSelected(
    const base::string16& selected_option) const {
  // TODO(crbug.com/905669): This shouldn't rely on the selection name and
  // metrics::AccessoryAction shouldn't be password-specific.
  if (selected_option ==
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK)) {
    UMA_HISTOGRAM_ENUMERATION("KeyboardAccessory.AccessoryActionSelected",
                              metrics::AccessoryAction::MANAGE_PASSWORDS,
                              metrics::AccessoryAction::COUNT);
    chrome::android::PreferencesLauncher::ShowPasswordSettings(web_contents_);
  }
}

void PasswordAccessoryControllerImpl::RefreshSuggestionsForField(
    const url::Origin& origin,
    bool is_fillable,
    bool is_password_field) {
  current_origin_ = is_fillable ? origin : url::Origin();
  GetManualFillingController()->RefreshSuggestionsForField(
      is_fillable, CreateAccessorySheetData(
                       origin,
                       is_fillable ? origin_suggestions_[origin]
                                   : std::vector<SuggestionElementData>(),
                       is_password_field));
}

void PasswordAccessoryControllerImpl::DidNavigateMainFrame() {
  favicon_tracker_.TryCancelAll();  // If there is a request pending, cancel it.
  current_origin_ = url::Origin();
  icons_request_data_.clear();
  origin_suggestions_.clear();
}

void PasswordAccessoryControllerImpl::ShowWhenKeyboardIsVisible() {
  GetManualFillingController()->ShowWhenKeyboardIsVisible();
}

void PasswordAccessoryControllerImpl::Hide() {
  GetManualFillingController()->Hide();
}

void PasswordAccessoryControllerImpl::GetFavicon(
    int desired_size_in_pixel,
    base::OnceCallback<void(const gfx::Image&)> icon_callback) {
  url::Origin origin = current_origin_;  // Copy origin in case it changes.
  // Check whether this request can be immediately answered with a cached icon.
  // It is empty if there wasn't at least one request that found an icon yet.
  FaviconRequestData* icon_request = &icons_request_data_[origin];
  if (!icon_request->cached_icon.IsEmpty()) {
    std::move(icon_callback).Run(icon_request->cached_icon);
    return;
  }
  if (!favicon_service_) {  // This might happen in tests.
    std::move(icon_callback).Run(gfx::Image());
    return;
  }

  // The cache is empty. Queue the callback.
  icon_request->pending_requests.emplace_back(std::move(icon_callback));
  if (icon_request->pending_requests.size() > 1)
    return;  // The favicon for this origin was already requested.

  favicon_service_->GetRawFaviconForPageURL(
      origin.GetURL(), {favicon_base::IconType::kFavicon},
      desired_size_in_pixel,
      /* fallback_to_host = */ false,
      base::BindRepeating(  // FaviconService doesn't support BindOnce yet.
          &PasswordAccessoryControllerImpl::OnImageFetched,
          base::AsWeakPtr<PasswordAccessoryControllerImpl>(this), origin),
      &favicon_tracker_);
}

void PasswordAccessoryControllerImpl::OnFillingTriggered(
    bool is_password,
    const base::string16& text_to_fill) {
  password_manager::ContentPasswordManagerDriverFactory* factory =
      password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
          web_contents_);
  DCHECK(factory);
  // TODO(fhorschig): Consider allowing filling on non-main frames.
  password_manager::ContentPasswordManagerDriver* driver =
      factory->GetDriverForFrame(web_contents_->GetMainFrame());
  if (!driver) {
    return;
  }  // |driver| can be NULL if the tab is being closed.
  driver->FillIntoFocusedField(
      is_password, text_to_fill,
      base::BindOnce(&PasswordAccessoryControllerImpl::OnFilledIntoFocusedField,
                     base::AsWeakPtr<PasswordAccessoryControllerImpl>(this)));
}

void PasswordAccessoryControllerImpl::OnGenerationRequested() {
  if (!target_frame_driver_)
    return;
  // TODO(crbug.com/835234): Take the modal dialog logic out of the accessory
  // controller.
  dialog_view_ = create_dialog_factory_.Run(this);
  uint32_t spec_priority = 0;
  base::string16 password =
      target_frame_driver_->GetPasswordGenerationManager()->GeneratePassword(
          web_contents_->GetLastCommittedURL().GetOrigin(),
          generation_element_data_->form_signature,
          generation_element_data_->field_signature,
          generation_element_data_->max_password_length, &spec_priority);
  if (target_frame_driver_ && target_frame_driver_->GetPasswordManager()) {
    target_frame_driver_->GetPasswordManager()
        ->ReportSpecPriorityForGeneratedPassword(generation_element_data_->form,
                                                 spec_priority);
  }
  dialog_view_->Show(password);
}

void PasswordAccessoryControllerImpl::GeneratedPasswordAccepted(
    const base::string16& password) {
  if (!target_frame_driver_)
    return;
  RecordGenerationDialogDismissal(true);
  target_frame_driver_->GeneratedPasswordAccepted(password);
  dialog_view_.reset();
}

void PasswordAccessoryControllerImpl::GeneratedPasswordRejected() {
  RecordGenerationDialogDismissal(false);
  dialog_view_.reset();
}

gfx::NativeWindow PasswordAccessoryControllerImpl::native_window() const {
  return web_contents_->GetTopLevelNativeWindow();
}

PasswordAccessoryControllerImpl::PasswordAccessoryControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      create_dialog_factory_(
          base::BindRepeating(&PasswordGenerationDialogViewInterface::Create)),
      favicon_service_(FaviconServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          ServiceAccessType::EXPLICIT_ACCESS)) {
}

// Additional creation functions in unit tests only:
PasswordAccessoryControllerImpl::PasswordAccessoryControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    CreateDialogFactory create_dialog_factory,
    favicon::FaviconService* favicon_service)
    : web_contents_(web_contents),
      mf_controller_(std::move(mf_controller)),
      create_dialog_factory_(create_dialog_factory),
      favicon_service_(favicon_service) {}

// static
AccessorySheetData PasswordAccessoryControllerImpl::CreateAccessorySheetData(
    const url::Origin& origin,
    const std::vector<SuggestionElementData>& suggestions,
    bool is_password_field) {
  // Create the title element
  base::string16 passwords_title_str = l10n_util::GetStringFUTF16(
      suggestions.empty()
          ? IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE
          : IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE,
      base::ASCIIToUTF16(origin.host()));
  AccessorySheetData data(passwords_title_str);

  // Create a username and a password element for every suggestion.
  for (const SuggestionElementData& suggestion : suggestions) {
    UserInfo user_info;

    user_info.add_field(UserInfo::Field(
        suggestion.username, suggestion.username, /*is_password=*/false,
        /*selectable=*/suggestion.username_selectable));

    user_info.add_field(UserInfo::Field(
        suggestion.password,
        l10n_util::GetStringFUTF16(
            IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION,
            suggestion.username),
        /*is_password=*/true, /*selectable=*/is_password_field));

    data.add_user_info(std::move(user_info));
  }

  // Create the link to all passwords.
  base::string16 manage_passwords_title = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK);
  data.add_footer_command(FooterCommand(manage_passwords_title));

  return data;
}

void PasswordAccessoryControllerImpl::OnImageFetched(
    url::Origin origin,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  FaviconRequestData* icon_request = &icons_request_data_[origin];

  favicon_base::FaviconImageResult image_result;
  if (bitmap_result.is_valid()) {
    image_result.image = gfx::Image::CreateFrom1xPNGBytes(
        bitmap_result.bitmap_data->front(), bitmap_result.bitmap_data->size());
  }
  icon_request->cached_icon = image_result.image;
  // Only trigger all the callbacks if they still affect a displayed origin.
  if (origin == current_origin_) {
    for (auto& callback : icon_request->pending_requests) {
      std::move(callback).Run(icon_request->cached_icon);
    }
  }
  icon_request->pending_requests.clear();
}

base::WeakPtr<ManualFillingController>
PasswordAccessoryControllerImpl::GetManualFillingController() {
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(web_contents_);
  DCHECK(mf_controller_);
  return mf_controller_;
}
