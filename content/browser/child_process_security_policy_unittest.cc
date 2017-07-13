// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/common/url_constants.h"
#include "content/test/test_content_browser_client.h"
#include "storage/browser/fileapi/file_permission_policy.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/common/fileapi/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

const int kRendererID = 42;
const int kWorkerRendererID = kRendererID + 1;

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define TEST_PATH(x) FILE_PATH_LITERAL("c:") FILE_PATH_LITERAL(x)
#else
#define TEST_PATH(x) FILE_PATH_LITERAL(x)
#endif

class ChildProcessSecurityPolicyTestBrowserClient
    : public TestContentBrowserClient {
 public:
  ChildProcessSecurityPolicyTestBrowserClient() {}

  bool IsHandledURL(const GURL& url) override {
    return schemes_.find(url.scheme()) != schemes_.end();
  }

  void ClearSchemes() {
    schemes_.clear();
  }

  void AddScheme(const std::string& scheme) {
    schemes_.insert(scheme);
  }

 private:
  std::set<std::string> schemes_;
};

}  // namespace

class ChildProcessSecurityPolicyTest : public testing::Test {
 public:
  ChildProcessSecurityPolicyTest() : old_browser_client_(NULL) {
  }

  void SetUp() override {
    old_browser_client_ = SetBrowserClientForTesting(&test_browser_client_);

    // Claim to always handle chrome:// URLs because the CPSP's notion of
    // allowing WebUI bindings is hard-wired to this particular scheme.
    test_browser_client_.AddScheme(kChromeUIScheme);

    // Claim to always handle file:// URLs like the browser would.
    // net::URLRequest::IsHandledURL() no longer claims support for default
    // protocols as this is the responsibility of the browser (which is
    // responsible for adding the appropriate ProtocolHandler).
    test_browser_client_.AddScheme(url::kFileScheme);
  }

  void TearDown() override {
    test_browser_client_.ClearSchemes();
    SetBrowserClientForTesting(old_browser_client_);
  }

 protected:
  void RegisterTestScheme(const std::string& scheme) {
    test_browser_client_.AddScheme(scheme);
  }

  void GrantPermissionsForFile(ChildProcessSecurityPolicyImpl* p,
                               int child_id,
                               const base::FilePath& file,
                               int permissions) {
    p->GrantPermissionsForFile(child_id, file, permissions);
  }

  void CheckHasNoFileSystemPermission(ChildProcessSecurityPolicyImpl* p,
                                      const std::string& child_id) {
    EXPECT_FALSE(p->CanReadFileSystem(kRendererID, child_id));
    EXPECT_FALSE(p->CanReadWriteFileSystem(kRendererID, child_id));
    EXPECT_FALSE(p->CanCopyIntoFileSystem(kRendererID, child_id));
    EXPECT_FALSE(p->CanDeleteFromFileSystem(kRendererID, child_id));
  }

  void CheckHasNoFileSystemFilePermission(ChildProcessSecurityPolicyImpl* p,
                                          const base::FilePath& file,
                                          const storage::FileSystemURL& url) {
    EXPECT_FALSE(p->CanReadFile(kRendererID, file));
    EXPECT_FALSE(p->CanCreateReadWriteFile(kRendererID, file));
    EXPECT_FALSE(p->CanReadFileSystemFile(kRendererID, url));
    EXPECT_FALSE(p->CanWriteFileSystemFile(kRendererID, url));
    EXPECT_FALSE(p->CanCreateFileSystemFile(kRendererID, url));
    EXPECT_FALSE(p->CanCreateReadWriteFileSystemFile(kRendererID, url));
    EXPECT_FALSE(p->CanCopyIntoFileSystemFile(kRendererID, url));
    EXPECT_FALSE(p->CanDeleteFileSystemFile(kRendererID, url));
  }

 private:
  ChildProcessSecurityPolicyTestBrowserClient test_browser_client_;
  ContentBrowserClient* old_browser_client_;
};


