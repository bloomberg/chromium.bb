// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SCOPED_FILE_OPENER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SCOPED_FILE_OPENER_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
namespace file_system_provider {

// Opens files and guarantees that they will be closed when the object gets out
// of scope. Use instead of manually calling OpenFile and CloseFile, as aborting
// of the OpenFile operation is racey and the operation may or may not be
// aborted.
class ScopedFileOpener {
 public:
  ScopedFileOpener(ProvidedFileSystemInterface* file_system,
                   const base::FilePath& file_path,
                   OpenFileMode mode,
                   ProvidedFileSystemInterface::OpenFileCallback callback);
  ~ScopedFileOpener();

 private:
  class Runner;
  scoped_refptr<Runner> runner_;
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SCOPED_FILE_OPENER_H_
