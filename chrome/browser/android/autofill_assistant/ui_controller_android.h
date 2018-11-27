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
#include "components/autofill_assistant/browser/access_token_fetcher.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/ui_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace autofill_assistant {
// Class implements UiController, Client and starts the Controller.
class UiControllerAndroid : public UiController,
                            public Client,
                            public AccessTokenFetcher {
 public:
  UiControllerAndroid(
      JNIEnv* env,
      jobject jcaller,
      const base::android::JavaParamRef<jobject>& webContents,
      const base::android::JavaParamRef<jobjectArray>& parameterNames,
      const base::android::JavaParamRef<jobjectArray>& parameterValues,
      const base::android::JavaParamRef<jstring>& locale,
      const base::android::JavaParamRef<jstring>& countryCode);
  ~UiControllerAndroid() override;

  // Overrides UiController:
  void SetUiDelegate(UiDelegate* ui_delegate) override;
  void ShowStatusMessage(const std::string& message) override;
  std::string GetStatusMessage() override;
  void ShowOverlay() override;
  void HideOverlay() override;
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

  // Overrides Client:
  std::string GetApiKey() override;
  AccessTokenFetcher* GetAccessTokenFetcher() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  std::string GetServerUrl() override;
  UiController* GetUiController() override;

  // Overrides AccessTokenFetcher
  void FetchAccessToken(
      base::OnceCallback<void(bool, const std::string&)>) override;
  void InvalidateAccessToken(const std::string& access_token) override;

  // Called by Java.
  void Start(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& jcaller,
             const base::android::JavaParamRef<jstring>& initialUrlString);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void ScrollBy(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                float distanceX,
                float distanceY);
  void UpdateTouchableArea(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);
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
  void OnAccessToken(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller,
                     jboolean success,
                     const base::android::JavaParamRef<jstring>& access_token);
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
  // Java-side AutofillAssistantUiController object.
  base::android::ScopedJavaGlobalRef<jobject>
      java_autofill_assistant_ui_controller_;

  UiDelegate* ui_delegate_;
  content::BrowserContext* browser_context_;

  base::OnceCallback<void(const std::string&)> choice_callback_;
  base::OnceCallback<void(std::unique_ptr<PaymentInformation>)>
      get_payment_information_callback_;
  std::unique_ptr<AccessTokenFetcher> access_token_fetcher_;
  base::OnceCallback<void(bool, const std::string&)>
      fetch_access_token_callback_;
  base::OnceCallback<void(bool)> show_details_callback_;

  base::WeakPtrFactory<UiControllerAndroid> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UiControllerAndroid);
};

}  // namespace autofill_assistant.
#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_