TEST_F(ChildProcessSecurityPolicyTest, IsWebSafeSchemeTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  EXPECT_TRUE(p->IsWebSafeScheme(url::kHttpScheme));
  EXPECT_TRUE(p->IsWebSafeScheme(url::kHttpsScheme));
  EXPECT_TRUE(p->IsWebSafeScheme(url::kFtpScheme));
  EXPECT_TRUE(p->IsWebSafeScheme(url::kDataScheme));
  EXPECT_TRUE(p->IsWebSafeScheme("feed"));
  EXPECT_TRUE(p->IsWebSafeScheme(url::kBlobScheme));
  EXPECT_TRUE(p->IsWebSafeScheme(url::kFileSystemScheme));

  EXPECT_FALSE(p->IsWebSafeScheme("registered-web-safe-scheme"));
  p->RegisterWebSafeScheme("registered-web-safe-scheme");
  EXPECT_TRUE(p->IsWebSafeScheme("registered-web-safe-scheme"));

  EXPECT_FALSE(p->IsWebSafeScheme(kChromeUIScheme));
}

TEST_F(ChildProcessSecurityPolicyTest, IsPseudoSchemeTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  EXPECT_TRUE(p->IsPseudoScheme(url::kAboutScheme));
  EXPECT_TRUE(p->IsPseudoScheme(url::kJavaScriptScheme));
  EXPECT_TRUE(p->IsPseudoScheme(kViewSourceScheme));
  EXPECT_TRUE(p->IsPseudoScheme(url::kHttpSuboriginScheme));
  EXPECT_TRUE(p->IsPseudoScheme(url::kHttpsSuboriginScheme));

  EXPECT_FALSE(p->IsPseudoScheme("registered-pseudo-scheme"));
  p->RegisterPseudoScheme("registered-pseudo-scheme");
  EXPECT_TRUE(p->IsPseudoScheme("registered-pseudo-scheme"));

  EXPECT_FALSE(p->IsPseudoScheme(kChromeUIScheme));
}

