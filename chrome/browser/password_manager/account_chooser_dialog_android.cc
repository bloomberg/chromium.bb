// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/account_chooser_dialog_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/credential_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/account_avatar_fetcher.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "jni/AccountChooserDialog_jni.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/range/range.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;

namespace {

void AddElementsToJavaCredentialArray(
    JNIEnv* env,
    ScopedJavaLocalRef<jobjectArray> java_credentials_array,
    const std::vector<const autofill::PasswordForm*>& password_forms,
    password_manager::CredentialType type,
    int indexStart = 0) {
  int index = indexStart;
  for (auto password_form : password_forms) {
    ScopedJavaLocalRef<jobject> java_credential = CreateNativeCredential(
        env, *password_form, index - indexStart, static_cast<int>(type));
    env->SetObjectArrayElement(java_credentials_array.obj(), index,
                               java_credential.obj());
    index++;
  }
}

class AvatarFetcherAndroid : public AccountAvatarFetcher {
 public:
  AvatarFetcherAndroid(
      const GURL& url,
      int index,
      const base::android::ScopedJavaGlobalRef<jobject>& java_dialog);

 private:
  ~AvatarFetcherAndroid() override = default;

  // chrome::BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  int index_;
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;

  DISALLOW_COPY_AND_ASSIGN(AvatarFetcherAndroid);
};

AvatarFetcherAndroid::AvatarFetcherAndroid(
    const GURL& url,
    int index,
    const base::android::ScopedJavaGlobalRef<jobject>& java_dialog)
    : AccountAvatarFetcher(url, base::WeakPtr<AccountAvatarFetcherDelegate>()),
      index_(index),
      java_dialog_(java_dialog) {}

void AvatarFetcherAndroid::OnFetchComplete(const GURL& url,
                                           const SkBitmap* bitmap) {
  if (bitmap) {
    base::android::ScopedJavaLocalRef<jobject> java_bitmap =
        gfx::ConvertToJavaBitmap(bitmap);
    Java_AccountChooserDialog_imageFetchComplete(
        AttachCurrentThread(), java_dialog_.obj(), index_, java_bitmap.obj());
  }
  delete this;
}

void FetchAvatars(
    const base::android::ScopedJavaGlobalRef<jobject>& java_dialog,
    const std::vector<const autofill::PasswordForm*>& password_forms,
    int index,
    net::URLRequestContextGetter* request_context) {
  for (auto password_form : password_forms) {
    if (!password_form->icon_url.is_valid())
      continue;
    // Fetcher deletes itself once fetching is finished.
    auto fetcher =
        new AvatarFetcherAndroid(password_form->icon_url, index, java_dialog);
    fetcher->Start(request_context);
    ++index;
  }
}

};  // namespace

AccountChooserDialogAndroid::AccountChooserDialogAndroid(
    content::WebContents* web_contents,
    ScopedVector<autofill::PasswordForm> local_credentials,
    ScopedVector<autofill::PasswordForm> federated_credentials,
    const GURL& origin,
    const ManagePasswordsState::CredentialsCallback& callback)
    : web_contents_(web_contents), origin_(origin) {
  passwords_data_.set_client(
      ChromePasswordManagerClient::FromWebContents(web_contents_));
  passwords_data_.OnRequestCredentials(
      std::move(local_credentials), std::move(federated_credentials), origin);
  passwords_data_.set_credentials_callback(callback);
}

AccountChooserDialogAndroid::~AccountChooserDialogAndroid() {}

void AccountChooserDialogAndroid::ShowDialog() {
  JNIEnv* env = AttachCurrentThread();
  bool is_smartlock_branding_enabled =
      password_bubble_experiment::IsSmartLockBrandingEnabled(
          ProfileSyncServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents_->GetBrowserContext())));
  base::string16 title;
  gfx::Range title_link_range = gfx::Range();
  GetAccountChooserDialogTitleTextAndLinkRange(is_smartlock_branding_enabled,
                                               &title, &title_link_range);
  gfx::NativeWindow native_window = web_contents_->GetTopLevelNativeWindow();
  size_t credential_array_size =
      local_credentials_forms().size() + federated_credentials_forms().size();
  ScopedJavaLocalRef<jobjectArray> java_credentials_array =
      CreateNativeCredentialArray(env, credential_array_size);
  AddElementsToJavaCredentialArray(
      env, java_credentials_array, local_credentials_forms(),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  AddElementsToJavaCredentialArray(
      env, java_credentials_array, federated_credentials_forms(),
      password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED,
      local_credentials_forms().size());
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_global;
  java_dialog_global.Reset(Java_AccountChooserDialog_createAccountChooser(
      env, native_window->GetJavaObject().obj(),
      reinterpret_cast<intptr_t>(this), java_credentials_array.obj(),
      base::android::ConvertUTF16ToJavaString(env, title).obj(),
      title_link_range.start(), title_link_range.end(),
      base::android::ConvertUTF8ToJavaString(
          env, password_manager::GetShownOrigin(origin_, std::string()))
          .obj()));
  base::android::ScopedJavaLocalRef<jobject> java_dialog(java_dialog_global);
  net::URLRequestContextGetter* request_context =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetRequestContext();
  FetchAvatars(java_dialog_global, local_credentials_forms(), 0,
               request_context);
  FetchAvatars(java_dialog_global, federated_credentials_forms(),
               local_credentials_forms().size(), request_context);
}

void AccountChooserDialogAndroid::OnCredentialClicked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint credential_item,
    jint credential_type) {
  ChooseCredential(
      credential_item,
      static_cast<password_manager::CredentialType>(credential_type));
}

void AccountChooserDialogAndroid::Destroy(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  delete this;
}

void AccountChooserDialogAndroid::CancelDialog(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ChooseCredential(-1, password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY);
}

void AccountChooserDialogAndroid::OnLinkClicked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(password_manager::kPasswordManagerAccountDashboardURL),
      content::Referrer(), NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_initiated */));
}

const std::vector<const autofill::PasswordForm*>&
AccountChooserDialogAndroid::local_credentials_forms() const {
  return passwords_data_.GetCurrentForms();
}

const std::vector<const autofill::PasswordForm*>&
AccountChooserDialogAndroid::federated_credentials_forms() const {
  return passwords_data_.federated_credentials_forms();
}

void AccountChooserDialogAndroid::ChooseCredential(
    size_t index,
    password_manager::CredentialType type) {
  using namespace password_manager;
  if (type == CredentialType::CREDENTIAL_TYPE_EMPTY) {
    passwords_data_.ChooseCredential(autofill::PasswordForm(), type);
    return;
  }
  DCHECK(type == CredentialType::CREDENTIAL_TYPE_PASSWORD ||
         type == CredentialType::CREDENTIAL_TYPE_FEDERATED);
  const auto& credentials_forms =
      (type == CredentialType::CREDENTIAL_TYPE_PASSWORD)
          ? local_credentials_forms()
          : federated_credentials_forms();
  if (index < credentials_forms.size()) {
    passwords_data_.ChooseCredential(*credentials_forms[index], type);
  }
}

bool RegisterAccountChooserDialogAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
