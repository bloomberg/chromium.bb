// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_BLUETOOTH_CHOOSER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_BLUETOOTH_CHOOSER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/browser/web_contents.h"

// Represents a way to ask the user to select a Bluetooth device from a list of
// options.
class BluetoothChooserAndroid : public content::BluetoothChooser {
 public:
  BluetoothChooserAndroid(content::WebContents* web_contents,
                          const EventHandler& event_handler,
                          const GURL& origin);
  ~BluetoothChooserAndroid() override;

  // content::BluetoothChooser:
  void SetAdapterPresence(AdapterPresence presence) override;
  void ShowDiscoveryState(DiscoveryState state) override;
  void AddDevice(const std::string& device_id,
                 const base::string16& device_name) override;
  void RemoveDevice(const std::string& device_id) override;

  // Report which device was selected (device_id).
  void OnDeviceSelected(JNIEnv* env, jobject obj, jstring device_id);

  // Notify bluetooth stack that the search needs to be re-issued.
  void RestartSearch(JNIEnv* env, jobject obj);

  void ShowBluetoothOverviewLink(JNIEnv* env, jobject obj);
  void ShowBluetoothPairingLink(JNIEnv* env, jobject obj);
  void ShowBluetoothAdapterOffLink(JNIEnv* env, jobject obj);

  static bool Register(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;

  BluetoothChooser::EventHandler event_handler_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_BLUETOOTH_CHOOSER_ANDROID_H_