TEST_F(ChildProcessSecurityPolicyTest, StandardSchemesTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->Add(kRendererID);

  // Safe to request, redirect or commit.
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("http://www.google.com/")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("https://www.paypal.com/")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("ftp://ftp.gnu.org/")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("data:text/html,<b>Hi</b>")));
  EXPECT_TRUE(p->CanRequestURL(
      kRendererID, GURL("filesystem:http://localhost/temporary/a.gif")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("http://www.google.com/")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("https://www.paypal.com/")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("ftp://ftp.gnu.org/")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("data:text/html,<b>Hi</b>")));
  EXPECT_TRUE(
      p->CanRedirectToURL(GURL("filesystem:http://localhost/temporary/a.gif")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("http://www.google.com/")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("https://www.paypal.com/")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("ftp://ftp.gnu.org/")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("data:text/html,<b>Hi</b>")));
  EXPECT_TRUE(p->CanCommitURL(
      kRendererID, GURL("filesystem:http://localhost/temporary/a.gif")));
  EXPECT_TRUE(
      p->CanSetAsOriginHeader(kRendererID, GURL("http://www.google.com/")));
  EXPECT_TRUE(
      p->CanSetAsOriginHeader(kRendererID, GURL("https://www.paypal.com/")));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, GURL("ftp://ftp.gnu.org/")));
  EXPECT_TRUE(
      p->CanSetAsOriginHeader(kRendererID, GURL("data:text/html,<b>Hi</b>")));
  EXPECT_TRUE(p->CanSetAsOriginHeader(
      kRendererID, GURL("filesystem:http://localhost/temporary/a.gif")));

  // Dangerous to request, commit, or set as origin header.
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("chrome://foo/bar")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("view-source:http://www.google.com/")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("chrome://foo/bar")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID,
                                GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID,
                                GURL("chrome://foo/bar")));
  EXPECT_FALSE(
      p->CanCommitURL(kRendererID, GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(
      p->CanSetAsOriginHeader(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("chrome://foo/bar")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(
      kRendererID, GURL("view-source:http://www.google.com/")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, BlobSchemeTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->Add(kRendererID);

  EXPECT_TRUE(
      p->CanRequestURL(kRendererID, GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("blob:null/some-guid")));
  EXPECT_TRUE(
      p->CanRequestURL(kRendererID, GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("blob:NulL/some-guid")));
  EXPECT_TRUE(
      p->CanRequestURL(kRendererID, GURL("blob:NulL/some-guid#fragment")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("blob:NulL/some-guid?query")));
  EXPECT_TRUE(
      p->CanRequestURL(kRendererID, GURL("blob:blobinternal://some-guid")));
  EXPECT_FALSE(p->CanRequestURL(
      kRendererID, GURL("blob:http://username@localhost/some-guid")));
  EXPECT_FALSE(p->CanRequestURL(
      kRendererID, GURL("blob:http://username     @localhost/some-guid")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("blob:blob:some-guid")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("blob:some-guid")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("blob:filesystem:http://localhost/path")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("filesystem:blob:http://localhost/guid")));

  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:null/some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:NulL/some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:NulL/some-guid#fragment")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:NulL/some-guid?query")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:blobinternal://some-guid")));
  EXPECT_TRUE(
      p->CanRedirectToURL(GURL("blob:http://username@localhost/some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(
      GURL("blob:http://username     @localhost/some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:blob:some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:some-guid")));
  EXPECT_TRUE(
      p->CanRedirectToURL(GURL("blob:filesystem:http://localhost/path")));
  EXPECT_FALSE(
      p->CanRedirectToURL(GURL("filesystem:blob:http://localhost/guid")));

  EXPECT_TRUE(
      p->CanCommitURL(kRendererID, GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("blob:null/some-guid")));
  EXPECT_TRUE(
      p->CanCommitURL(kRendererID, GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("blob:NulL/some-guid")));
  EXPECT_TRUE(
      p->CanCommitURL(kRendererID, GURL("blob:NulL/some-guid#fragment")));
  EXPECT_TRUE(
      p->CanCommitURL(kRendererID, GURL("blob:blobinternal://some-guid")));
  EXPECT_FALSE(p->CanCommitURL(
      kRendererID, GURL("blob:http://username@localhost/some-guid")));
  EXPECT_FALSE(p->CanCommitURL(
      kRendererID, GURL("blob:http://username     @localhost/some-guid")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("blob:blob:some-guid")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("blob:some-guid")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID,
                               GURL("blob:filesystem:http://localhost/path")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID,
                               GURL("filesystem:blob:http://localhost/guid")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, AboutTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->Add(kRendererID);

  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("about:blank")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:BlAnK")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("aBouT:BlAnK")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("aBouT:blank")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("about:blank")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:BlAnK")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("aBouT:BlAnK")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("aBouT:blank")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("about:blank")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:BlAnK")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("aBouT:BlAnK")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("aBouT:blank")));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, GURL("about:blank")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("about:BlAnK")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("aBouT:BlAnK")));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, GURL("aBouT:blank")));

  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("about:srcdoc")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:srcdoc")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("about:srcdoc")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("about:srcdoc")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:SRCDOC")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:SRCDOC")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("about:SRCDOC")));

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:crash")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:cache")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:hang")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:version")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:crash")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:cache")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:hang")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:version")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:crash")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:cache")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:hang")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:version")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("about:crash")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("about:cache")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("about:hang")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("about:version")));

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("aBoUt:version")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:CrASh")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("abOuT:cAChe")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("aBoUt:version")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:CrASh")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("abOuT:cAChe")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("aBoUt:version")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:CrASh")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("abOuT:cAChe")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("aBoUt:version")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("aBoUt:version")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("about:CrASh")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("abOuT:cAChe")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("aBoUt:version")));

  // Requests for about: pages should be denied.
  p->GrantRequestURL(kRendererID, GURL("about:crash"));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:crash")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:crash")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:crash")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("about:crash")));

  // These requests for chrome:// pages should be granted.
  GURL chrome_url("chrome://foo");
  p->GrantRequestURL(kRendererID, chrome_url);
  EXPECT_TRUE(p->CanRequestURL(kRendererID, chrome_url));
  EXPECT_TRUE(p->CanRedirectToURL(GURL(chrome_url)));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, chrome_url));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, chrome_url));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, JavaScriptTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->Add(kRendererID);

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("javascript:alert('xss')")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("javascript:alert('xss')")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("javascript:alert('xss')")));
  EXPECT_FALSE(
      p->CanSetAsOriginHeader(kRendererID, GURL("javascript:alert('xss')")));
  p->GrantRequestURL(kRendererID, GURL("javascript:alert('xss')"));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("javascript:alert('xss')")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("javascript:alert('xss')")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("javascript:alert('xss')")));
  EXPECT_FALSE(
      p->CanSetAsOriginHeader(kRendererID, GURL("javascript:alert('xss')")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, SuboriginTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->Add(kRendererID);

  // Suborigin URLs are not requestable or committable.
  EXPECT_FALSE(
      p->CanRequestURL(kRendererID, GURL("http-so://foobar.example.com")));
  EXPECT_FALSE(
      p->CanRequestURL(kRendererID, GURL("https-so://foobar.example.com")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("http-so://foobar.example.com")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("https-so://foobar.example.com")));
  EXPECT_FALSE(
      p->CanCommitURL(kRendererID, GURL("http-so://foobar.example.com")));
  EXPECT_FALSE(
      p->CanCommitURL(kRendererID, GURL("https-so://foobar.example.com")));

  // It's not possible to grant suborigins requestable status.
  p->GrantRequestURL(kRendererID, GURL("https-so://foobar.example.com"));
  EXPECT_FALSE(
      p->CanCommitURL(kRendererID, GURL("https-so://foobar.example.com")));

  // Suborigin URLs are valid origin headers.
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID,
                                      GURL("http-so://foobar.example.com")));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID,
                                      GURL("https-so://foobar.example.com")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, RegisterWebSafeSchemeTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->Add(kRendererID);

  // Currently, "asdf" is destined for ShellExecute, so it is allowed to be
  // requested but not committed.
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("asdf:rockers")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("asdf:rockers")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("asdf:rockers")));

  // Once we register "asdf", we default to deny.
  RegisterTestScheme("asdf");
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("asdf:rockers")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("asdf:rockers")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, GURL("asdf:rockers")));

  // We can allow new schemes by adding them to the whitelist.
  p->RegisterWebSafeScheme("asdf");
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("asdf:rockers")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("asdf:rockers")));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, GURL("asdf:rockers")));

  // Cleanup.
  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, CanServiceCommandsTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->Add(kRendererID);

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_FALSE(
      p->CanSetAsOriginHeader(kRendererID, GURL("file:///etc/passwd")));
  p->GrantRequestURL(kRendererID, GURL("file:///etc/passwd"));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, GURL("file:///etc/passwd")));

  // We should forget our state if we repeat a renderer id.
  p->Remove(kRendererID);
  p->Add(kRendererID);
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_FALSE(
      p->CanSetAsOriginHeader(kRendererID, GURL("file:///etc/passwd")));
  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, ViewSource) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->Add(kRendererID);

  // Child processes cannot request view source URLs.
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(
      kRendererID, GURL("view-source:view-source:http://www.google.com/")));

  // Child processes cannot be redirected to view source URLs.
  EXPECT_FALSE(p->CanRedirectToURL(GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("view-source:file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanRedirectToURL(
      GURL("view-source:view-source:http://www.google.com/")));

  // View source URLs don't actually commit; the renderer is put into view
  // source mode, and the inner URL commits.
  EXPECT_FALSE(p->CanCommitURL(kRendererID,
                               GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID,
                               GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(
      kRendererID, GURL("view-source:view-source:http://www.google.com/")));

  // View source URLs should not be setable as origin headers
  EXPECT_FALSE(p->CanSetAsOriginHeader(
      kRendererID, GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID,
                                       GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(
      p->CanSetAsOriginHeader(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(
      kRendererID, GURL("view-source:view-source:http://www.google.com/")));

  p->GrantRequestURL(kRendererID, GURL("view-source:file:///etc/passwd"));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_FALSE(
      p->CanSetAsOriginHeader(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_FALSE(
      p->CanRequestURL(kRendererID, GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID,
                               GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID,
                                       GURL("view-source:file:///etc/passwd")));
  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, SpecificFile) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->Add(kRendererID);

  GURL icon_url("file:///tmp/foo.png");
  GURL sensitive_url("file:///etc/passwd");
  EXPECT_FALSE(p->CanRequestURL(kRendererID, icon_url));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, sensitive_url));
  EXPECT_TRUE(p->CanRedirectToURL(icon_url));
  EXPECT_TRUE(p->CanRedirectToURL(sensitive_url));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, icon_url));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, sensitive_url));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, icon_url));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, sensitive_url));

  p->GrantRequestSpecificFileURL(kRendererID, icon_url);
  EXPECT_TRUE(p->CanRequestURL(kRendererID, icon_url));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, sensitive_url));
  EXPECT_TRUE(p->CanRedirectToURL(icon_url));
  EXPECT_TRUE(p->CanRedirectToURL(sensitive_url));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, icon_url));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, sensitive_url));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, icon_url));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, sensitive_url));

  p->GrantRequestURL(kRendererID, icon_url);
  EXPECT_TRUE(p->CanRequestURL(kRendererID, icon_url));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, sensitive_url));
  EXPECT_TRUE(p->CanRedirectToURL(icon_url));
  EXPECT_TRUE(p->CanRedirectToURL(sensitive_url));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, icon_url));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, sensitive_url));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, icon_url));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, sensitive_url));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, FileSystemGrantsTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->Add(kRendererID);
  std::string read_id =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypeTest, "read_filesystem", base::FilePath());
  std::string read_write_id =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypeTest,
          "read_write_filesystem",
          base::FilePath());
  std::string copy_into_id =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypeTest,
          "copy_into_filesystem",
          base::FilePath());
  std::string delete_from_id =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypeTest,
          "delete_from_filesystem",
          base::FilePath());

  // Test initially having no permissions.
  CheckHasNoFileSystemPermission(p, read_id);
  CheckHasNoFileSystemPermission(p, read_write_id);
  CheckHasNoFileSystemPermission(p, copy_into_id);
  CheckHasNoFileSystemPermission(p, delete_from_id);

  // Testing varying combinations of grants and checks.
  p->GrantReadFileSystem(kRendererID, read_id);
  EXPECT_TRUE(p->CanReadFileSystem(kRendererID, read_id));
  EXPECT_FALSE(p->CanReadWriteFileSystem(kRendererID, read_id));
  EXPECT_FALSE(p->CanCopyIntoFileSystem(kRendererID, read_id));
  EXPECT_FALSE(p->CanDeleteFromFileSystem(kRendererID, read_id));

  p->GrantReadFileSystem(kRendererID, read_write_id);
  p->GrantWriteFileSystem(kRendererID, read_write_id);
  EXPECT_TRUE(p->CanReadFileSystem(kRendererID, read_write_id));
  EXPECT_TRUE(p->CanReadWriteFileSystem(kRendererID, read_write_id));
  EXPECT_FALSE(p->CanCopyIntoFileSystem(kRendererID, read_write_id));
  EXPECT_FALSE(p->CanDeleteFromFileSystem(kRendererID, read_write_id));

  p->GrantCopyIntoFileSystem(kRendererID, copy_into_id);
  EXPECT_FALSE(p->CanReadFileSystem(kRendererID, copy_into_id));
  EXPECT_FALSE(p->CanReadWriteFileSystem(kRendererID, copy_into_id));
  EXPECT_TRUE(p->CanCopyIntoFileSystem(kRendererID, copy_into_id));
  EXPECT_FALSE(p->CanDeleteFromFileSystem(kRendererID, copy_into_id));

  p->GrantDeleteFromFileSystem(kRendererID, delete_from_id);
  EXPECT_FALSE(p->CanReadFileSystem(kRendererID, delete_from_id));
  EXPECT_FALSE(p->CanReadWriteFileSystem(kRendererID, delete_from_id));
  EXPECT_FALSE(p->CanCopyIntoFileSystem(kRendererID, delete_from_id));
  EXPECT_TRUE(p->CanDeleteFromFileSystem(kRendererID, delete_from_id));

  // Test revoke permissions on renderer ID removal.
  p->Remove(kRendererID);
  CheckHasNoFileSystemPermission(p, read_id);
  CheckHasNoFileSystemPermission(p, read_write_id);
  CheckHasNoFileSystemPermission(p, copy_into_id);
  CheckHasNoFileSystemPermission(p, delete_from_id);

  // Test having no permissions upon re-adding same renderer ID.
  p->Add(kRendererID);
  CheckHasNoFileSystemPermission(p, read_id);
  CheckHasNoFileSystemPermission(p, read_write_id);
  CheckHasNoFileSystemPermission(p, copy_into_id);
  CheckHasNoFileSystemPermission(p, delete_from_id);

  // Cleanup.
  p->Remove(kRendererID);
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(read_id);
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(read_write_id);
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(copy_into_id);
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(delete_from_id);
}

