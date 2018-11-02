// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/language/core/common/language_experiments.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "content/public/browser/web_contents.h"
#include "jni/TranslateBridge_jni.h"

static translate::TranslateManager* GetTranslateManager(
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(chrome_translate_client);
  translate::TranslateManager* manager =
      chrome_translate_client->GetTranslateManager();
  DCHECK(manager);
  return manager;
}

static void JNI_TranslateBridge_Translate(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  translate::TranslateManager* manager = GetTranslateManager(j_web_contents);
  manager->InitiateManualTranslation();
}

static jboolean JNI_TranslateBridge_CanManuallyTranslate(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  translate::TranslateManager* manager = GetTranslateManager(j_web_contents);
  return manager->CanManuallyTranslate();
}

static jboolean JNI_TranslateBridge_ShouldShowManualTranslateIPH(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(chrome_translate_client);
  translate::TranslateManager* manager =
      chrome_translate_client->GetTranslateManager();
  DCHECK(manager);

  const std::string page_lang = manager->GetLanguageState().original_language();
  std::unique_ptr<translate::TranslatePrefs> translate_prefs(
      chrome_translate_client->GetTranslatePrefs());

  return base::StartsWith(page_lang, "en",
                          base::CompareCase::INSENSITIVE_ASCII) &&
         language::ShouldForceTriggerTranslateOnEnglishPages(
             translate_prefs->GetForceTriggerOnEnglishPagesCount()) &&
         manager->GetLanguageState().translate_enabled();
}
