// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/layout_browsertest.h"

namespace content {

class IndexedDBLayoutTest : public InProcessBrowserLayoutTest {
 public:
  IndexedDBLayoutTest() : InProcessBrowserLayoutTest(
      base::FilePath(),
      base::FilePath().AppendASCII("storage").AppendASCII("indexeddb")) {
  }

  void RunLayoutTests(const char* file_names[]) {
    for (size_t i = 0; file_names[i]; i++)
      RunLayoutTest(file_names[i]);
  }
};

namespace {

static const char* kBasicTests[] = {
  "basics.html",
  "basics-shared-workers.html",
  // Failing on Precise bot (crbug.com/145592).
  // "basics-workers.html",
  "database-basics.html",
  "factory-basics.html",
  "index-basics.html",
  "objectstore-basics.html",
  NULL
};

static const char* kComplexTests[] = {
  "prefetch-bugfix-108071.html",
  // Flaky: http://crbug.com/123685
  // "pending-version-change-stuck-works-with-terminate.html",
  "pending-version-change-on-exit.html",
  NULL
};

static const char* kIndexTests[] = {
  "deleteIndex.html",
  // Flaky: http://crbug.com/123685
  // "index-basics-workers.html",
  "index-count.html",
  "index-cursor.html",
  "index-get-key-argument-required.html",
  "index-multientry.html",
  "index-population.html",
  "index-unique.html",
  NULL
};

static const char* kKeyTests[] = {
  "key-generator.html",
  "keypath-basics.html",
  "keypath-edges.html",
  "keypath-fetch-key.html",
  "keyrange.html",
  "keyrange-required-arguments.html",
  "key-sort-order-across-types.html",
  "key-sort-order-date.html",
  "key-type-array.html",
  "key-type-infinity.html",
  "invalid-keys.html",
  NULL
};

static const char* kTransactionTests[] = {
  "transaction-abort.html",
  "transaction-complete-with-js-recursion-cross-frame.html",
  "transaction-complete-with-js-recursion.html",
  "transaction-complete-workers.html",
  "transaction-after-close.html",
  "transaction-and-objectstore-calls.html",
  "transaction-basics.html",
  "transaction-crash-on-abort.html",
  "transaction-event-propagation.html",
  "transaction-read-only.html",
  "transaction-rollback.html",
  "transaction-storeNames-required.html",
  NULL
};

static const char* kRegressionTests[] = {
  "dont-commit-on-blocked.html",
  NULL
};

const char* kIntVersionTests1[] = {
  "intversion-abort-in-initial-upgradeneeded.html",
  "intversion-blocked.html",
  "intversion-close-between-events.html",
  "intversion-close-in-oncomplete.html",
  "intversion-close-in-upgradeneeded.html",
  "delete-in-upgradeneeded-close-in-open-success.html",
  NULL
};

const char* kIntVersionTests2[] = {
  "delete-in-upgradeneeded-close-in-versionchange.html",
  "intversion-gated-on-delete.html",
  "intversion-long-queue.html",
  "intversion-omit-parameter.html",
  "intversion-open-with-version.html",
  "intversion-upgrades.html",
  NULL
};

}

IN_PROC_BROWSER_TEST_F(IndexedDBLayoutTest, BasicTests) {
  RunLayoutTests(kBasicTests);
}


IN_PROC_BROWSER_TEST_F(IndexedDBLayoutTest, ComplexTests) {
  RunLayoutTests(kComplexTests);
}

IN_PROC_BROWSER_TEST_F(IndexedDBLayoutTest, IndexTests) {
  RunLayoutTests(kIndexTests);
}

IN_PROC_BROWSER_TEST_F(IndexedDBLayoutTest, KeyTests) {
  RunLayoutTests(kKeyTests);
}

IN_PROC_BROWSER_TEST_F(IndexedDBLayoutTest, TransactionTests) {
  RunLayoutTests(kTransactionTests);
}

// http://crbug.com/223101
#if defined(OS_LINUX) && !defined(NDEBUG)
#define MAYBE_IntVersionTests1  DISABLED_IntVersionTests1
#else
#define MAYBE_IntVersionTests1  IntVersionTests1
#endif
IN_PROC_BROWSER_TEST_F(IndexedDBLayoutTest, MAYBE_IntVersionTests1) {
  RunLayoutTests(kIntVersionTests1);
}

IN_PROC_BROWSER_TEST_F(IndexedDBLayoutTest, IntVersionTests2) {
  RunLayoutTests(kIntVersionTests2);
}

IN_PROC_BROWSER_TEST_F(IndexedDBLayoutTest, RegressionTests) {
  RunLayoutTests(kRegressionTests);
}

}  // namespace content
