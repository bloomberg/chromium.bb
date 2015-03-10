// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_MESSAGE_PORT_SERVICE_IMPL_H_
#define ANDROID_WEBVIEW_NATIVE_AW_MESSAGE_PORT_SERVICE_IMPL_H_

#include <jni.h>
#include <map>

#include "android_webview/browser/aw_message_port_service.h"
#include "base/android/jni_weak_ref.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"

namespace android_webview {

// This class is the native peer of AwMessagePortService.java. Please see the
// java class for an explanation of use, ownership and lifetime.

// Threading: Created and initialized on UI thread. For other methods, see
// the method level DCHECKS or documentation.
class AwMessagePortServiceImpl : public AwMessagePortService {
 public:
  static AwMessagePortServiceImpl* GetInstance();

  AwMessagePortServiceImpl();
  ~AwMessagePortServiceImpl() override;
  void Init(JNIEnv* env, jobject object);

  void CreateMessageChannel(JNIEnv* env, jobjectArray ports,
      scoped_refptr<AwMessagePortMessageFilter> filter);

  // AwMessagePortService implementation
  void OnConvertedWebToAppMessage(
      int message_port_id,
      const base::ListValue& message,
      const std::vector<int>& sent_message_port_ids) override;
  void OnMessagePortMessageFilterClosing(
      AwMessagePortMessageFilter* filter) override;
  void CleanupPort(int message_port_id) override;

  // Methods called from Java.
  void PostAppToWebMessage(JNIEnv* env, jobject object, int sender_id,
      jstring message, jintArray sent_ports);
  void ClosePort(JNIEnv* env, jobject object, int message_port_id);
  void ReleaseMessages(JNIEnv* env, jobject object, int message_port_id);

  void RemoveSentPorts(const std::vector<int>& sent_ports);

private:
  void PostAppToWebMessageOnIOThread(
      int sender_id,
      base::string16* message,
      std::vector<int>* sent_ports);
  void CreateMessageChannelOnIOThread(
      scoped_refptr<AwMessagePortMessageFilter> filter,
      int* port1,
      int* port2);
  void OnMessageChannelCreated(
      base::android::ScopedJavaGlobalRef<jobjectArray>* ports,
      int* port1,
      int* port2);
  void AddPort(int message_port_id, AwMessagePortMessageFilter* filter);
  void PostClosePortMessage(int message_port_id);

  JavaObjectWeakGlobalRef java_ref_;
  typedef std::map<int, AwMessagePortMessageFilter*> MessagePorts;
  MessagePorts ports_; // Access on IO thread

  DISALLOW_COPY_AND_ASSIGN(AwMessagePortServiceImpl);
};

bool RegisterAwMessagePortService(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_MESSAGE_PORT_SERVICE_IMPL_H_
