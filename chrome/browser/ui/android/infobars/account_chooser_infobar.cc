// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/account_chooser_infobar.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/account_chooser_infobar_delegate_android.h"
#include "chrome/browser/password_manager/credential_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/account_avatar_fetcher.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "jni/AccountChooserInfoBar_jni.h"
#include "ui/gfx/android/java_bitmap.h"

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
      const base::android::ScopedJavaGlobalRef<jobject>& java_infobar);

 private:
  ~AvatarFetcherAndroid() override = default;
  // chrome::BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  int index_;
  base::android::ScopedJavaGlobalRef<jobject> java_infobar_;

  DISALLOW_COPY_AND_ASSIGN(AvatarFetcherAndroid);
};

AvatarFetcherAndroid::AvatarFetcherAndroid(
    const GURL& url,
    int index,
    const base::android::ScopedJavaGlobalRef<jobject>& java_infobar)
    : AccountAvatarFetcher(url, base::WeakPtr<AccountAvatarFetcherDelegate>()),
      index_(index),
      java_infobar_(java_infobar) {
}

void AvatarFetcherAndroid::OnFetchComplete(const GURL& url,
                                           const SkBitmap* bitmap) {
  if (bitmap) {
    base::android::ScopedJavaLocalRef<jobject> java_bitmap =
        gfx::ConvertToJavaBitmap(bitmap);
    Java_AccountChooserInfoBar_imageFetchComplete(
        base::android::AttachCurrentThread(), java_infobar_.obj(), index_,
        java_bitmap.obj());
  }
  delete this;
}

void FetchAvatars(
    const base::android::ScopedJavaGlobalRef<jobject>& java_infobar,
    const std::vector<const autofill::PasswordForm*>& password_forms,
    int index,
    net::URLRequestContextGetter* request_context) {
  for (auto password_form : password_forms) {
    if (!password_form->icon_url.is_valid())
      continue;
    // Fetcher deletes itself once fetching is finished.
    auto fetcher = new AvatarFetcherAndroid(password_form->icon_url, index,
                                            java_infobar);
    fetcher->Start(request_context);
    ++index;
  }
}

};  // namespace

AccountChooserInfoBar::AccountChooserInfoBar(
    scoped_ptr<AccountChooserInfoBarDelegateAndroid> delegate)
    : InfoBarAndroid(delegate.Pass()) {
}

AccountChooserInfoBar::~AccountChooserInfoBar() {
}

base::android::ScopedJavaLocalRef<jobject>
AccountChooserInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  size_t credential_array_size =
      GetDelegate()->local_credentials_forms().size() +
      GetDelegate()->federated_credentials_forms().size();
  ScopedJavaLocalRef<jobjectArray> java_credentials_array =
      CreateNativeCredentialArray(env, credential_array_size);
  AddElementsToJavaCredentialArray(
      env, java_credentials_array, GetDelegate()->local_credentials_forms(),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  AddElementsToJavaCredentialArray(
      env, java_credentials_array, GetDelegate()->federated_credentials_forms(),
      password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED,
      GetDelegate()->local_credentials_forms().size());
  base::android::ScopedJavaGlobalRef<jobject> java_infobar_global;
  java_infobar_global.Reset(Java_AccountChooserInfoBar_show(
      env, GetEnumeratedIconId(), java_credentials_array.obj(),
      base::android::ConvertUTF16ToJavaString(env, GetDelegate()->message())
          .obj(),
      GetDelegate()->message_link_range().start(),
      GetDelegate()->message_link_range().end()));
  base::android::ScopedJavaLocalRef<jobject> java_infobar(java_infobar_global);
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(this);
  net::URLRequestContextGetter* request_context =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetRequestContext();
  FetchAvatars(java_infobar_global, GetDelegate()->local_credentials_forms(), 0,
               request_context);
  FetchAvatars(
      java_infobar_global, GetDelegate()->federated_credentials_forms(),
      GetDelegate()->local_credentials_forms().size(), request_context);
  return java_infobar;
}

void AccountChooserInfoBar::OnCredentialClicked(JNIEnv* env,
                                                jobject obj,
                                                jint credential_item,
                                                jint credential_type) {
  GetDelegate()->ChooseCredential(
      credential_item,
      static_cast<password_manager::CredentialType>(credential_type));
  RemoveSelf();
}

void AccountChooserInfoBar::ProcessButton(int action) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  GetDelegate()->ChooseCredential(
      -1, password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY);
  RemoveSelf();
}

void AccountChooserInfoBar::OnLinkClicked(JNIEnv* env, jobject obj) {
  GetDelegate()->LinkClicked();
}

AccountChooserInfoBarDelegateAndroid* AccountChooserInfoBar::GetDelegate() {
  return static_cast<AccountChooserInfoBarDelegateAndroid*>(delegate());
}

void AccountChooserInfoBar::SetJavaInfoBar(
    const base::android::JavaRef<jobject>& java_info_bar) {
  InfoBarAndroid::SetJavaInfoBar(java_info_bar);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AccountChooserInfoBar_setNativePtr(env, java_info_bar.obj(),
                                          reinterpret_cast<intptr_t>(this));
}

bool RegisterAccountChooserInfoBar(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
