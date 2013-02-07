// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/layout_browsertest.h"

namespace content {

class BlobLayoutTest : public InProcessBrowserLayoutTest {
 public:
  BlobLayoutTest() :
      InProcessBrowserLayoutTest(
          base::FilePath(),
          base::FilePath(FILE_PATH_LITERAL("fast/files")).
              NormalizePathSeparators()) {
  }
};

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, SliceTests) {
  RunLayoutTest("blob-slice-test.html");
}

// crbug.com/169240 for disabling on Windows, crbug.com/173079 for temporary
// disabling everywhere.
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_ApplyBlobUrlToImg) {
  RunLayoutTest("apply-blob-url-to-img.html");
}

// crbug.com/169240 for disabling on Windows, crbug.com/173079 for temporary
// disabling everywhere.
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_ApplyBlobUrlToXhr) {
  RunLayoutTest("apply-blob-url-to-xhr.html");
}

// webkit.org/b/85174
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_BlobConstructor) {
  RunLayoutTest("blob-constructor.html");
}

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, BlobSliceOverflow) {
  RunLayoutTest("blob-slice-overflow.html");
}

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, BlobSliceTest) {
  RunLayoutTest("blob-slice-test.html");
}

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, CreateBlobUrlCrash) {
  RunLayoutTest("create-blob-url-crash.html");
}

// crbug.com/170155
IN_PROC_BROWSER_TEST_F(BlobLayoutTest,
    DISABLED_DomurlScriptExecutionContextCrash) {
  RunLayoutTest("domurl-script-execution-context-crash.html");
}

// crbug.com/169240 for disabling on Windows, crbug.com/173079 for temporary
// disabling everywhere.
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_FileListTest) {
  RunLayoutTest("file-list-test.html");
}

// crbug.com/169240 for disabling on Windows, crbug.com/173079 for temporary
// disabling everywhere.
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_FileReaderAbort) {
  RunLayoutTest("file-reader-abort.html");
}

// crbug.com/169240 for disabling on Windows, crbug.com/173079 for temporary
// disabling everywhere.
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_FileReaderDirectoryCrash) {
  RunLayoutTest("file-reader-directory-crash.html");
}

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, FileReaderDoneReadingAbort) {
  RunLayoutTest("file-reader-done-reading-abort.html");
}

// crbug.com/169240 for disabling on Windows, crbug.com/173079 for temporary
// disabling everywhere.
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_FileReaderEventListener) {
  RunLayoutTest("file-reader-event-listener.html");
}

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, FileReaderFffd) {
  RunLayoutTest("file-reader-fffd.html");
}

// crbug.com/170154
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_FileReaderFileUrl) {
  RunLayoutTest("file-reader-file-url.html");
}

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, FileReaderImmediateAbort) {
  RunLayoutTest("file-reader-immediate-abort.html");
}

// crbug.com/170154
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_FileReaderSandboxIframe) {
  RunLayoutTest("file-reader-sandbox-iframe.html");
}

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, NotEnoughArguments) {
  RunLayoutTest("not-enough-arguments.html");
}

// crbug.com/169240 for disabling on Windows, crbug.com/173079 for temporary
// disabling everywhere.
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_NullOriginString) {
  RunLayoutTest("null-origin-string.html");
}

// crbug.com/169240 for disabling on Windows, crbug.com/173079 for temporary
// disabling everywhere.
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_ReadBlobAsync) {
  RunLayoutTest("read-blob-async.html");
}

// crbug.com/169240 for disabling on Windows, crbug.com/173079 for temporary
// disabling everywhere.
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_ReadFileAsync) {
  RunLayoutTest("read-file-async.html");
}

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, RevokeBlobUrl) {
  RunLayoutTest("revoke-blob-url.html");
}

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, UrlNull) {
  RunLayoutTest("url-null.html");
}

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, UrlRequiredArguments) {
  RunLayoutTest("url-required-arguments.html");
}

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, XhrResponseBlob) {
  RunLayoutTest("xhr-response-blob.html");
}

// crbug.com/170154
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_WorkersInlineWorkerViaBlobUrl) {
  RunLayoutTest("workers/inline-worker-via-blob-url.html");
}

// webkit.org/b/101099
IN_PROC_BROWSER_TEST_F(BlobLayoutTest,
    DISABLED_WorkersWorkerApplyBlobUrlToXhr) {
  RunLayoutTest("workers/worker-apply-blob-url-to-xhr.html");
}

// webkit.org/b/79540
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_WorkersWorkerReadBlobAsync) {
  RunLayoutTest("workers/worker-read-blob-async.html");
}

// webkit.org/b/79540
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_WorkersWorkerReadBlobSync) {
  RunLayoutTest("workers/worker-read-blob-sync.html");
}

// crbug.com/170152
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_WorkersWorkerReadFileAsync) {
  RunLayoutTest("workers/worker-read-file-async.html");
}

// crbug.com/170152
IN_PROC_BROWSER_TEST_F(BlobLayoutTest, DISABLED_WorkersWorkerReadFileSync) {
  RunLayoutTest("workers/worker-read-file-sync.html");
}

}  // namespace content
