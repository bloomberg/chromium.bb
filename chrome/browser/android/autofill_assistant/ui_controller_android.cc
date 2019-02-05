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
#include "base/bind.h"
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
#include "components/autofill_assistant/browser/metrics.h"
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
#include "jni/AssistantOverlayModel_jni.h"
#include "jni/AssistantPaymentRequestModel_jni.h"
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
      overlay_delegate_(this),
      header_delegate_(this),
      payment_request_delegate_(this),
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

  // Register overlay_delegate_ as delegate for the overlay.
  Java_AssistantOverlayModel_setDelegate(env, GetOverlayModel(),
                                         overlay_delegate_.GetJavaObject());

  // Register header_delegate_ as delegate for clicks on header buttons.
  Java_AssistantHeaderModel_setDelegate(env, GetHeaderModel(),
                                        header_delegate_.GetJavaObject());

  // Register payment_request_delegate_ as delegate for the payment request UI.
  Java_AssistantPaymentRequestModel_setDelegate(
      env, GetPaymentRequestModel(), payment_request_delegate_.GetJavaObject());
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

void UiControllerAndroid::OnStateChanged(AutofillAssistantState new_state) {
  switch (new_state) {
    case AutofillAssistantState::STARTING:
      SetOverlayState(OverlayState::FULL);
      AllowShowingSoftKeyboard(false);
      SetProgressPulsingEnabled(false);
      return;

    case AutofillAssistantState::RUNNING:
      SetOverlayState(OverlayState::FULL);
      AllowShowingSoftKeyboard(false);
      SetProgressPulsingEnabled(false);
      return;

    case AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT:
      SetOverlayState(OverlayState::HIDDEN);
      AllowShowingSoftKeyboard(true);
      SetProgressPulsingEnabled(true);

      // user interaction is needed.
      ExpandBottomSheet();
      return;

    case AutofillAssistantState::PROMPT:
      SetOverlayState(OverlayState::PARTIAL);
      AllowShowingSoftKeyboard(true);
      SetProgressPulsingEnabled(true);

      // user interaction is needed.
      ExpandBottomSheet();
      return;

    case AutofillAssistantState::MODAL_DIALOG:
      SetOverlayState(OverlayState::FULL);
      AllowShowingSoftKeyboard(true);
      SetProgressPulsingEnabled(false);
      return;

    case AutofillAssistantState::STOPPED:
      SetOverlayState(OverlayState::HIDDEN);
      AllowShowingSoftKeyboard(true);
      SetProgressPulsingEnabled(false);

      // make sure user sees the error message.
      ExpandBottomSheet();
      ShutdownGracefully();
      return;

    case AutofillAssistantState::INACTIVE:
      // TODO(crbug.com/806868): Add support for switching back to the inactive
      // state, which is the initial state. We never enter it, so there should
      // never be a call OnStateChanged(INACTIVE)
      NOTREACHED() << "Switching to the inactive state is not supported.";
      return;
  }
  NOTREACHED() << "Unknown state: " << static_cast<int>(new_state);
}

void UiControllerAndroid::OnStatusMessageChanged(const std::string& message) {
  if (!message.empty()) {
    JNIEnv* env = AttachCurrentThread();
    Java_AssistantHeaderModel_setStatusMessage(
        env, GetHeaderModel(),
        base::android::ConvertUTF8ToJavaString(env, message));
  }
}

void UiControllerAndroid::OnProgressChanged(int progress) {
  Java_AssistantHeaderModel_setProgress(AttachCurrentThread(), GetHeaderModel(),
                                        progress);
}

void UiControllerAndroid::AllowShowingSoftKeyboard(bool enabled) {
  Java_AssistantModel_setAllowSoftKeyboard(AttachCurrentThread(), GetModel(),
                                           enabled);
}

