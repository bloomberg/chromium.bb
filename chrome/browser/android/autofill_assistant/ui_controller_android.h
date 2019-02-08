// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/autofill_assistant/assistant_carousel_delegate.h"
#include "chrome/browser/android/autofill_assistant/assistant_header_delegate.h"
#include "chrome/browser/android/autofill_assistant/assistant_overlay_delegate.h"
#include "chrome/browser/android/autofill_assistant/assistant_payment_request_delegate.h"
#include "components/autofill_assistant/browser/chip.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/overlay_state.h"
#include "components/autofill_assistant/browser/ui_controller.h"

namespace autofill_assistant {
// Class implements UiController, Client and starts the Controller.
// TODO(crbug.com/806868): This class should be renamed to
// AssistantMediator(Android) and listen for state changes to forward those
// changes to the UI model.
class UiControllerAndroid : public UiController {
 public:
  // pointers to |web_contents|, |client| must remain valid for the lifetime of
  // this instance.
  //
  // Pointer to |ui_delegate| must remain valid for the lifetime of this
  // instance or until WillShutdown is called.
  UiControllerAndroid(content::WebContents* web_contents,
                      Client* client,
                      UiDelegate* ui_delegate);
  ~UiControllerAndroid() override;

  // Called by ClientAndroid.
  void ShowOnboarding(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& on_accept);
  void Destroy();

  // Overrides UiController:
  void OnStateChanged(AutofillAssistantState new_state) override;
  void OnStatusMessageChanged(const std::string& message) override;
  void WillShutdown(Metrics::DropOutReason reason) override;
  void OnChipsChanged(const std::vector<Chip>& chips) override;
  void OnPaymentRequestChanged(const PaymentRequestOptions* options) override;
  void OnDetailsChanged(const Details* details) override;
  void OnProgressChanged(int progress) override;
  void OnTouchableAreaChanged(const std::vector<RectF>& areas) override;

  // Called by AssistantOverlayDelegate:
  void OnUnexpectedTaps();
  void UpdateTouchableArea();
  void OnUserInteractionInsideTouchableArea();

  // Called by AssistantHeaderDelegate:
  void OnFeedbackButtonClicked();
  void OnCloseButtonClicked();

  // Called by AssistantPaymentRequestDelegate:
  void OnGetPaymentInformation(
      std::unique_ptr<PaymentInformation> payment_info);

  // Called by AssistantCarouselDelegate:
  void OnChipSelected(int index);

  // Called by Java.
  void Stop(JNIEnv* env,
            const base::android::JavaParamRef<jobject>& obj,
            int reason);
  void DestroyUI(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnFatalError(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& message,
                    int reason);
  base::android::ScopedJavaLocalRef<jstring> GetPrimaryAccountName(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

 private:
  Client* const client_;

  // A pointer to the Autofill Assistant Controller. It can become nullptr after
  // WillShutdown() has been called.
  UiDelegate* ui_delegate_;
  AssistantOverlayDelegate overlay_delegate_;
  AssistantHeaderDelegate header_delegate_;
  AssistantPaymentRequestDelegate payment_request_delegate_;
  AssistantCarouselDelegate carousel_delegate_;

  base::android::ScopedJavaLocalRef<jobject> GetModel();
  base::android::ScopedJavaLocalRef<jobject> GetOverlayModel();
  base::android::ScopedJavaLocalRef<jobject> GetHeaderModel();
  base::android::ScopedJavaLocalRef<jobject> GetDetailsModel();
  base::android::ScopedJavaLocalRef<jobject> GetPaymentRequestModel();
  base::android::ScopedJavaLocalRef<jobject> GetCarouselModel();

  void SetOverlayState(OverlayState state);
  void AllowShowingSoftKeyboard(bool enabled);
  void ExpandBottomSheet();
  void SetProgressPulsingEnabled(bool enabled);
  void SetAllowSwipingSheet(bool allow);
  std::string GetDebugContext();

  // Debug context captured previously. If non-empty, GetDebugContext() returns
  // this context.
  std::string captured_debug_context_;

  // Java-side AutofillAssistantUiController object.
  base::android::ScopedJavaGlobalRef<jobject>
      java_autofill_assistant_ui_controller_;

  base::WeakPtrFactory<UiControllerAndroid> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UiControllerAndroid);
};

}  // namespace autofill_assistant.
#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_
