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
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/ui_controller.h"

namespace autofill_assistant {
// Class implements UiController, Client and starts the Controller.
class UiControllerAndroid : public UiController {
 public:
  // pointers to |web_contents|, |client| and |ui_delegate| must remain valid
  // for the lifetime of this instance.
  UiControllerAndroid(content::WebContents* web_contents,
                      Client* client,
                      UiDelegate* ui_delegate);
  ~UiControllerAndroid() override;

  // Overrides UiController:
  void ShowStatusMessage(const std::string& message) override;
  std::string GetStatusMessage() override;
  void ShowOverlay() override;
  void HideOverlay() override;
  void AllowShowingSoftKeyboard(bool enabled) override;
  void Shutdown() override;
  void ShutdownGracefully() override;
  void Close() override;
  void UpdateScripts(const std::vector<ScriptHandle>& scripts) override;
  void Choose(const std::vector<UiController::Choice>& choices,
              base::OnceCallback<void(const std::string&)> callback) override;
  void ForceChoose(const std::string& result) override;
  void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) override;
  void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) override;
  void GetPaymentInformation(
      payments::mojom::PaymentOptionsPtr payment_options,
      base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
      const std::string& title,
      const std::vector<std::string>& supported_basic_card_networks) override;
  void HideDetails() override;
  void ShowDetails(const DetailsProto& details,
                   base::OnceCallback<void(bool)> callback) override;
  void ShowProgressBar(int progress, const std::string& message) override;
  void HideProgressBar() override;
  void UpdateTouchableArea(bool enabled,
                           const std::vector<RectF>& areas) override;
  std::string GetDebugContext() const override;
  void ExpandBottomSheet() override;

  // Called by Java.
  void Stop(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void UpdateTouchableArea(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);
  void OnUserInteractionInsideTouchableArea(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void OnScriptSelected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jscript_path);
  void OnChoice(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& jcaller,
                const base::android::JavaParamRef<jbyteArray>& jserver_payload);
  void OnAddressSelected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jaddress_guid);
  void OnCardSelected(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& jcaller,
                      const base::android::JavaParamRef<jstring>& jcard_guid);
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
  void OnShowDetails(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller,
                     jboolean success);
  base::android::ScopedJavaLocalRef<jstring> GetPrimaryAccountName(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  base::android::ScopedJavaLocalRef<jstring> OnRequestDebugContext(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

 private:
  Client* const client_;
  UiDelegate* const ui_delegate_;

  // Java-side AutofillAssistantUiController object.
  base::android::ScopedJavaGlobalRef<jobject>
      java_autofill_assistant_ui_controller_;

  base::OnceCallback<void(const std::string&)> choice_callback_;
  base::OnceCallback<void(std::unique_ptr<PaymentInformation>)>
      get_payment_information_callback_;
  base::OnceCallback<void(bool)> show_details_callback_;

  DISALLOW_COPY_AND_ASSIGN(UiControllerAndroid);
};

}  // namespace autofill_assistant.
#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_
