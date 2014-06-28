// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_AUTOCOMPLETE_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_AUTOCOMPLETE_CONTROLLER_ANDROID_H_

#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "components/autocomplete/autocomplete_input.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

class AutocompleteController;
struct AutocompleteMatch;
class AutocompleteResult;
class Profile;
class Tab;

// The native part of the Java AutocompleteController class.
class AutocompleteControllerAndroid : public AutocompleteControllerDelegate,
                                      public KeyedService {
 public:
  explicit AutocompleteControllerAndroid(Profile* profile);

  // Methods that forward to AutocompleteController:
  void Start(JNIEnv* env,
             jobject obj,
             jstring j_text,
             jstring j_desired_tld,
             jstring j_current_url,
             bool prevent_inline_autocomplete,
             bool prefer_keyword,
             bool allow_exact_keyword_match,
             bool best_match_only);
  base::android::ScopedJavaLocalRef<jobject> Classify(JNIEnv* env,
                                                      jobject obj,
                                                      jstring j_text);
  void StartZeroSuggest(JNIEnv* env,
                        jobject obj,
                        jstring j_omnibox_text,
                        jstring j_current_url,
                        jboolean is_query_in_omnibox,
                        jboolean focused_from_fakebox);
  void Stop(JNIEnv* env, jobject obj, bool clear_result);
  void ResetSession(JNIEnv* env, jobject obj);
  void OnSuggestionSelected(JNIEnv* env,
                            jobject obj,
                            jint selected_index,
                            jstring j_current_url,
                            jboolean is_query_in_omnibox,
                            jboolean focused_from_fakebox,
                            jlong elapsed_time_since_first_modified,
                            jobject j_web_contents);
  void DeleteSuggestion(JNIEnv* env, jobject obj, int selected_index);
  base::android::ScopedJavaLocalRef<jstring> UpdateMatchDestinationURL(
      JNIEnv* env,
      jobject obj,
      jint selected_index,
      jlong elapsed_time_since_input_change);

  base::android::ScopedJavaLocalRef<jobject> GetTopSynchronousMatch(
      JNIEnv* env,
      jobject obj,
      jstring query);

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static AutocompleteControllerAndroid* GetForProfile(Profile* profile,
                                             JNIEnv* env,
                                             jobject obj);

    static Factory* GetInstance();

   protected:
    virtual content::BrowserContext* GetBrowserContextToUse(
        content::BrowserContext* context) const OVERRIDE;

   private:
    friend struct DefaultSingletonTraits<Factory>;

    Factory();
    virtual ~Factory();

    // BrowserContextKeyedServiceFactory
    virtual KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* profile) const OVERRIDE;
  };

 private:
  virtual ~AutocompleteControllerAndroid();
  void InitJNI(JNIEnv* env, jobject obj);

  // AutocompleteControllerDelegate implementation.
  virtual void OnResultChanged(bool default_match_changed) OVERRIDE;

  // Notifies the Java AutocompleteController that suggestions were received
  // based on the text the user typed in last.
  void NotifySuggestionsReceived(
      const AutocompleteResult& autocomplete_result);

  // Classifies the type of page we are on.
  metrics::OmniboxEventProto::PageClassification ClassifyPage(
      const GURL& gurl,
      bool is_query_in_omnibox,
      bool focused_from_fakebox) const;

  base::android::ScopedJavaLocalRef<jobject> BuildOmniboxSuggestion(
      JNIEnv* env, const AutocompleteMatch& match);

  // Converts destination_url (which is in its canonical form or punycode) to a
  // user-friendly URL by looking up accept languages of the current profile.
  // e.g. http://xn--6q8b.kr/ --> 한.kr
  base::string16 FormatURLUsingAcceptLanguages(GURL url);

  // A helper method for fetching the top synchronous autocomplete result.
  // The |prevent_inline_autocomplete| flag is passed to the AutocompleteInput
  // object, see documentation there for its description.
  base::android::ScopedJavaLocalRef<jobject> GetTopSynchronousResult(
      JNIEnv* env,
      jobject obj,
      jstring j_text,
      bool prevent_inline_autocomplete);

  scoped_ptr<AutocompleteController> autocomplete_controller_;

  // Last input we sent to the autocomplete controller.
  AutocompleteInput input_;

  // Whether we're currently inside a call to Start() that's called
  // from GetTopSynchronousResult().
  bool inside_synchronous_start_;

  JavaObjectWeakGlobalRef weak_java_autocomplete_controller_android_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteControllerAndroid);
};

// Registers the LocationBar native method.
bool RegisterAutocompleteControllerAndroid(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_AUTOCOMPLETE_CONTROLLER_ANDROID_H_
