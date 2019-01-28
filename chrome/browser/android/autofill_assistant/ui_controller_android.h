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
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/ui_controller.h"

namespace autofill_assistant {
// Class implements UiController, Client and starts the Controller.
// TODO(crbug.com/806868): This class should be renamed to
// AssistantMediator(Android) and listen for state changes to forward those
// changes to the UI model.
class UiControllerAndroid : public UiController {
 public:
  // pointers to |web_contents|, |client| and |ui_delegate| must remain valid
  // for the lifetime of this instance.
  UiControllerAndroid(content::WebContents* web_contents,
                      Client* client,
                      UiDelegate* ui_delegate);
  ~UiControllerAndroid() override;

  // Called by ClientAndroid.
  void ShowOnboarding(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& on_accept);

  // Overrides UiController:
  void OnStateChanged(AutofillAssistantState new_state) override;
  void OnStatusMessageChanged(const std::string& message) override;
  void Shutdown(Metrics::DropOutReason reason) override;
  void Close() override;
  void SetChips(std::unique_ptr<std::vector<Chip>> chips) override;
  void ClearChips() override;
  void GetPaymentInformation(
      payments::mojom::PaymentOptionsPtr payment_options,
      base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
      const std::string& title,
      const std::vector<std::string>& supported_basic_card_networks) override;
  void OnDetailsChanged(const Details* details) override;
  void ShowProgressBar(int progress) override;
  void HideProgressBar() override;
  void UpdateTouchableArea(bool enabled,
                           const std::vector<RectF>& areas) override;

  // Called by AssistantOverlayDelegate:
  void OnUnexpectedTaps();
  void UpdateTouchableArea();
  void OnUserInteractionInsideTouchableArea();

  // Called by AssistantHeaderDelegate:
  void OnFeedbackButtonClicked();
  void OnCloseButtonClicked();

  // Called by AssistantCarouselDelegate:
  void OnChipSelected(int index);

  // Called by Java.
  void Stop(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnGetPaymentInformation(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean jsucceed,
      const base::android::JavaParamRef<jobject>& jcard,
      const base::android::JavaParamRef<jobject>& jaddress,
      const base::android::JavaParamRef<jstring>& jpayer_name,
      const base::android::JavaParamRef<jstring>& jpayer_phone,
      const base::android::JavaParamRef<jstring>& jpayer_email,
      jboolean jis_terms_and_services_accepted);
  base::android::ScopedJavaLocalRef<jstring> GetPrimaryAccountName(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

 private:
  Client* const client_;
  UiDelegate* const ui_delegate_;
  AssistantOverlayDelegate overlay_delegate_;
  AssistantHeaderDelegate header_delegate_;
  AssistantCarouselDelegate carousel_delegate_;

  base::android::ScopedJavaLocalRef<jobject> GetModel();
  base::android::ScopedJavaLocalRef<jobject> GetOverlayModel();
  base::android::ScopedJavaLocalRef<jobject> GetHeaderModel();
  base::android::ScopedJavaLocalRef<jobject> GetDetailsModel();
  base::android::ScopedJavaLocalRef<jobject> GetCarouselModel();

  void ShowOverlay();
  void HideOverlay();
  void AllowShowingSoftKeyboard(bool enabled);
  void ExpandBottomSheet();
  void ShutdownGracefully();
  void SetProgressPulsingEnabled(bool enabled);
  std::string GetDebugContext();

  // Java-side AutofillAssistantUiController object.
  base::android::ScopedJavaGlobalRef<jobject>
      java_autofill_assistant_ui_controller_;

  std::unique_ptr<std::vector<Chip>> current_chips_;
  base::OnceCallback<void(std::unique_ptr<PaymentInformation>)>
      get_payment_information_callback_;

  base::WeakPtrFactory<UiControllerAndroid> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UiControllerAndroid);
};

}  // namespace autofill_assistant.
#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_
