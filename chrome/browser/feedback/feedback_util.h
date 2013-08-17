// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_FEEDBACK_UTIL_H_
#define CHROME_BROWSER_FEEDBACK_FEEDBACK_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/feedback/proto/common.pb.h"
#include "chrome/browser/feedback/proto/dom.pb.h"
#include "chrome/browser/feedback/proto/extension.pb.h"
#include "chrome/browser/feedback/proto/math.pb.h"
#include "ui/gfx/rect.h"

#if defined(OS_MACOSX)
#include "base/sys_info.h"
#elif defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

class FeedbackData;
class Profile;

namespace content {
class WebContents;
}

namespace chrome {
extern const char kAppLauncherCategoryTag[];
}  // namespace chrome

namespace feedback_util {

  void SendReport(scoped_refptr<FeedbackData> data);
  bool ZipString(const std::string& logs, std::string* compressed_logs);

}  // namespace feedback_util

#endif  // CHROME_BROWSER_FEEDBACK_FEEDBACK_UTIL_H_