TEST_F(ChildProcessSecurityPolicyTest, FilePermissionGrantingAndRevoking) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->RegisterFileSystemPermissionPolicy(
      storage::kFileSystemTypeTest,
      storage::FILE_PERMISSION_USE_FILE_PERMISSION);

  p->Add(kRendererID);
  base::FilePath file(TEST_PATH("/dir/testfile"));
  file = file.NormalizePathSeparators();
  storage::FileSystemURL url = storage::FileSystemURL::CreateForTest(
      GURL("http://foo/"), storage::kFileSystemTypeTest, file);

  // Test initially having no permissions.
  CheckHasNoFileSystemFilePermission(p, file, url);

  // Testing every combination of permissions granting and revoking.
  p->GrantReadFile(kRendererID, file);
  EXPECT_TRUE(p->CanReadFile(kRendererID, file));
  EXPECT_FALSE(p->CanCreateReadWriteFile(kRendererID, file));
  EXPECT_TRUE(p->CanReadFileSystemFile(kRendererID, url));
  EXPECT_FALSE(p->CanWriteFileSystemFile(kRendererID, url));
  EXPECT_FALSE(p->CanCreateFileSystemFile(kRendererID, url));
  EXPECT_FALSE(p->CanCreateReadWriteFileSystemFile(kRendererID, url));
  EXPECT_FALSE(p->CanCopyIntoFileSystemFile(kRendererID, url));
  EXPECT_FALSE(p->CanDeleteFileSystemFile(kRendererID, url));
  p->RevokeAllPermissionsForFile(kRendererID, file);
  CheckHasNoFileSystemFilePermission(p, file, url);

  p->GrantCreateReadWriteFile(kRendererID, file);
  EXPECT_TRUE(p->CanReadFile(kRendererID, file));
  EXPECT_TRUE(p->CanCreateReadWriteFile(kRendererID, file));
  EXPECT_TRUE(p->CanReadFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanWriteFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanCreateFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanCreateReadWriteFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanCopyIntoFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanDeleteFileSystemFile(kRendererID, url));
  p->RevokeAllPermissionsForFile(kRendererID, file);
  CheckHasNoFileSystemFilePermission(p, file, url);

  // Test revoke permissions on renderer ID removal.
  p->GrantCreateReadWriteFile(kRendererID, file);
  EXPECT_TRUE(p->CanReadFile(kRendererID, file));
  EXPECT_TRUE(p->CanCreateReadWriteFile(kRendererID, file));
  EXPECT_TRUE(p->CanReadFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanWriteFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanCreateFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanCreateReadWriteFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanCopyIntoFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanDeleteFileSystemFile(kRendererID, url));
  p->Remove(kRendererID);
  CheckHasNoFileSystemFilePermission(p, file, url);

  // Test having no permissions upon re-adding same renderer ID.
  p->Add(kRendererID);
  CheckHasNoFileSystemFilePermission(p, file, url);

  // Cleanup.
  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, FilePermissions) {
  base::FilePath granted_file = base::FilePath(TEST_PATH("/home/joe"));
  base::FilePath sibling_file = base::FilePath(TEST_PATH("/home/bob"));
  base::FilePath child_file = base::FilePath(TEST_PATH("/home/joe/file"));
  base::FilePath parent_file = base::FilePath(TEST_PATH("/home"));
  base::FilePath parent_slash_file = base::FilePath(TEST_PATH("/home/"));
  base::FilePath child_traversal1 =
      base::FilePath(TEST_PATH("/home/joe/././file"));
  base::FilePath child_traversal2 = base::FilePath(
      TEST_PATH("/home/joe/file/../otherfile"));
  base::FilePath evil_traversal1 =
      base::FilePath(TEST_PATH("/home/joe/../../etc/passwd"));
  base::FilePath evil_traversal2 = base::FilePath(
      TEST_PATH("/home/joe/./.././../etc/passwd"));
  base::FilePath self_traversal =
      base::FilePath(TEST_PATH("/home/joe/../joe/file"));
  base::FilePath relative_file = base::FilePath(FILE_PATH_LITERAL("home/joe"));

  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Grant permissions for a file.
  p->Add(kRendererID);
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_OPEN));

  GrantPermissionsForFile(p, kRendererID, granted_file,
                             base::File::FLAG_OPEN |
                             base::File::FLAG_OPEN_TRUNCATED |
                             base::File::FLAG_READ |
                             base::File::FLAG_WRITE);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, granted_file,
                                       base::File::FLAG_OPEN |
                                       base::File::FLAG_OPEN_TRUNCATED |
                                       base::File::FLAG_READ |
                                       base::File::FLAG_WRITE));
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, granted_file,
                                       base::File::FLAG_OPEN |
                                       base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_CREATE));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file, 0));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_CREATE |
                                        base::File::FLAG_OPEN_TRUNCATED |
                                        base::File::FLAG_READ |
                                        base::File::FLAG_WRITE));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, sibling_file,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, parent_file,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, child_file,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, child_traversal1,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, child_traversal2,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, evil_traversal1,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, evil_traversal2,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  // CPSP doesn't allow this case for the sake of simplicity.
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, self_traversal,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  p->Remove(kRendererID);

  // Grant permissions for the directory the file is in.
  p->Add(kRendererID);
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_OPEN));
  GrantPermissionsForFile(p, kRendererID, parent_file,
                             base::File::FLAG_OPEN |
                             base::File::FLAG_READ);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_OPEN));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_READ |
                                        base::File::FLAG_WRITE));
  p->Remove(kRendererID);

  // Grant permissions for the directory the file is in (with trailing '/').
  p->Add(kRendererID);
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_OPEN));
  GrantPermissionsForFile(p, kRendererID, parent_slash_file,
                             base::File::FLAG_OPEN |
                             base::File::FLAG_READ);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_OPEN));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_READ |
                                        base::File::FLAG_WRITE));

  // Grant permissions for the file (should overwrite the permissions granted
  // for the directory).
  GrantPermissionsForFile(p, kRendererID, granted_file,
                             base::File::FLAG_TEMPORARY);
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_OPEN));
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, granted_file,
                                       base::File::FLAG_TEMPORARY));

  // Revoke all permissions for the file (it should inherit its permissions
  // from the directory again).
  p->RevokeAllPermissionsForFile(kRendererID, granted_file);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, granted_file,
                                       base::File::FLAG_OPEN |
                                       base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_TEMPORARY));
  p->Remove(kRendererID);

  // Grant file permissions for the file to main thread renderer process,
  // make sure its worker thread renderer process inherits those.
  p->Add(kRendererID);
  GrantPermissionsForFile(p, kRendererID, granted_file,
                             base::File::FLAG_OPEN |
                             base::File::FLAG_READ);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, granted_file,
                                       base::File::FLAG_OPEN |
                                       base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                       base::File::FLAG_WRITE));
  p->AddWorker(kWorkerRendererID, kRendererID);
  EXPECT_TRUE(p->HasPermissionsForFile(kWorkerRendererID, granted_file,
                                       base::File::FLAG_OPEN |
                                       base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kWorkerRendererID, granted_file,
                                        base::File::FLAG_WRITE));
  p->Remove(kRendererID);
  EXPECT_FALSE(p->HasPermissionsForFile(kWorkerRendererID, granted_file,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  p->Remove(kWorkerRendererID);

  p->Add(kRendererID);
  GrantPermissionsForFile(p, kRendererID, relative_file,
                             base::File::FLAG_OPEN);
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, relative_file,
                                        base::File::FLAG_OPEN));
  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, CanServiceWebUIBindings) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL url("chrome://thumb/http://www.google.com/");

  p->Add(kRendererID);

  EXPECT_FALSE(p->HasWebUIBindings(kRendererID));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
  EXPECT_TRUE(p->CanRedirectToURL(url));
  p->GrantWebUIBindings(kRendererID);
  EXPECT_TRUE(p->HasWebUIBindings(kRendererID));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, url));
  EXPECT_TRUE(p->CanRedirectToURL(url));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, RemoveRace) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL url("file:///etc/passwd");
  base::FilePath file(TEST_PATH("/etc/passwd"));

  p->Add(kRendererID);

  p->GrantRequestURL(kRendererID, url);
  p->GrantReadFile(kRendererID, file);
  p->GrantWebUIBindings(kRendererID);

  EXPECT_TRUE(p->CanRequestURL(kRendererID, url));
  EXPECT_TRUE(p->CanRedirectToURL(url));
  EXPECT_TRUE(p->CanReadFile(kRendererID, file));
  EXPECT_TRUE(p->HasWebUIBindings(kRendererID));

  p->Remove(kRendererID);

  // Renderers are added and removed on the UI thread, but the policy can be
  // queried on the IO thread.  The ChildProcessSecurityPolicy needs to be
  // prepared to answer policy questions about renderers who no longer exist.

  // In this case, we default to secure behavior.
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
  EXPECT_TRUE(p->CanRedirectToURL(url));
  EXPECT_FALSE(p->CanReadFile(kRendererID, file));
  EXPECT_FALSE(p->HasWebUIBindings(kRendererID));
}

