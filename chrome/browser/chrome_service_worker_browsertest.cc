// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests that Service Workers (a Content feature) work in the Chromium
// embedder.

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

class ChromeServiceWorkerTest : public InProcessBrowserTest {
 protected:
  ChromeServiceWorkerTest() {
    EXPECT_TRUE(service_worker_dir_.CreateUniqueTempDir());
  }

  void WriteFile(const base::FilePath::StringType& filename,
                 base::StringPiece contents) {
    EXPECT_EQ(base::checked_cast<int>(contents.size()),
              base::WriteFile(service_worker_dir_.path().Append(filename),
                              contents.data(),
                              contents.size()));
  }

  base::ScopedTempDir service_worker_dir_;
};

static void ExpectResultAndRun(bool expected,
                               const base::Closure& continuation,
                               bool actual) {
  EXPECT_EQ(expected, actual);
  continuation.Run();
}

// http://crbug.com/368570
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest,
                       CanShutDownWithRegisteredServiceWorker) {
  WriteFile(FILE_PATH_LITERAL("service_worker.js"), "");
  WriteFile(FILE_PATH_LITERAL("service_worker.js.mock-http-headers"),
            "HTTP/1.1 200 OK\nContent-Type: text/javascript");

  embedded_test_server()->ServeFilesFromDirectory(service_worker_dir_.path());
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  content::ServiceWorkerContext* sw_context =
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
          ->GetServiceWorkerContext();

  base::RunLoop run_loop;
  sw_context->RegisterServiceWorker(
      embedded_test_server()->GetURL("/*"),
      embedded_test_server()->GetURL("/service_worker.js"),
      base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();

  // Leave the Service Worker registered, and make sure that the browser can
  // shut down without DCHECK'ing. It'd be nice to check here that the SW is
  // actually occupying a process, but we don't yet have the public interface to
  // do that.
}

}  // namespace
