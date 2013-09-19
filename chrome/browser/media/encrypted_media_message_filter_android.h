// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ENCRYPTED_MEDIA_MESSAGE_FILTER_ANDROID_H_
#define CHROME_BROWSER_MEDIA_ENCRYPTED_MEDIA_MESSAGE_FILTER_ANDROID_H_

#include "base/basictypes.h"
#include "content/public/browser/browser_message_filter.h"

namespace android {
struct SupportedKeySystemRequest;
struct SupportedKeySystemResponse;
}

namespace chrome {

// Message filter for EME on android. It is responsible for getting the
// SupportedKeySystems information and passing it back to renderer.
class EncryptedMediaMessageFilterAndroid
    : public content::BrowserMessageFilter {
 public:
  EncryptedMediaMessageFilterAndroid();

 private:
  virtual ~EncryptedMediaMessageFilterAndroid();

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;

  // Retrieve the supported key systems.
  void OnGetSupportedKeySystems(
      const android::SupportedKeySystemRequest& request,
      android::SupportedKeySystemResponse* response);

  DISALLOW_COPY_AND_ASSIGN(EncryptedMediaMessageFilterAndroid);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_ENCRYPTED_MEDIA_MESSAGE_FILTER_ANDROID_H_
