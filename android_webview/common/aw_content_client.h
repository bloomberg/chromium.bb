// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_CONTENT_CLIENT_H_
#define ANDROID_WEBVIEW_COMMON_AW_CONTENT_CLIENT_H_

#include "content/public/common/content_client.h"

#include "base/compiler_specific.h"

namespace android_webview {

std::string GetUserAgent();

class AwContentClient : public content::ContentClient {
 public:
  // ContentClient implementation.
  virtual std::string GetProduct() const OVERRIDE;
  virtual std::string GetUserAgent() const OVERRIDE;
  virtual base::string16 GetLocalizedString(int message_id) const OVERRIDE;
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const OVERRIDE;
  virtual bool CanSendWhileSwappedOut(const IPC::Message* message) OVERRIDE;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_AW_CONTENT_CLIENT_H_
