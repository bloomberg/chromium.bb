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
  virtual std::string GetSource() OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;

  // Returns PNG or GIF (when possible and if |is_image_animated| flag
  // is true) encoded image for user with specified email.  If there's
  // no user with such email, returns the first default image.
  base::RefCountedMemory* GetUserImage(const std::string& email,
                                       bool is_image_animated,
                                       ui::ScaleFactor scale_factor) const;

 private:
  virtual ~UserImageSource();

  DISALLOW_COPY_AND_ASSIGN(UserImageSource);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_USER_IMAGE_SOURCE_H_
