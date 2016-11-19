// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_LOCALE_SPECIAL_LOCALE_HANDLER_H_
#define CHROME_BROWSER_ANDROID_LOCALE_SPECIAL_LOCALE_HANDLER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/search_engines/template_url.h"

using base::android::JavaParamRef;

class TemplateURLService;
class Profile;

class SpecialLocaleHandler {
 public:
  explicit SpecialLocaleHandler(Profile* profile,
                                const std::string& locale,
                                TemplateURLService* service);
  void Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj);
  jboolean LoadTemplateUrls(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void RemoveTemplateUrls(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void OverrideDefaultSearchProvider(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj);
  void SetGoogleAsDefaultSearch(JNIEnv* env, const JavaParamRef<jobject>& obj);

  virtual ~SpecialLocaleHandler();

 protected:
  virtual std::vector<std::unique_ptr<TemplateURLData>>
    GetLocalPrepopulatedEngines(Profile* profile);
  virtual int GetDesignatedSearchEngine();

 private:
  Profile* profile_;

  std::string locale_;

  // Tracks all local search engines that were added to TURL service.
  std::vector<int> prepopulate_ids_;

  // Pointer to the TemplateUrlService for the main profile.
  TemplateURLService* template_url_service_;

  DISALLOW_COPY_AND_ASSIGN(SpecialLocaleHandler);
};

bool RegisterSpecialLocaleHandler(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_LOCALE_SPECIAL_LOCALE_HANDLER_H_