// Test the granting of origin permissions, and their interactions with
// granting scheme permissions.
TEST_F(ChildProcessSecurityPolicyTest, OriginGranting) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->Add(kRendererID);

  GURL url_foo1("chrome://foo/resource1");
  GURL url_foo2("chrome://foo/resource2");
  GURL url_bar("chrome://bar/resource3");

  EXPECT_FALSE(p->CanRequestURL(kRendererID, url_foo1));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url_bar));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo1));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo2));
  EXPECT_TRUE(p->CanRedirectToURL(url_bar));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url_foo1));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url_bar));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, url_foo1));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, url_bar));

  p->GrantOrigin(kRendererID, url::Origin(url_foo1));

  EXPECT_TRUE(p->CanRequestURL(kRendererID, url_foo1));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url_bar));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo1));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo2));
  EXPECT_TRUE(p->CanRedirectToURL(url_bar));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, url_foo1));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url_bar));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, url_foo1));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanSetAsOriginHeader(kRendererID, url_bar));

  p->GrantScheme(kRendererID, kChromeUIScheme);

  EXPECT_TRUE(p->CanRequestURL(kRendererID, url_foo1));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, url_foo2));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, url_bar));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo1));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo2));
  EXPECT_TRUE(p->CanRedirectToURL(url_bar));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, url_foo1));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, url_foo2));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, url_bar));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, url_foo1));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, url_foo2));
  EXPECT_TRUE(p->CanSetAsOriginHeader(kRendererID, url_bar));

  p->Remove(kRendererID);
}

