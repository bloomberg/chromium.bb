// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"

#include <map>
#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/post_task.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/signin/core/browser/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "jni/AssistantCarouselModel_jni.h"
#include "jni/AssistantDetailsModel_jni.h"
#include "jni/AssistantDetails_jni.h"
#include "jni/AssistantHeaderModel_jni.h"
#include "jni/AssistantModel_jni.h"
#include "jni/AutofillAssistantUiController_jni.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace autofill_assistant {

UiControllerAndroid::UiControllerAndroid(content::WebContents* web_contents,
                                         Client* client,
                                         UiDelegate* ui_delegate)
    : client_(client),
      ui_delegate_(ui_delegate),
      header_delegate_(this),
      carousel_delegate_(this),
      weak_ptr_factory_(this) {
  DCHECK(web_contents);
  DCHECK(client);
  DCHECK(ui_delegate);
  JNIEnv* env = AttachCurrentThread();
  java_autofill_assistant_ui_controller_ =
      Java_AutofillAssistantUiController_createAndStartUi(
          env, web_contents->GetJavaWebContents(),
          reinterpret_cast<intptr_t>(this));

  // Register header_delegate_ as delegate for clicks on header buttons.
  Java_AssistantHeaderModel_setDelegate(env, GetHeaderModel(),
                                        header_delegate_.GetJavaObject());
}

