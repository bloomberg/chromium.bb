// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_SHELL_CONTENT_CLIENT_H_
#define APPS_SHELL_SHELL_CONTENT_CLIENT_H_

#include "base/compiler_specific.h"
#include "content/public/common/content_client.h"

namespace apps {

class ShellContentClient : public content::ContentClient {
 public:
  ShellContentClient();
  virtual ~ShellContentClient();

  virtual std::string GetUserAgent() const OVERRIDE;
  virtual string16 GetLocalizedString(int message_id) const OVERRIDE;
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const OVERRIDE;
  virtual base::RefCountedStaticMemory* GetDataResourceBytes(
      int resource_id) const OVERRIDE;
  virtual gfx::Image& GetNativeImageNamed(int resource_id) const OVERRIDE;
};

}  // namespace apps

#endif  // APPS_SHELL_SHELL_CONTENT_CLIENT_H_
