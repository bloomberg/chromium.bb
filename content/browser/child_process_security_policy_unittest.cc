// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/platform_file.h"
#include "content/browser/child_process_security_policy.h"
#include "content/common/test_url_constants.h"
#include "content/public/common/url_constants.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChildProcessSecurityPolicyTest : public testing::Test {
 protected:
  // testing::Test
  virtual void SetUp() {
    // In the real world, "chrome:" is a handled scheme.
    RegisterProtocolFactory(chrome::kChromeUIScheme,
                            &net::URLRequestTestJob::Factory);
  }
  virtual void TearDown() {
    RegisterProtocolFactory(chrome::kChromeUIScheme, NULL);
  }

  static net::URLRequest::ProtocolFactory* RegisterProtocolFactory(
      const std::string& scheme,
      net::URLRequest::ProtocolFactory* factory) {
    return net::URLRequest::Deprecated::RegisterProtocolFactory(
        scheme, factory);
  }
};

static int kRendererID = 42;
static int kWorkerRendererID = kRendererID + 1;

TEST_F(ChildProcessSecurityPolicyTest, IsWebSafeSchemeTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  EXPECT_TRUE(p->IsWebSafeScheme(chrome::kHttpScheme));
  EXPECT_TRUE(p->IsWebSafeScheme(chrome::kHttpsScheme));
  EXPECT_TRUE(p->IsWebSafeScheme(chrome::kFtpScheme));
  EXPECT_TRUE(p->IsWebSafeScheme(chrome::kDataScheme));
  EXPECT_TRUE(p->IsWebSafeScheme("feed"));
  EXPECT_TRUE(p->IsWebSafeScheme(chrome::kBlobScheme));
  EXPECT_TRUE(p->IsWebSafeScheme(chrome::kFileSystemScheme));

  EXPECT_FALSE(p->IsWebSafeScheme("registered-web-safe-scheme"));
  p->RegisterWebSafeScheme("registered-web-safe-scheme");
  EXPECT_TRUE(p->IsWebSafeScheme("registered-web-safe-scheme"));

  EXPECT_FALSE(p->IsWebSafeScheme(chrome::kChromeUIScheme));
}

TEST_F(ChildProcessSecurityPolicyTest, IsPseudoSchemeTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  EXPECT_TRUE(p->IsPseudoScheme(chrome::kAboutScheme));
  EXPECT_TRUE(p->IsPseudoScheme(chrome::kJavaScriptScheme));
  EXPECT_TRUE(p->IsPseudoScheme(chrome::kViewSourceScheme));

  EXPECT_FALSE(p->IsPseudoScheme("registered-pseudo-scheme"));
  p->RegisterPseudoScheme("registered-pseudo-scheme");
  EXPECT_TRUE(p->IsPseudoScheme("registered-pseudo-scheme"));

  EXPECT_FALSE(p->IsPseudoScheme(chrome::kChromeUIScheme));
}

TEST_F(ChildProcessSecurityPolicyTest, IsDisabledSchemeTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  EXPECT_FALSE(p->IsDisabledScheme("evil-scheme"));
  std::set<std::string> disabled_set;
  disabled_set.insert("evil-scheme");
  p->RegisterDisabledSchemes(disabled_set);
  EXPECT_TRUE(p->IsDisabledScheme("evil-scheme"));
  EXPECT_FALSE(p->IsDisabledScheme("good-scheme"));

  disabled_set.clear();
  p->RegisterDisabledSchemes(disabled_set);
  EXPECT_FALSE(p->IsDisabledScheme("evil-scheme"));
  EXPECT_FALSE(p->IsDisabledScheme("good-scheme"));
}

