// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_DESTINATION_FILE_SYSTEM_H_
#define CHROME_BROWSER_UI_PASSWORDS_DESTINATION_FILE_SYSTEM_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/password_manager/core/browser/export/destination.h"

class DestinationFileSystem : public password_manager::Destination {
 public:
  explicit DestinationFileSystem(base::FilePath destination_path);
  ~DestinationFileSystem() override = default;

  // password_manager::Destination
  base::string16 Write(const std::string& data) override;

  // Get this instance's target.
  const base::FilePath& GetDestinationPathForTesting();

 private:
  // The file, to which the data will be written.
  const base::FilePath destination_path_;

  DISALLOW_COPY_AND_ASSIGN(DestinationFileSystem);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_DESTINATION_FILE_SYSTEM_H_