// Verifies parsing logic that extracts origins from --isolate-origins.
TEST_F(ChildProcessSecurityPolicyTest, IsolateOriginsFromCommandLine) {
  // Invalid and unique origins are not permitted.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOriginsFromCommandLine("foo");
  policy->AddIsolatedOriginsFromCommandLine("");
  policy->AddIsolatedOriginsFromCommandLine("about:blank");
  EXPECT_EQ(0U, policy->isolated_origins_.size());

  policy->AddIsolatedOriginsFromCommandLine("http://isolated.foo.com");
  EXPECT_EQ(1U, policy->isolated_origins_.size());
  EXPECT_TRUE(
      policy->IsIsolatedOrigin(url::Origin(GURL("http://isolated.foo.com"))));

  policy->AddIsolatedOriginsFromCommandLine(
      "http://a.com,https://b.com,,https://c.com:8000");
  EXPECT_EQ(4U, policy->isolated_origins_.size());
  EXPECT_TRUE(
      policy->IsIsolatedOrigin(url::Origin(GURL("http://isolated.foo.com"))));
  EXPECT_TRUE(policy->IsIsolatedOrigin(url::Origin(GURL("http://a.com"))));
  EXPECT_TRUE(policy->IsIsolatedOrigin(url::Origin(GURL("https://b.com"))));
  EXPECT_TRUE(
      policy->IsIsolatedOrigin(url::Origin(GURL("https://c.com:8000"))));
}

}  // namespace content