UiControllerAndroid::~UiControllerAndroid() {
  Java_AutofillAssistantUiController_clearNativePtr(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

base::android::ScopedJavaLocalRef<jobject> UiControllerAndroid::GetModel() {
  return Java_AutofillAssistantUiController_getModel(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

// Header related methods.

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetHeaderModel() {
  return Java_AssistantModel_getHeaderModel(AttachCurrentThread(), GetModel());
}

void UiControllerAndroid::ShowStatusMessage(const std::string& message) {
  if (!message.empty()) {
    JNIEnv* env = AttachCurrentThread();
    Java_AssistantHeaderModel_setStatusMessage(
        env, GetHeaderModel(),
        base::android::ConvertUTF8ToJavaString(env, message));
  }
}

std::string UiControllerAndroid::GetStatusMessage() {
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> message =
      Java_AssistantHeaderModel_getStatusMessage(env, GetHeaderModel());
  std::string status;
  base::android::ConvertJavaStringToUTF8(env, message.obj(), &status);
  return status;
}

void UiControllerAndroid::ShowProgressBar(int progress,
                                          const std::string& message) {
  ShowStatusMessage(message);
  // TODO(crbug.com/806868): Get progress first and call setProgress only if
  // progress > current_progress, and remove that logic from
  // AnimatedProgressBar.
  Java_AssistantHeaderModel_setProgress(AttachCurrentThread(), GetHeaderModel(),
                                        progress);
}

void UiControllerAndroid::HideProgressBar() {
  // TODO(crbug.com/806868): Remove calls to this function.
}

void UiControllerAndroid::SetProgressPulsingEnabled(bool enabled) {
  Java_AssistantHeaderModel_setProgressPulsingEnabled(
      AttachCurrentThread(), GetHeaderModel(), enabled);
}

void UiControllerAndroid::OnFeedbackButtonClicked() {
  JNIEnv* env = AttachCurrentThread();
  auto jdetails = Java_AssistantDetailsModel_getDetails(env, GetDetailsModel());
  auto jstatus_message =
      Java_AssistantHeaderModel_getStatusMessage(env, GetHeaderModel());
  Java_AutofillAssistantUiController_showFeedback(
      env, java_autofill_assistant_ui_controller_,
      base::android::ConvertUTF8ToJavaString(env, GetDebugContext()), jdetails,
      jstatus_message);
}

void UiControllerAndroid::OnCloseButtonClicked() {
  JNIEnv* env = AttachCurrentThread();
  // TODO(crbug.com/806868): Move here the logic that shutdowns immediately if
  // close button is clicked during graceful shutdown.
  Java_AutofillAssistantUiController_dismissAndShowSnackbar(
      env, java_autofill_assistant_ui_controller_,
      base::android::ConvertUTF8ToJavaString(
          env, l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_STOPPED)));
}

std::string UiControllerAndroid::GetDebugContext() {
  return ui_delegate_->GetDebugContext();
}

// Carousel related methods.

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetCarouselModel() {
  return Java_AssistantModel_getCarouselModel(AttachCurrentThread(),
                                              GetModel());
}

void UiControllerAndroid::SetChips(std::unique_ptr<std::vector<Chip>> chips) {
  DCHECK(chips);
  current_chips_ = std::move(chips);

  int types[current_chips_->size()];
  std::vector<std::string> texts;
  int i = 0;
  for (const auto& chip : *current_chips_) {
    types[i++] = chip.type;
    texts.emplace_back(chip.text);
  }

  JNIEnv* env = AttachCurrentThread();
  Java_AssistantCarouselModel_setChips(
      env, GetCarouselModel(),
      base::android::ToJavaIntArray(env, types, current_chips_->size()),
      base::android::ToJavaArrayOfStrings(env, texts),
      carousel_delegate_.GetJavaObject());
}

void UiControllerAndroid::ClearChips() {
  current_chips_.reset();
  Java_AssistantCarouselModel_clearChips(AttachCurrentThread(),
                                         GetCarouselModel());
}

void UiControllerAndroid::OnChipSelected(int index) {
  if (current_chips_ && index >= 0 && index < (int)current_chips_->size()) {
    auto callback = std::move((*current_chips_)[index].callback);
    current_chips_.reset();
    std::move(callback).Run();
  }
}

// Other methods.

void UiControllerAndroid::ShowOnboarding(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& on_accept) {
  Java_AutofillAssistantUiController_onShowOnboarding(
      env, java_autofill_assistant_ui_controller_, on_accept);
}

void UiControllerAndroid::ShowOverlay() {
  Java_AutofillAssistantUiController_onShowOverlay(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
  SetProgressPulsingEnabled(false);
}

void UiControllerAndroid::HideOverlay() {
  Java_AutofillAssistantUiController_onHideOverlay(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);

  // Hiding the overlay generally means that the user is expected to interact
  // with the page, so we enable progress bar pulsing animation.
  SetProgressPulsingEnabled(true);
}

void UiControllerAndroid::AllowShowingSoftKeyboard(bool enabled) {
  Java_AutofillAssistantUiController_onAllowShowingSoftKeyboard(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_, enabled);
}

void UiControllerAndroid::Shutdown() {
  Java_AutofillAssistantUiController_onShutdown(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::ShutdownGracefully() {
  Java_AutofillAssistantUiController_onShutdownGracefully(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::Close() {
  Java_AutofillAssistantUiController_onClose(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::UpdateTouchableArea(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  ui_delegate_->UpdateTouchableArea();
}

void UiControllerAndroid::OnUserInteractionInsideTouchableArea(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcallerj) {
  ui_delegate_->OnUserInteractionInsideTouchableArea();
}

void UiControllerAndroid::OnGetPaymentInformation(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jboolean jsucceed,
    const JavaParamRef<jobject>& jcard,
    const JavaParamRef<jobject>& jaddress,
    const JavaParamRef<jstring>& jpayer_name,
    const JavaParamRef<jstring>& jpayer_phone,
    const JavaParamRef<jstring>& jpayer_email,
    jboolean jis_terms_and_services_accepted) {
  DCHECK(get_payment_information_callback_);
  SetProgressPulsingEnabled(false);

  std::unique_ptr<PaymentInformation> payment_info =
      std::make_unique<PaymentInformation>();
  payment_info->succeed = jsucceed;
  payment_info->is_terms_and_conditions_accepted =
      jis_terms_and_services_accepted;
  if (payment_info->succeed) {
    if (jcard != nullptr) {
      payment_info->card = std::make_unique<autofill::CreditCard>();
      autofill::PersonalDataManagerAndroid::PopulateNativeCreditCardFromJava(
          jcard, env, payment_info->card.get());

      auto guid = payment_info->card->billing_address_id();
      if (!guid.empty()) {
        autofill::AutofillProfile* profile =
            autofill::PersonalDataManagerFactory::GetForProfile(
                ProfileManager::GetLastUsedProfile())
                ->GetProfileByGUID(guid);
        if (profile != nullptr)
          payment_info->billing_address =
              std::make_unique<autofill::AutofillProfile>(*profile);
      }
    }
    if (jaddress != nullptr) {
      payment_info->shipping_address =
          std::make_unique<autofill::AutofillProfile>();
      autofill::PersonalDataManagerAndroid::PopulateNativeProfileFromJava(
          jaddress, env, payment_info->shipping_address.get());
    }
    if (jpayer_name != nullptr) {
      base::android::ConvertJavaStringToUTF8(env, jpayer_name,
                                             &payment_info->payer_name);
    }
    if (jpayer_phone != nullptr) {
      base::android::ConvertJavaStringToUTF8(env, jpayer_phone,
                                             &payment_info->payer_phone);
    }
    if (jpayer_email != nullptr) {
      base::android::ConvertJavaStringToUTF8(env, jpayer_email,
                                             &payment_info->payer_email);
    }
  }
  std::move(get_payment_information_callback_).Run(std::move(payment_info));
}

void UiControllerAndroid::GetPaymentInformation(
    payments::mojom::PaymentOptionsPtr payment_options,
    base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
    const std::string& title,
    const std::vector<std::string>& supported_basic_card_networks) {
  DCHECK(!get_payment_information_callback_);
  get_payment_information_callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  SetProgressPulsingEnabled(true);
  Java_AutofillAssistantUiController_onRequestPaymentInformation(
      env, java_autofill_assistant_ui_controller_,
      base::android::ConvertUTF8ToJavaString(env,
                                             client_->GetAccountEmailAddress()),
      payment_options->request_shipping, payment_options->request_payer_name,
      payment_options->request_payer_phone,
      payment_options->request_payer_email,
      static_cast<int>(payment_options->shipping_type),
      base::android::ConvertUTF8ToJavaString(env, title),
      base::android::ToJavaArrayOfStrings(env, supported_basic_card_networks));
}

// Details related method.

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetDetailsModel() {
  return Java_AssistantModel_getDetailsModel(AttachCurrentThread(), GetModel());
}

void UiControllerAndroid::HideDetails() {
  Java_AssistantDetailsModel_clearDetails(AttachCurrentThread(),
                                          GetDetailsModel());
}

void UiControllerAndroid::ShowInitialDetails(const std::string& title,
                                             const std::string& description,
                                             const std::string& mid,
                                             const std::string& date) {
  JNIEnv* env = AttachCurrentThread();
  auto jdetails = Java_AssistantDetails_createInitial(
      env, base::android::ConvertUTF8ToJavaString(env, title),
      base::android::ConvertUTF8ToJavaString(env, description),
      base::android::ConvertUTF8ToJavaString(env, mid),
      base::android::ConvertUTF8ToJavaString(env, date));
  Java_AssistantDetailsModel_setDetails(env, GetDetailsModel(), jdetails);
}

void UiControllerAndroid::ShowDetails(const ShowDetailsProto& show_details,
                                      base::OnceCallback<void(bool)> callback) {
  show_details_callback_ = std::move(callback);
  const DetailsChanges& change_flags = show_details.change_flags();
  ShowDetails(show_details, change_flags.user_approval_required(),
              change_flags.highlight_title(), change_flags.highlight_date());

  if (!change_flags.user_approval_required()) {
    std::move(show_details_callback_).Run(true);
    return;
  }

  // Ask for user approval.
  std::string previous_status_message = GetStatusMessage();
  ShowStatusMessage(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DETAILS_DIFFER));
  SetProgressPulsingEnabled(true);

  // Continue button.
  auto chips = std::make_unique<std::vector<Chip>>();
  chips->emplace_back();
  chips->back().text =
      l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_CONTINUE_BUTTON);
  chips->back().type = Chip::Type::BUTTON_FILLED_BLUE;
  chips->back().callback = base::BindOnce(
      &UiControllerAndroid::OnUserApproval, weak_ptr_factory_.GetWeakPtr(),
      show_details, previous_status_message, /* success= */ true);

  // Go back button.
  chips->emplace_back();
  chips->back().text =
      l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DETAILS_DIFFER_GO_BACK);
  chips->back().type = Chip::Type::BUTTON_TEXT;
  chips->back().callback = base::BindOnce(
      &UiControllerAndroid::OnUserApproval, weak_ptr_factory_.GetWeakPtr(),
      show_details, previous_status_message, /* success= */ false);

  SetChips(std::move(chips));
}

void UiControllerAndroid::OnUserApproval(
    const ShowDetailsProto& show_details,
    const std::string& previous_status_message,
    bool success) {
  if (success) {
    // Restore previous state.
    ShowStatusMessage(previous_status_message);
    SetProgressPulsingEnabled(false);

    // Show details without highlighted fields.
    ShowDetails(show_details, /* user_approval_required= */ false,
                /* highlight_title= */ false, /* highlight_date= */ false);
  }

  if (show_details_callback_) {
    std::move(show_details_callback_).Run(success);
  }
}

void UiControllerAndroid::ShowDetails(const ShowDetailsProto& show_details,
                                      bool user_approval_required,
                                      bool highlight_title,
                                      bool highlight_date) {
  const DetailsProto& details = show_details.details();
  int year = details.datetime().date().year();
  int month = details.datetime().date().month();
  int day = details.datetime().date().day();
  int hour = details.datetime().time().hour();
  int minute = details.datetime().time().minute();
  int second = details.datetime().time().second();

  JNIEnv* env = AttachCurrentThread();
  auto jdetails = Java_AssistantDetails_create(
      env, base::android::ConvertUTF8ToJavaString(env, details.title()),
      base::android::ConvertUTF8ToJavaString(env, details.url()),
      base::android::ConvertUTF8ToJavaString(env, details.description()),
      base::android::ConvertUTF8ToJavaString(env, details.m_id()),
      base::android::ConvertUTF8ToJavaString(env, details.total_price()), year,
      month, day, hour, minute, second, user_approval_required, highlight_title,
      highlight_date);
  Java_AssistantDetailsModel_setDetails(env, GetDetailsModel(), jdetails);
}

void UiControllerAndroid::Stop(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  client_->Stop();
}

void UiControllerAndroid::UpdateTouchableArea(bool enabled,
                                              const std::vector<RectF>& areas) {
  JNIEnv* env = AttachCurrentThread();
  std::vector<float> flattened;
  for (const auto& rect : areas) {
    flattened.emplace_back(rect.left);
    flattened.emplace_back(rect.top);
    flattened.emplace_back(rect.right);
    flattened.emplace_back(rect.bottom);
  }
  Java_AutofillAssistantUiController_updateTouchableArea(
      env, java_autofill_assistant_ui_controller_, enabled,
      base::android::ToJavaFloatArray(env, flattened));
  SetProgressPulsingEnabled(true);
}

void UiControllerAndroid::ExpandBottomSheet() {
  Java_AutofillAssistantUiController_expandBottomSheet(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}
}  // namespace autofill_assistant.
