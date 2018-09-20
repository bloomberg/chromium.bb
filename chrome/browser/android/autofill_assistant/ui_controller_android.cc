// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"

#include <map>
#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "jni/AutofillAssistantUiController_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace autofill_assistant {
namespace switches {
const char* const kAutofillAssistantServerKey = "autofill-assistant-key";
}  // namespace switches

namespace {

// Builds a map from two Java arrays of strings with the same length.
std::unique_ptr<std::map<std::string, std::string>>
BuildParametersFromJava(JNIEnv* env, jobjectArray names, jobjectArray values) {
  std::vector<std::string> names_vector;
  base::android::AppendJavaStringArrayToStringVector(env, names, &names_vector);
  std::vector<std::string> values_vector;
  base::android::AppendJavaStringArrayToStringVector(env, values,
                                                     &values_vector);
  DCHECK_EQ(names_vector.size(), values_vector.size());
  auto parameters = std::make_unique<std::map<std::string, std::string>>();
  for (size_t i = 0; i < names_vector.size(); ++i) {
    parameters->insert(std::make_pair(names_vector[i], values_vector[i]));
  }
  return parameters;
}
}  // namespace

UiControllerAndroid::UiControllerAndroid(
    JNIEnv* env,
    jobject jcaller,
    const JavaParamRef<jobject>& webContents,
    const base::android::JavaParamRef<jobjectArray>& parameterNames,
    const base::android::JavaParamRef<jobjectArray>& parameterValues)
    : ui_delegate_(nullptr) {
  java_autofill_assistant_ui_controller_.Reset(env, jcaller);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(webContents);
  DCHECK(web_contents);
  Controller::CreateAndStartForWebContents(
      web_contents, base::WrapUnique(this),
      BuildParametersFromJava(env, parameterNames, parameterValues));
  DCHECK(ui_delegate_);
}

UiControllerAndroid::~UiControllerAndroid() {}

void UiControllerAndroid::SetUiDelegate(UiDelegate* ui_delegate) {
  ui_delegate_ = ui_delegate;
}

void UiControllerAndroid::ShowStatusMessage(const std::string& message) {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_onShowStatusMessage(
      env, java_autofill_assistant_ui_controller_,
      base::android::ConvertUTF8ToJavaString(env, message));
}

void UiControllerAndroid::ShowOverlay() {
  Java_AutofillAssistantUiController_onShowOverlay(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::HideOverlay() {
  Java_AutofillAssistantUiController_onHideOverlay(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::UpdateScripts(
    const std::vector<ScriptHandle>& scripts) {
  std::vector<std::string> script_paths;
  std::vector<std::string> script_names;
  for (const auto& script : scripts) {
    script_paths.emplace_back(script.path);
    script_names.emplace_back(script.name);
  }
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_onUpdateScripts(
      env, java_autofill_assistant_ui_controller_,
      base::android::ToJavaArrayOfStrings(env, script_names),
      base::android::ToJavaArrayOfStrings(env, script_paths));
}

void UiControllerAndroid::OnScriptSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& jscript_path) {
  std::string script_path;
  base::android::ConvertJavaStringToUTF8(env, jscript_path, &script_path);
  ui_delegate_->OnScriptSelected(script_path);
}

void UiControllerAndroid::ChooseAddress(
    base::OnceCallback<void(const std::string&)> callback) {
  // TODO(crbug.com/806868): Implement ChooseAddress.
  std::move(callback).Run("");
}

void UiControllerAndroid::ChooseCard(
    base::OnceCallback<void(const std::string&)> callback) {
  // TODO(crbug.com/806868): Implement ChooseCard.
  std::move(callback).Run("");
}

std::string UiControllerAndroid::GetApiKey() {
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

UiController* UiControllerAndroid::GetUiController() {
  return this;
}

void UiControllerAndroid::Destroy(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  ui_delegate_->OnDestroy();
}

static jlong JNI_AutofillAssistantUiController_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& webContents,
    const base::android::JavaParamRef<jobjectArray>& parameterNames,
    const base::android::JavaParamRef<jobjectArray>& parameterValues) {
  auto* ui_controller_android = new autofill_assistant::UiControllerAndroid(
      env, jcaller, webContents, parameterNames, parameterValues);
  return reinterpret_cast<intptr_t>(ui_controller_android);
}

}  // namespace autofill_assistant.
