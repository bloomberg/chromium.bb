// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test_utils.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "net/base/net_util.h"

namespace content {

FilePath GetTestFilePath(const char* dir, const char* file) {
  FilePath path;
  PathService::Get(DIR_TEST_DATA, &path);
  return path.Append(
      FilePath().AppendASCII(dir).Append(FilePath().AppendASCII(file)));
}

GURL GetTestUrl(const char* dir, const char* file) {
  return net::FilePathToFileURL(GetTestFilePath(dir, file));
}

void NavigateToURL(Shell* window, const GURL& url) {
  NavigationController* controller = &window->web_contents()->GetController();
  TestNavigationObserver same_tab_observer(
      Source<NavigationController>(controller),
      NULL,
      1);

  window->LoadURL(url);

  base::RunLoop run_loop;
  same_tab_observer.WaitForObservation(
      base::Bind(&RunThisRunLoop, base::Unretained(&run_loop)),
      GetQuitTaskForRunLoop(&run_loop));
}

}  // namespace content
