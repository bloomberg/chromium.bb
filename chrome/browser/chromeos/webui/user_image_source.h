// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_USER_IMAGE_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_USER_IMAGE_SOURCE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"

namespace chromeos {

// UserImageSource is the data source that serves user images for users that
// have it.
class UserImageSource : public ChromeURLDataManager::DataSource {
 public:
  UserImageSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);

  virtual std::string GetMimeType(const std::string&) const;

  // Returns PNG encoded image for user with specified email.
  // If there's no user with such email, returns the default image.
  std::vector<unsigned char> GetUserImage(const std::string& email) const;

 private:
  virtual ~UserImageSource();

  DISALLOW_COPY_AND_ASSIGN(UserImageSource);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_USER_IMAGE_SOURCE_H_
