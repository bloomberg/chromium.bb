// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "content/common/sandbox_mac.h"
#include "content/common/sandbox_mac_unittest_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

//--------------------- Clipboard Sandboxing ----------------------
// Test case for checking sandboxing of clipboard access.
class MacSandboxedClipboardTestCase : public MacSandboxTestCase {
 public:
  MacSandboxedClipboardTestCase();
  virtual ~MacSandboxedClipboardTestCase();

  virtual bool SandboxedTest() OVERRIDE;

  virtual void SetTestData(const char* test_data) OVERRIDE;
 private:
  NSString* clipboard_name_;
};

REGISTER_SANDBOX_TEST_CASE(MacSandboxedClipboardTestCase);

MacSandboxedClipboardTestCase::MacSandboxedClipboardTestCase() :
    clipboard_name_(nil) {}

MacSandboxedClipboardTestCase::~MacSandboxedClipboardTestCase() {
  [clipboard_name_ release];
}

bool MacSandboxedClipboardTestCase::SandboxedTest() {
  // Shouldn't be able to open the pasteboard in the sandbox.

  if ([clipboard_name_ length] == 0) {
    LOG(ERROR) << "Clipboard name is empty";
    return false;
  }

  NSPasteboard* pb = [NSPasteboard pasteboardWithName:clipboard_name_];
  if (pb != nil) {
    LOG(ERROR) << "Was able to access named clipboard";
    return false;
  }

  pb = [NSPasteboard generalPasteboard];
  if (pb != nil) {
    LOG(ERROR) << "Was able to access system clipboard";
    return false;
  }

  return true;
}

void MacSandboxedClipboardTestCase::SetTestData(const char* test_data) {
  clipboard_name_ = [base::SysUTF8ToNSString(test_data) retain];
}

TEST_F(MacSandboxTest, ClipboardAccess) {
  NSPasteboard* pb = [NSPasteboard pasteboardWithUniqueName];
  EXPECT_EQ([[pb types] count], 0U);

  std::string pasteboard_name = base::SysNSStringToUTF8([pb name]);
  EXPECT_TRUE(RunTestInAllSandboxTypes("MacSandboxedClipboardTestCase",
                  pasteboard_name.c_str()));

  // After executing the test, the clipboard should still be empty.
  EXPECT_EQ([[pb types] count], 0U);
}

//--------------------- File Access Sandboxing ----------------------
// Test case for checking sandboxing of filesystem apis.
class MacSandboxedFileAccessTestCase : public MacSandboxTestCase {
 public:
  virtual bool SandboxedTest() OVERRIDE;
};

REGISTER_SANDBOX_TEST_CASE(MacSandboxedFileAccessTestCase);

bool MacSandboxedFileAccessTestCase::SandboxedTest() {
  int fdes = open("/etc/passwd", O_RDONLY);
  file_util::ScopedFD file_closer(&fdes);
  return fdes == -1;
}

TEST_F(MacSandboxTest, FileAccess) {
  EXPECT_TRUE(RunTestInAllSandboxTypes("MacSandboxedFileAccessTestCase", NULL));
}

//--------------------- /dev/urandom Sandboxing ----------------------
// /dev/urandom is available to ppapi sandbox only.
class MacSandboxedUrandomTestCase : public MacSandboxTestCase {
 public:
  virtual bool SandboxedTest() OVERRIDE;
};

REGISTER_SANDBOX_TEST_CASE(MacSandboxedUrandomTestCase);

bool MacSandboxedUrandomTestCase::SandboxedTest() {
  int fdes = open("/dev/urandom", O_RDONLY);
  file_util::ScopedFD file_closer(&fdes);

  // Open succeeds under ppapi sandbox, else it is not permitted.
  if (test_data_ == "ppapi") {
    if (fdes == -1)
      return false;

    char buf[16];
    int rc = read(fdes, buf, sizeof(buf));
    return rc == sizeof(buf);
  } else {
    return fdes == -1 && errno == EPERM;
  }
}

TEST_F(MacSandboxTest, UrandomAccess) {
  // Similar to RunTestInAllSandboxTypes(), except changing
  // |test_data| for the ppapi case.  Passing "" in the non-ppapi case
  // to overwrite the test data (NULL means not to change it).
  for (SandboxType i = SANDBOX_TYPE_FIRST_TYPE;
       i < SANDBOX_TYPE_AFTER_LAST_TYPE; ++i) {
    if (i == SANDBOX_TYPE_PPAPI) {
      EXPECT_TRUE(RunTestInSandbox(i, "MacSandboxedUrandomTestCase", "ppapi"));
    } else {
      EXPECT_TRUE(RunTestInSandbox(i, "MacSandboxedUrandomTestCase", ""))
          << "for sandbox type " << i;
    }
  }
}

}  // namespace content
