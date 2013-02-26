// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_ANDROID_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_ANDROID_H_

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class TemplateURL;
class TemplateURLService;


// Android wrapper of the TemplateUrlService which provides access from the Java
// layer. Note that on Android, there's only a single profile, and therefore
// a single instance of this wrapper.
class TemplateUrlServiceAndroid : public content::NotificationObserver {
 public:
  TemplateUrlServiceAndroid(JNIEnv* env, jobject obj);

  void Load(JNIEnv* env, jobject obj);
  void SetDefaultSearchProvider(JNIEnv* env, jobject obj, jint selected_index);
  jint GetDefaultSearchProvider(JNIEnv* env, jobject obj);
  jint GetTemplateUrlCount(JNIEnv* env, jobject obj);
  jboolean IsLoaded(JNIEnv* env, jobject obj);
  base::android::ScopedJavaLocalRef<jobject>
      GetPrepopulatedTemplateUrlAt(JNIEnv* env, jobject obj, jint index);

  // NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  static bool Register(JNIEnv* env);

 private:
  virtual ~TemplateUrlServiceAndroid();

  bool IsPrepopulatedTemplate(TemplateURL* url);

  JavaObjectWeakGlobalRef weak_java_obj_;
  content::NotificationRegistrar registrar_;

  // Pointer to the TemplateUrlService for the main profile.
  TemplateURLService* template_url_service_;

  DISALLOW_COPY_AND_ASSIGN(TemplateUrlServiceAndroid);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_ANDROID_H_
