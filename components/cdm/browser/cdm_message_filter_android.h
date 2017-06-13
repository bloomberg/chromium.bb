// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_BROWSER_CDM_MESSAGE_FILTER_ANDROID_H_
#define COMPONENTS_CDM_BROWSER_CDM_MESSAGE_FILTER_ANDROID_H_

#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"

struct SupportedKeySystemRequest;
struct SupportedKeySystemResponse;

namespace cdm {

// Message filter for EME on Android. It is responsible for getting the
// SupportedKeySystems information and passing it back to renderer.
// TODO(xhwang): Convert this to a mojo interface or merge this with
// desktop Chromium's IsPepperCdmAvailable() path.
class CdmMessageFilterAndroid : public content::BrowserMessageFilter {
 public:
  explicit CdmMessageFilterAndroid(bool can_use_secure_codecs = false);

 private:
  ~CdmMessageFilterAndroid() override;

  // BrowserMessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;

  // Query the key system information.
  void OnQueryKeySystemSupport(const SupportedKeySystemRequest& request,
                               SupportedKeySystemResponse* response);

  void OnGetPlatformKeySystemNames(std::vector<std::string>* key_systems);

  // By default, rendering of secure codecs is supported when AndroidOverlay is
  // enabled. However, on platforms like Cast on Android, secure codecs are
  // always supported. This flag is used to force secure codecs support on such
  // platforms.
  // TODO(yucliu): Remove this and completely switch to the Clank model.
  const bool force_to_support_secure_codecs_;

  DISALLOW_COPY_AND_ASSIGN(CdmMessageFilterAndroid);
};

}  // namespace cdm

#endif  // COMPONENTS_CDM_BROWSER_CDM_MESSAGE_FILTER_ANDROID_H_
