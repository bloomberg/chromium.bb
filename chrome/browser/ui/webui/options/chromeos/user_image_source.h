// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_USER_IMAGE_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_USER_IMAGE_SOURCE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/url_data_source.h"
#include "ui/base/layout.h"

class AccountId;

namespace base {
class RefCountedMemory;
}

namespace chromeos {
namespace options {

// UserImageSource is the data source that serves user images for users that
// have it.
class UserImageSource : public content::URLDataSource {
 public:
  UserImageSource();

  // content::URLDataSource implementation.
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string& path) const override;

  // Returns PNG encoded image for user with specified |account_id|. If there's
  // no user with such an id, returns the first default image.
  static base::RefCountedMemory* GetUserImage(const AccountId& account_id,
                                              ui::ScaleFactor scale_factor);

 private:
  ~UserImageSource() override;

  DISALLOW_COPY_AND_ASSIGN(UserImageSource);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_USER_IMAGE_SOURCE_H_
