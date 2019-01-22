// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/contacts/contacts_provider_android.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents.h"
#include "jni/ContactsDialogHost_jni.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/android/window_android.h"

namespace content {

ContactsProviderAndroid::ContactsProviderAndroid(
    RenderFrameHostImpl* render_frame_host) {
  JNIEnv* env = base::android::AttachCurrentThread();

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;
  if (!web_contents->GetTopLevelNativeWindow())
    return;

  dialog_.Reset(Java_ContactsDialogHost_create(
      env, web_contents->GetTopLevelNativeWindow()->GetJavaObject(),
      reinterpret_cast<intptr_t>(this)));
  DCHECK(!dialog_.is_null());
}

ContactsProviderAndroid::~ContactsProviderAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContactsDialogHost_destroy(env, dialog_);
}

void ContactsProviderAndroid::Select(
    bool multiple,
    bool include_names,
    bool include_emails,
    blink::mojom::ContactsManager::SelectCallback callback) {
  if (!dialog_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  callback_ = std::move(callback);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContactsDialogHost_showDialog(env, dialog_, multiple);
}

void ContactsProviderAndroid::AddContact(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobjectArray>& names_java,
    const base::android::JavaParamRef<jobjectArray>& emails_java,
    const base::android::JavaParamRef<jobjectArray>& tel_java) {
  DCHECK(callback_);
  // TODO(finnur): Allow for nullability in names, emails and tel.
  std::vector<std::string> names;
  AppendJavaStringArrayToStringVector(env, names_java, &names);

  std::vector<std::string> emails;
  AppendJavaStringArrayToStringVector(env, emails_java, &emails);

  std::vector<std::string> tel;
  AppendJavaStringArrayToStringVector(env, tel_java, &tel);

  blink::mojom::ContactInfoPtr contact =
      blink::mojom::ContactInfo::New(names, emails, tel);

  contacts_.push_back(std::move(contact));
}

void ContactsProviderAndroid::EndContactsList(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK(callback_);
  std::move(callback_).Run(std::move(contacts_));
}

}  // namespace content
