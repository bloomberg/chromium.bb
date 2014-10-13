// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_CAST_CONTENT_CLIENT_H_
#define CHROMECAST_COMMON_CAST_CONTENT_CLIENT_H_

#include "content/public/common/content_client.h"

namespace chromecast {
namespace shell {

std::string GetUserAgent();

class CastContentClient : public content::ContentClient {
 public:
  virtual ~CastContentClient();

  // content::ContentClient implementation:
  virtual std::string GetUserAgent() const override;
  virtual base::string16 GetLocalizedString(int message_id) const override;
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const override;
  virtual base::RefCountedStaticMemory* GetDataResourceBytes(
      int resource_id) const override;
  virtual gfx::Image& GetNativeImageNamed(int resource_id) const override;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_COMMON_CAST_CONTENT_CLIENT_H_