TEST_F(ChildProcessSecurityPolicyTest, StandardSchemesTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  // Safe
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("http://www.google.com/")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("https://www.paypal.com/")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("ftp://ftp.gnu.org/")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("data:text/html,<b>Hi</b>")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID,
                               GURL("view-source:http://www.google.com/")));
  EXPECT_TRUE(p->CanRequestURL(
      kRendererID, GURL("filesystem:http://localhost/temporary/a.gif")));

  // Dangerous
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("chrome://foo/bar")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, AboutTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("about:blank")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("about:BlAnK")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("aBouT:BlAnK")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("aBouT:blank")));

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:memory")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:crash")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:cache")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:hang")));

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("aBoUt:memory")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:CrASh")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("abOuT:cAChe")));

  // These requests for about: pages should be denied.
  p->GrantRequestURL(kRendererID, GURL(chrome::kTestGpuCleanURL));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL(chrome::kTestGpuCleanURL)));

  p->GrantRequestURL(kRendererID, GURL(chrome::kAboutCrashURL));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL(chrome::kAboutCrashURL)));

  p->GrantRequestURL(kRendererID, GURL(chrome::kTestCacheURL));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL(chrome::kTestCacheURL)));

  p->GrantRequestURL(kRendererID, GURL(chrome::kTestHangURL));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL(chrome::kTestHangURL)));

  // These requests for chrome:// pages should be granted.
  p->GrantRequestURL(kRendererID, GURL(chrome::kTestNewTabURL));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL(chrome::kTestNewTabURL)));

  p->GrantRequestURL(kRendererID, GURL(chrome::kTestHistoryURL));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL(chrome::kTestHistoryURL)));

  p->GrantRequestURL(kRendererID, GURL(chrome::kTestBookmarksURL));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL(chrome::kTestBookmarksURL)));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, JavaScriptTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("javascript:alert('xss')")));
  p->GrantRequestURL(kRendererID, GURL("javascript:alert('xss')"));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("javascript:alert('xss')")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, RegisterWebSafeSchemeTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  // Currently, "asdf" is destined for ShellExecute, so it is allowed.
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));

  // Once we register a ProtocolFactory for "asdf", we default to deny.
  RegisterProtocolFactory("asdf", &net::URLRequestTestJob::Factory);
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));

  // We can allow new schemes by adding them to the whitelist.
  p->RegisterWebSafeScheme("asdf");
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));

  // Cleanup.
  RegisterProtocolFactory("asdf", NULL);
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, CanServiceCommandsTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  p->GrantRequestURL(kRendererID, GURL("file:///etc/passwd"));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));

  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("evil-scheme:/path")));
  std::set<std::string> disabled_set;
  disabled_set.insert("evil-scheme");
  p->RegisterDisabledSchemes(disabled_set);
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("http://www.google.com")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("evil-scheme:/path")));
  disabled_set.clear();
  p->RegisterDisabledSchemes(disabled_set);
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("http://www.google.com")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("evil-scheme:/path")));

  // We should forget our state if we repeat a renderer id.
  p->Remove(kRendererID);
  p->Add(kRendererID);
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, ViewSource) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  // View source is determined by the embedded scheme.
  EXPECT_TRUE(p->CanRequestURL(kRendererID,
                               GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(
      kRendererID, GURL("view-source:view-source:http://www.google.com/")));

  p->GrantRequestURL(kRendererID, GURL("view-source:file:///etc/passwd"));
  // View source needs to be able to request the embedded scheme.
  EXPECT_TRUE(p->CanRequestURL(kRendererID,
                               GURL("view-source:file:///etc/passwd")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, CanReadFiles) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  EXPECT_FALSE(p->CanReadFile(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/passwd"))));
  p->GrantReadFile(kRendererID, FilePath(FILE_PATH_LITERAL("/etc/passwd")));
  EXPECT_TRUE(p->CanReadFile(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/passwd"))));
  EXPECT_FALSE(p->CanReadFile(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/shadow"))));

  p->Remove(kRendererID);
  p->Add(kRendererID);

  EXPECT_FALSE(p->CanReadFile(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/passwd"))));
  EXPECT_FALSE(p->CanReadFile(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/shadow"))));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, CanReadDirectories) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  EXPECT_FALSE(p->CanReadDirectory(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/"))));
  p->GrantReadDirectory(kRendererID, FilePath(FILE_PATH_LITERAL("/etc/")));
  EXPECT_TRUE(p->CanReadDirectory(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/"))));
  EXPECT_TRUE(p->CanReadFile(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/passwd"))));

  p->Remove(kRendererID);
  p->Add(kRendererID);

  EXPECT_FALSE(p->CanReadDirectory(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/"))));
  EXPECT_FALSE(p->CanReadFile(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/passwd"))));

  // Just granting read permission as a file doesn't imply reading as a
  // directory.
  p->GrantReadFile(kRendererID, FilePath(FILE_PATH_LITERAL("/etc/")));
  EXPECT_TRUE(p->CanReadFile(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/passwd"))));
  EXPECT_FALSE(p->CanReadDirectory(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/"))));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, FilePermissions) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  // Grant permissions for a file.
  p->Add(kRendererID);
  FilePath file = FilePath(FILE_PATH_LITERAL("/etc/passwd"));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, file,
                                        base::PLATFORM_FILE_OPEN));

  p->GrantPermissionsForFile(kRendererID, file,
                             base::PLATFORM_FILE_OPEN |
                             base::PLATFORM_FILE_OPEN_TRUNCATED |
                             base::PLATFORM_FILE_READ |
                             base::PLATFORM_FILE_WRITE);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, file,
                                       base::PLATFORM_FILE_OPEN |
                                       base::PLATFORM_FILE_OPEN_TRUNCATED |
                                       base::PLATFORM_FILE_READ |
                                       base::PLATFORM_FILE_WRITE));
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, file,
                                       base::PLATFORM_FILE_OPEN |
                                       base::PLATFORM_FILE_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, file,
                                        base::PLATFORM_FILE_CREATE));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, file,
                                        base::PLATFORM_FILE_CREATE |
                                        base::PLATFORM_FILE_OPEN_TRUNCATED |
                                        base::PLATFORM_FILE_READ |
                                        base::PLATFORM_FILE_WRITE));
  p->Remove(kRendererID);

  // Grant permissions for the directory the file is in.
  p->Add(kRendererID);
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, file,
                                        base::PLATFORM_FILE_OPEN));
  p->GrantPermissionsForFile(kRendererID, FilePath(FILE_PATH_LITERAL("/etc")),
                             base::PLATFORM_FILE_OPEN |
                             base::PLATFORM_FILE_READ);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, file,
                                        base::PLATFORM_FILE_OPEN));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, file,
                                        base::PLATFORM_FILE_READ |
                                        base::PLATFORM_FILE_WRITE));
  p->Remove(kRendererID);

  // Grant permissions for the directory the file is in (with trailing '/').
  p->Add(kRendererID);
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, file,
                                        base::PLATFORM_FILE_OPEN));
  p->GrantPermissionsForFile(kRendererID, FilePath(FILE_PATH_LITERAL("/etc/")),
                             base::PLATFORM_FILE_OPEN |
                             base::PLATFORM_FILE_READ);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, file,
                                        base::PLATFORM_FILE_OPEN));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, file,
                                        base::PLATFORM_FILE_READ |
                                        base::PLATFORM_FILE_WRITE));

  // Grant permissions for the file (should overwrite the permissions granted
  // for the directory).
  p->GrantPermissionsForFile(kRendererID, file, base::PLATFORM_FILE_TEMPORARY);
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, file,
                                        base::PLATFORM_FILE_OPEN));
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, file,
                                       base::PLATFORM_FILE_TEMPORARY));

  // Revoke all permissions for the file (it should inherit its permissions
  // from the directory again).
  p->RevokeAllPermissionsForFile(kRendererID, file);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, file,
                                       base::PLATFORM_FILE_OPEN |
                                       base::PLATFORM_FILE_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, file,
                                        base::PLATFORM_FILE_TEMPORARY));
  p->Remove(kRendererID);

  // Grant file permissions for the file to main thread renderer process,
  // make sure its worker thread renderer process inherits those.
  p->Add(kRendererID);
  p->GrantPermissionsForFile(kRendererID, file, base::PLATFORM_FILE_OPEN |
                                                base::PLATFORM_FILE_READ);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, file,
                                       base::PLATFORM_FILE_OPEN |
                                       base::PLATFORM_FILE_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, file,
                                       base::PLATFORM_FILE_WRITE));
  p->AddWorker(kWorkerRendererID, kRendererID);
  EXPECT_TRUE(p->HasPermissionsForFile(kWorkerRendererID, file,
                                       base::PLATFORM_FILE_OPEN |
                                       base::PLATFORM_FILE_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kWorkerRendererID, file,
                                        base::PLATFORM_FILE_WRITE));
  p->Remove(kRendererID);
  EXPECT_FALSE(p->HasPermissionsForFile(kWorkerRendererID, file,
                                        base::PLATFORM_FILE_OPEN |
                                        base::PLATFORM_FILE_READ));
  p->Remove(kWorkerRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, CanServiceWebUIBindings) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  GURL url("chrome://thumb/http://www.google.com/");

  p->Add(kRendererID);

  EXPECT_FALSE(p->HasWebUIBindings(kRendererID));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
  p->GrantWebUIBindings(kRendererID);
  EXPECT_TRUE(p->HasWebUIBindings(kRendererID));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, url));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, RemoveRace) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  GURL url("file:///etc/passwd");
  FilePath file(FILE_PATH_LITERAL("/etc/passwd"));

  p->Add(kRendererID);

  p->GrantRequestURL(kRendererID, url);
  p->GrantReadFile(kRendererID, file);
  p->GrantWebUIBindings(kRendererID);

  EXPECT_TRUE(p->CanRequestURL(kRendererID, url));
  EXPECT_TRUE(p->CanReadFile(kRendererID, file));
  EXPECT_TRUE(p->HasWebUIBindings(kRendererID));

  p->Remove(kRendererID);

  // Renderers are added and removed on the UI thread, but the policy can be
  // queried on the IO thread.  The ChildProcessSecurityPolicy needs to be
  // prepared to answer policy questions about renderers who no longer exist.

  // In this case, we default to secure behavior.
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
  EXPECT_FALSE(p->CanReadFile(kRendererID, file));
  EXPECT_FALSE(p->HasWebUIBindings(kRendererID));
}