void UiControllerAndroid::ExpandBottomSheet() {
  Java_AutofillAssistantUiController_expandBottomSheet(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::ShutdownGracefully() {
  DCHECK(ui_delegate_->GetDropOutReason() != Metrics::AA_START);
  Java_AutofillAssistantUiController_onShutdownGracefully(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_,
      ui_delegate_->GetDropOutReason());
}

void UiControllerAndroid::SetProgressPulsingEnabled(bool enabled) {
  Java_AssistantHeaderModel_setProgressPulsingEnabled(
      AttachCurrentThread(), GetHeaderModel(), enabled);
}

void UiControllerAndroid::OnFeedbackButtonClicked() {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_showFeedback(
      env, java_autofill_assistant_ui_controller_,
      base::android::ConvertUTF8ToJavaString(env, GetDebugContext()));
}

void UiControllerAndroid::OnCloseButtonClicked() {
  JNIEnv* env = AttachCurrentThread();
  // TODO(crbug.com/806868): Move here the logic that shutdowns immediately if
  // close button is clicked during graceful shutdown.
  Java_AutofillAssistantUiController_dismissAndShowSnackbar(
      env, java_autofill_assistant_ui_controller_,
      base::android::ConvertUTF8ToJavaString(
          env, l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_STOPPED)),
      Metrics::SHEET_CLOSED);
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

void UiControllerAndroid::OnChipsChanged(const std::vector<Chip>& chips) {
  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetCarouselModel();
  if (chips.empty()) {
    Java_AssistantCarouselModel_clearChips(env, jmodel);
    return;
  }

  std::vector<int> types;
  std::vector<std::string> texts;
  for (const auto& chip : chips) {
    types.emplace_back(chip.type);
    texts.emplace_back(chip.text);
  }
  Java_AssistantCarouselModel_setChips(
      env, jmodel, base::android::ToJavaIntArray(env, types),
      base::android::ToJavaArrayOfStrings(env, texts),
      carousel_delegate_.GetJavaObject());
}

void UiControllerAndroid::OnChipSelected(int index) {
  ui_delegate_->SelectChip(index);
}

// Overlay related methods.

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetOverlayModel() {
  return Java_AssistantModel_getOverlayModel(AttachCurrentThread(), GetModel());
}

void UiControllerAndroid::SetOverlayState(OverlayState state) {
  Java_AssistantOverlayModel_setState(AttachCurrentThread(), GetOverlayModel(),
                                      state);
}

void UiControllerAndroid::OnTouchableAreaChanged(
    const std::vector<RectF>& areas) {
  JNIEnv* env = AttachCurrentThread();
  std::vector<float> flattened;
  for (const auto& rect : areas) {
    flattened.emplace_back(rect.left);
    flattened.emplace_back(rect.top);
    flattened.emplace_back(rect.right);
    flattened.emplace_back(rect.bottom);
  }
  Java_AssistantOverlayModel_setTouchableArea(
      env, GetOverlayModel(), base::android::ToJavaFloatArray(env, flattened));
}

void UiControllerAndroid::OnUnexpectedTaps() {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_dismissAndShowSnackbar(
      env, java_autofill_assistant_ui_controller_,
      base::android::ConvertUTF8ToJavaString(
          env, l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_MAYBE_GIVE_UP)),
      Metrics::OVERLAY_STOP);
}

void UiControllerAndroid::UpdateTouchableArea() {
  ui_delegate_->UpdateTouchableArea();
}

void UiControllerAndroid::OnUserInteractionInsideTouchableArea() {
  ui_delegate_->OnUserInteractionInsideTouchableArea();
}

// Other methods.

void UiControllerAndroid::ShowOnboarding(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& on_accept) {
  Java_AutofillAssistantUiController_onShowOnboarding(
      env, java_autofill_assistant_ui_controller_, on_accept);
}

void UiControllerAndroid::Shutdown(Metrics::DropOutReason reason) {
  Java_AutofillAssistantUiController_onShutdown(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_, reason);
}

void UiControllerAndroid::Close() {
  Java_AutofillAssistantUiController_onClose(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

// Payment request related methods.

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetPaymentRequestModel() {
  return Java_AssistantModel_getPaymentRequestModel(AttachCurrentThread(),
                                                    GetModel());
}

void UiControllerAndroid::OnGetPaymentInformation(
    std::unique_ptr<PaymentInformation> payment_info) {
  JNIEnv* env = AttachCurrentThread();
  Java_AssistantModel_setAllowSwipingSheet(env, GetModel(), true);
  Java_AssistantPaymentRequestModel_clearOptions(env, GetPaymentRequestModel());
  if (get_payment_information_callback_) {
    std::move(get_payment_information_callback_).Run(std::move(payment_info));
  }
}

void UiControllerAndroid::GetPaymentInformation(
    payments::mojom::PaymentOptionsPtr payment_options,
    base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
    const std::vector<std::string>& supported_basic_card_networks) {
  DCHECK(!get_payment_information_callback_);
  get_payment_information_callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  Java_AssistantModel_setAllowSwipingSheet(env, GetModel(), false);
  Java_AssistantPaymentRequestModel_setOptions(
      env, GetPaymentRequestModel(),
      base::android::ConvertUTF8ToJavaString(env,
                                             client_->GetAccountEmailAddress()),
      payment_options->request_shipping, payment_options->request_payer_name,
      payment_options->request_payer_phone,
      payment_options->request_payer_email,
      base::android::ToJavaArrayOfStrings(env, supported_basic_card_networks));
}

// Details related method.

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetDetailsModel() {
  return Java_AssistantModel_getDetailsModel(AttachCurrentThread(), GetModel());
}

void UiControllerAndroid::OnDetailsChanged(const Details* details) {
  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetDetailsModel();
  if (!details) {
    Java_AssistantDetailsModel_clearDetails(env, jmodel);
    return;
  }

  const DetailsProto& proto = details->proto;
  auto jdetails = Java_AssistantDetails_create(
      env, base::android::ConvertUTF8ToJavaString(env, proto.title()),
      base::android::ConvertUTF8ToJavaString(env, proto.url()),
      base::android::ConvertUTF8ToJavaString(env, proto.description()),
      base::android::ConvertUTF8ToJavaString(env, proto.m_id()),
      base::android::ConvertUTF8ToJavaString(env, proto.total_price()),
      base::android::ConvertUTF8ToJavaString(env, details->datetime),
      proto.datetime().date().year(), proto.datetime().date().month(),
      proto.datetime().date().day(), proto.datetime().time().hour(),
      proto.datetime().time().minute(), proto.datetime().time().second(),
      details->changes.user_approval_required(),
      details->changes.highlight_title(), details->changes.highlight_date());
  Java_AssistantDetailsModel_setDetails(env, jmodel, jdetails);
}

void UiControllerAndroid::Stop(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  client_->Stop();
}
}  // namespace autofill_assistant.
