// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_SYSTEM_LOGS_WRITER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_SYSTEM_LOGS_WRITER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"

// Helper function for writing system logs used in Feedback reports. Currently
// used by chrome://net-internals#chromeos for manual uploading of system logs.

namespace chromeos {
namespace system_logs_writer {

// Writes system_logs.txt.zip to |dest_dir|, containing the contents from
// Feedback reports. Runs |callback| on completion with the complete file path
// on success, or nullopt on failure.
void WriteSystemLogs(
    const base::FilePath& dest_dir,
    base::OnceCallback<void(base::Optional<base::FilePath>)> callback);

}  // namespace system_logs_writer
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_SYSTEM_LOGS_WRITER_H_
