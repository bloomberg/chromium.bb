// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHELL_COMMON_CAST_CONTENT_CLIENT_H_
#define CHROMECAST_SHELL_COMMON_CAST_CONTENT_CLIENT_H_

#include "content/public/common/content_client.h"

namespace chromecast {
namespace shell {

std::string GetUserAgent();

class CastContentClient : public content::ContentClient {
 public:
  virtual ~CastContentClient();

  // content::ContentClient implementation:
  virtual std::string GetUserAgent() const OVERRIDE;
  virtual base::string16 GetLocalizedString(int message_id) const OVERRIDE;
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const OVERRIDE;
  virtual base::RefCountedStaticMemory* GetDataResourceBytes(
      int resource_id) const OVERRIDE;
  virtual gfx::Image& GetNativeImageNamed(int resource_id) const OVERRIDE;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_SHELL_COMMON_CAST_CONTENT_CLIENT_H_
