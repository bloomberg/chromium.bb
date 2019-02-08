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

  if (ui_delegate->GetState() != AutofillAssistantState::INACTIVE) {
    // The UI was created for an existing Controller.
    OnStatusMessageChanged(ui_delegate->GetStatusMessage());
    OnProgressChanged(ui_delegate->GetProgress());
    OnDetailsChanged(ui_delegate->GetDetails());
    OnChipsChanged(ui_delegate->GetChips());
    OnPaymentRequestChanged(ui_delegate->GetPaymentRequestOptions());

    std::vector<RectF> area;
    ui_delegate->GetTouchableArea(&area);
    OnTouchableAreaChanged(area);

    OnStateChanged(ui_delegate->GetState());
  }
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
      SetAllowSwipingSheet(true);
      return;

    case AutofillAssistantState::RUNNING:
      SetOverlayState(OverlayState::FULL);
      AllowShowingSoftKeyboard(false);
      SetProgressPulsingEnabled(false);
      SetAllowSwipingSheet(true);
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

      SetAllowSwipingSheet(ui_delegate_->GetPaymentRequestOptions() == nullptr);
      // user interaction is needed.
      ExpandBottomSheet();
      return;

    case AutofillAssistantState::MODAL_DIALOG:
      SetOverlayState(OverlayState::FULL);
      AllowShowingSoftKeyboard(true);
      SetProgressPulsingEnabled(false);
      SetAllowSwipingSheet(true);
      return;

    case AutofillAssistantState::STOPPED:
      SetOverlayState(OverlayState::HIDDEN);
      AllowShowingSoftKeyboard(true);
      SetProgressPulsingEnabled(false);
      SetAllowSwipingSheet(true);

      // make sure user sees the error message.
      ExpandBottomSheet();
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

void UiControllerAndroid::SetProgressPulsingEnabled(bool enabled) {
  Java_AssistantHeaderModel_setProgressPulsingEnabled(
      AttachCurrentThread(), GetHeaderModel(), enabled);
}

void UiControllerAndroid::SetAllowSwipingSheet(bool allow) {
  Java_AssistantModel_setAllowSwipingSheet(AttachCurrentThread(), GetModel(),
                                           allow);
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
  if (captured_debug_context_.empty() && ui_delegate_) {
    return ui_delegate_->GetDebugContext();
  }
  return captured_debug_context_;
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
  if (ui_delegate_)
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
  if (ui_delegate_)
    ui_delegate_->UpdateTouchableArea();
}

void UiControllerAndroid::OnUserInteractionInsideTouchableArea() {
  if (ui_delegate_)
    ui_delegate_->OnUserInteractionInsideTouchableArea();
}

// Other methods.

void UiControllerAndroid::ShowOnboarding(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& on_accept) {
  Java_AutofillAssistantUiController_onShowOnboarding(
      env, java_autofill_assistant_ui_controller_, on_accept);
}

void UiControllerAndroid::Destroy() {
  Java_AutofillAssistantUiController_destroy(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_,
      /* delayed= */ false);
}

void UiControllerAndroid::WillShutdown(Metrics::DropOutReason reason) {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_destroy(
      env, java_autofill_assistant_ui_controller_,
      /* delayed= */ ui_delegate_->GetState() ==
          AutofillAssistantState::STOPPED);
  if (reason == Metrics::CUSTOM_TAB_CLOSED) {
    Java_AutofillAssistantUiController_scheduleCloseCustomTab(
        env, java_autofill_assistant_ui_controller_);
  }

  // Capture the debug context, for including into a feedback possibly sent
  // later, after the delegate has been destroyed.
  captured_debug_context_ = ui_delegate_->GetDebugContext();
  ui_delegate_ = nullptr;
}

// Payment request related methods.

base::android::ScopedJavaLocalRef<jobject>
UiControllerAndroid::GetPaymentRequestModel() {
  return Java_AssistantModel_getPaymentRequestModel(AttachCurrentThread(),
                                                    GetModel());
}

void UiControllerAndroid::OnGetPaymentInformation(
    std::unique_ptr<PaymentInformation> payment_info) {
  ui_delegate_->SetPaymentInformation(std::move(payment_info));
}

void UiControllerAndroid::OnPaymentRequestChanged(
    const PaymentRequestOptions* payment_options) {
  JNIEnv* env = AttachCurrentThread();
  auto jmodel = GetPaymentRequestModel();
  if (!payment_options) {
    Java_AssistantPaymentRequestModel_clearOptions(env, jmodel);
    return;
  }
  Java_AssistantPaymentRequestModel_setOptions(
      env, jmodel,
      base::android::ConvertUTF8ToJavaString(env,
                                             client_->GetAccountEmailAddress()),
      payment_options->request_shipping, payment_options->request_payer_name,
      payment_options->request_payer_phone,
      payment_options->request_payer_email,
      base::android::ToJavaArrayOfStrings(
          env, payment_options->supported_basic_card_networks));
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

void UiControllerAndroid::Stop(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               int jreason) {
  client_->Shutdown(static_cast<Metrics::DropOutReason>(jreason));
}

void UiControllerAndroid::DestroyUI(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  client_->DestroyUI();
}

void UiControllerAndroid::OnFatalError(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& jmessage,
    int jreason) {
  if (!ui_delegate_)
    return;
  ui_delegate_->OnFatalError(
      base::android::ConvertJavaStringToUTF8(env, jmessage),
      static_cast<Metrics::DropOutReason>(jreason));
}
}  // namespace autofill_assistant.
