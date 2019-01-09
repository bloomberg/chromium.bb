// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/client_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/locale_utils.h"
#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/post_task.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/access_token_fetcher.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/signin/core/browser/account_info.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "jni/AutofillAssistantClient_jni.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace autofill_assistant {
namespace switches {
const char* const kAutofillAssistantServerKey = "autofill-assistant-key";
}  // namespace switches

namespace {

const base::FeatureParam<std::string> kAutofillAssistantServerUrl{
    &chrome::android::kAutofillAssistant, "url",
    "https://automate-pa.googleapis.com"};

// Time between two attempts to destroy the controller.
static constexpr base::TimeDelta kDestroyRetryInterval =
    base::TimeDelta::FromSeconds(2);

// Fills a map from two Java arrays of strings of the same length.
void FillParametersFromJava(JNIEnv* env,
                            const JavaRef<jobjectArray>& names,
                            const JavaRef<jobjectArray>& values,
                            std::map<std::string, std::string>* parameters) {
  std::vector<std::string> names_vector;
  base::android::AppendJavaStringArrayToStringVector(env, names, &names_vector);
  std::vector<std::string> values_vector;
  base::android::AppendJavaStringArrayToStringVector(env, values,
                                                     &values_vector);
  DCHECK_EQ(names_vector.size(), values_vector.size());
  for (size_t i = 0; i < names_vector.size(); ++i) {
    parameters->insert(std::make_pair(names_vector[i], values_vector[i]));
  }
}

}  // namespace

static base::android::ScopedJavaLocalRef<jobject>
JNI_AutofillAssistantClient_FromWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  ClientAndroid::CreateForWebContents(web_contents);
  return ClientAndroid::FromWebContents(web_contents)->GetJavaObject();
}

ClientAndroid::ClientAndroid(content::WebContents* web_contents)
    : web_contents_(web_contents),
      // TODO: consider creating the java objects when needed.
      java_object_(Java_AutofillAssistantClient_create(
          AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this))),
      weak_ptr_factory_(this) {}

ClientAndroid::~ClientAndroid() {
  Java_AutofillAssistantClient_clearNativePtr(AttachCurrentThread(),
                                              java_object_);
}

base::android::ScopedJavaLocalRef<jobject> ClientAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

void ClientAndroid::Autostart(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& jinitial_url,
    const JavaParamRef<jobjectArray>& parameterNames,
    const JavaParamRef<jobjectArray>& parameterValues) {
  CreateController();
  GURL initial_url(base::android::ConvertJavaStringToUTF8(env, jinitial_url));
  std::map<std::string, std::string> parameters;
  FillParametersFromJava(env, parameterNames, parameterValues, &parameters);
  controller_->Start(initial_url, parameters);
}

base::android::ScopedJavaLocalRef<jstring> ClientAndroid::GetPrimaryAccountName(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  AccountInfo account_info =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()))
          ->GetPrimaryAccountInfo();
  return base::android::ConvertUTF8ToJavaString(env, account_info.email);
}

void ClientAndroid::OnAccessToken(JNIEnv* env,
                                  const JavaParamRef<jobject>& jcaller,
                                  jboolean success,
                                  const JavaParamRef<jstring>& access_token) {
  if (fetch_access_token_callback_) {
    std::move(fetch_access_token_callback_)
        .Run(success, base::android::ConvertJavaStringToUTF8(access_token));
  }
}

std::string ClientAndroid::GetApiKey() {
  std::string api_key;
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    api_key = chrome::GetChannel() == version_info::Channel::STABLE
                  ? google_apis::GetAPIKey()
                  : google_apis::GetNonStableAPIKey();
  }
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAutofillAssistantServerKey)) {
    api_key = command_line->GetSwitchValueASCII(
        switches::kAutofillAssistantServerKey);
  }
  return api_key;
}

std::string ClientAndroid::GetAccountEmailAddress() {
  JNIEnv* env = AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_AutofillAssistantClient_getAccountEmailAddress(env, java_object_));
}

AccessTokenFetcher* ClientAndroid::GetAccessTokenFetcher() {
  return this;
}

autofill::PersonalDataManager* ClientAndroid::GetPersonalDataManager() {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      ProfileManager::GetLastUsedProfile());
}

std::string ClientAndroid::GetServerUrl() {
  return kAutofillAssistantServerUrl.Get();
}

UiController* ClientAndroid::GetUiController() {
  DCHECK(controller_);
  if (!ui_controller_android_ && controller_) {
    ui_controller_android_ = std::make_unique<UiControllerAndroid>(
        web_contents_, /* client= */ this, controller_.get());
  }
  return ui_controller_android_.get();
}

std::string ClientAndroid::GetLocale() {
  return base::android::GetDefaultLocaleString();
}

std::string ClientAndroid::GetCountryCode() {
  return base::android::ConvertJavaStringToUTF8(
      Java_AutofillAssistantClient_getCountryCode(AttachCurrentThread(),
                                                  java_object_));
}

void ClientAndroid::Stop() {
  if (!controller_)
    return;

  if (!controller_->Terminate()) {
    // This is a safety net and should be removed once all uses of
    // base::Unretained in the execution and script tracking has been removed.
    base::PostDelayedTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ClientAndroid::Stop, weak_ptr_factory_.GetWeakPtr()),
        kDestroyRetryInterval);
    return;
  }
  ui_controller_android_.reset();
  controller_.reset();
}

void ClientAndroid::FetchAccessToken(
    base::OnceCallback<void(bool, const std::string&)> callback) {
  DCHECK(!fetch_access_token_callback_);

  fetch_access_token_callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantClient_fetchAccessToken(env, java_object_);
}

void ClientAndroid::InvalidateAccessToken(const std::string& access_token) {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantClient_invalidateAccessToken(
      env, java_object_,
      base::android::ConvertUTF8ToJavaString(env, access_token));
}

void ClientAndroid::CreateController() {
  if (controller_) {
    return;
  }
  controller_ = std::make_unique<Controller>(
      web_contents_,
      /* client= */ this, WebController::CreateForWebContents(web_contents_),
      Service::Create(web_contents_->GetBrowserContext(),
                      /* client= */ this));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClientAndroid);

}  // namespace autofill_assistant.
