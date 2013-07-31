// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_file_host.h"

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/nacl_host/nacl_browser.h"
#include "components/nacl/common/nacl_browser_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using nacl_file_host::PnaclCanOpenFile;

class DummyNaClBrowserDelegate : public NaClBrowserDelegate {
 public:
  virtual void ShowNaClInfobar(int render_process_id,
                               int render_view_id,
                               int error_id) OVERRIDE {
  }

  virtual bool DialogsAreSuppressed() OVERRIDE {
    return false;
  }

  virtual bool GetCacheDirectory(base::FilePath* cache_dir) OVERRIDE {
    return false;
  }

  virtual bool GetPluginDirectory(base::FilePath* plugin_dir) OVERRIDE {
    return false;
  }

  virtual bool GetPnaclDirectory(base::FilePath* pnacl_dir) OVERRIDE {
    *pnacl_dir = pnacl_path_;
    return true;
  }

  virtual bool GetUserDirectory(base::FilePath* user_dir) OVERRIDE {
    return false;
  }

  virtual std::string GetVersionString() const OVERRIDE {
    return std::string();
  }

  virtual ppapi::host::HostFactory* CreatePpapiHostFactory(
      content::BrowserPpapiHost* ppapi_host) OVERRIDE {
    return NULL;
  }

  virtual void TryInstallPnacl(
      const base::Callback<void(bool)>& installed) OVERRIDE {
    NOTREACHED();
  }

  void SetPnaclDirectory(const base::FilePath& pnacl_dir) {
    pnacl_path_ = pnacl_dir;
  }

 private:
  base::FilePath pnacl_path_;
};

// Try to pass a few funny filenames with a dummy pnacl directory set.
TEST(PnaclFileHostTest, TestFilenamesWithPnaclPath) {
  base::ScopedTempDir scoped_tmp_dir;
  ASSERT_TRUE(scoped_tmp_dir.CreateUniqueTempDir());

  base::FilePath kDummyPnaclPath = scoped_tmp_dir.path();

  DummyNaClBrowserDelegate* nacl_browser_delegate =
      new DummyNaClBrowserDelegate;
  nacl_browser_delegate->SetPnaclDirectory(kDummyPnaclPath);
  NaClBrowser::SetDelegate(nacl_browser_delegate);
  ASSERT_TRUE(NaClBrowser::GetDelegate()->GetPnaclDirectory(&kDummyPnaclPath));

  // Check allowed strings, and check that the expected prefix is added.
  base::FilePath out_path;
  EXPECT_TRUE(PnaclCanOpenFile("pnacl_json", &out_path));
  base::FilePath expected_path = kDummyPnaclPath.Append(
      FILE_PATH_LITERAL("pnacl_public_pnacl_json"));
  EXPECT_EQ(out_path, expected_path);

  EXPECT_TRUE(PnaclCanOpenFile("x86_32_llc", &out_path));
  expected_path = kDummyPnaclPath.Append(
      FILE_PATH_LITERAL("pnacl_public_x86_32_llc"));
  EXPECT_EQ(out_path, expected_path);

  // Check character ranges.
  EXPECT_FALSE(PnaclCanOpenFile(".xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("/xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("x/chars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("\\xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("x\\chars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("$xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("%xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("CAPS", &out_path));
  const char non_ascii[] = "\xff\xfe";
  EXPECT_FALSE(PnaclCanOpenFile(non_ascii, &out_path));

  // Check file length restriction.
  EXPECT_FALSE(PnaclCanOpenFile("thisstringisactuallywaaaaaaaaaaaaaaaaaaaaaaaa"
                                "toolongwaytoolongwaaaaayyyyytoooooooooooooooo"
                                "looooooooong",
                                &out_path));

  // Other bad files.
  EXPECT_FALSE(PnaclCanOpenFile(std::string(), &out_path));
  EXPECT_FALSE(PnaclCanOpenFile(".", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("..", &out_path));
#if defined(OS_WIN)
  EXPECT_FALSE(PnaclCanOpenFile("..\\llc", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("%SystemRoot%", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("%SystemRoot%\\explorer.exe", &out_path));
#else
  EXPECT_FALSE(PnaclCanOpenFile("../llc", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("/bin/sh", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("$HOME", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("$HOME/.bashrc", &out_path));
#endif
  NaClBrowser::SetDelegate(NULL);
}
