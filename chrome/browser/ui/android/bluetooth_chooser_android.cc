// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/bluetooth_chooser_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ssl/chrome_security_state_model_client.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/BluetoothChooserDialog_jni.h"
#include "ui/android/window_android.h"
#include "url/origin.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;

BluetoothChooserAndroid::BluetoothChooserAndroid(
    content::WebContents* web_contents,
    const EventHandler& event_handler,
    const url::Origin& origin)
    : event_handler_(event_handler) {
  DCHECK(!origin.unique());
  base::android::ScopedJavaLocalRef<jobject> window_android =
      content::ContentViewCore::FromWebContents(
          web_contents)->GetWindowAndroid()->GetJavaObject();

  ChromeSecurityStateModelClient* security_model_client =
      ChromeSecurityStateModelClient::FromWebContents(web_contents);
  DCHECK(security_model_client);

  // Create (and show) the BluetoothChooser dialog.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> origin_string =
      ConvertUTF8ToJavaString(env, origin.Serialize());
  java_dialog_.Reset(Java_BluetoothChooserDialog_create(
      env, window_android.obj(), origin_string.obj(),
      security_model_client->GetSecurityInfo().security_level,
      reinterpret_cast<intptr_t>(this)));
}

BluetoothChooserAndroid::~BluetoothChooserAndroid() {
  if (!java_dialog_.is_null()) {
    Java_BluetoothChooserDialog_closeDialog(AttachCurrentThread(),
                                            java_dialog_.obj());
  }
}

bool BluetoothChooserAndroid::CanAskForScanningPermission() {
  // Creating the dialog returns null if Chromium can't ask for permission to
  // scan for BT devices.
  return !java_dialog_.is_null();
}

void BluetoothChooserAndroid::SetAdapterPresence(AdapterPresence presence) {
  if (presence != AdapterPresence::POWERED_ON) {
    Java_BluetoothChooserDialog_notifyAdapterTurnedOff(AttachCurrentThread(),
                                                       java_dialog_.obj());
  }
}

void BluetoothChooserAndroid::ShowDiscoveryState(DiscoveryState state) {
  // These constants are used in BluetoothChooserDialog.notifyDiscoveryState.
  int java_state = -1;
  switch (state) {
    case DiscoveryState::FAILED_TO_START:
      java_state = 0;
      break;
    case DiscoveryState::DISCOVERING:
      java_state = 1;
      break;
    case DiscoveryState::IDLE:
      java_state = 2;
      break;
  }
  Java_BluetoothChooserDialog_notifyDiscoveryState(
      AttachCurrentThread(), java_dialog_.obj(), java_state);
}

void BluetoothChooserAndroid::AddDevice(const std::string& device_id,
                                        const base::string16& device_name) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_device_id =
      ConvertUTF8ToJavaString(env, device_id);
  ScopedJavaLocalRef<jstring> java_device_name =
      ConvertUTF16ToJavaString(env, device_name);
  Java_BluetoothChooserDialog_addDevice(
      env, java_dialog_.obj(), java_device_id.obj(), java_device_name.obj());
}

void BluetoothChooserAndroid::RemoveDevice(const std::string& device_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_device_id =
      ConvertUTF16ToJavaString(env, base::UTF8ToUTF16(device_id));
  Java_BluetoothChooserDialog_removeDevice(env, java_dialog_.obj(),
                                           java_device_id.obj());
}

void BluetoothChooserAndroid::OnDialogFinished(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint event_type,
    const JavaParamRef<jstring>& device_id) {
  // Values are defined in BluetoothChooserDialog as DIALOG_FINISHED constants.
  switch (event_type) {
    case 0:
      event_handler_.Run(Event::DENIED_PERMISSION, "");
      return;
    case 1:
      event_handler_.Run(Event::CANCELLED, "");
      return;
    case 2:
      event_handler_.Run(
          Event::SELECTED,
          base::android::ConvertJavaStringToUTF8(env, device_id));
      return;
  }
  NOTREACHED();
}

void BluetoothChooserAndroid::RestartSearch(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  event_handler_.Run(Event::RESCAN, "");
}

void BluetoothChooserAndroid::ShowBluetoothOverviewLink(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  event_handler_.Run(Event::SHOW_OVERVIEW_HELP, "");
}

void BluetoothChooserAndroid::ShowBluetoothPairingLink(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  event_handler_.Run(Event::SHOW_PAIRING_HELP, "");
}

void BluetoothChooserAndroid::ShowBluetoothAdapterOffLink(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  event_handler_.Run(Event::SHOW_ADAPTER_OFF_HELP, "");
}

void BluetoothChooserAndroid::ShowNeedLocationPermissionLink(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  event_handler_.Run(Event::SHOW_NEED_LOCATION_HELP, "");
}

// static
bool BluetoothChooserAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
