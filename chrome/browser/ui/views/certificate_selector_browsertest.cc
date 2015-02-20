// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/certificate_selector.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestCertificateSelector : public chrome::CertificateSelector {
 public:
  TestCertificateSelector(const net::CertificateList& certificates,
                          content::WebContents* web_contents)
      : CertificateSelector(certificates, web_contents) {}

  void Init() { InitWithText(base::ASCIIToUTF16("some arbitrary text")); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCertificateSelector);
};

class CertificateSelectorTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    client_1_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem");
    ASSERT_NE(nullptr, client_1_);

    client_2_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_2.pem");
    ASSERT_NE(nullptr, client_2_);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(content::WaitForLoadStop(
        browser()->tab_strip_model()->GetActiveWebContents()));

    net::CertificateList certificates;
    certificates.push_back(client_1_);
    certificates.push_back(client_2_);

    selector_ = new TestCertificateSelector(
        certificates, browser()->tab_strip_model()->GetActiveWebContents());
    selector_->Init();
    selector_->Show();
  }

  void TearDownOnMainThread() override {
    selector_->Close();
    selector_ = nullptr;
  }

 protected:
  scoped_refptr<net::X509Certificate> client_1_;
  scoped_refptr<net::X509Certificate> client_2_;

  // The selector will be deleted when the browser is shutdown.
  TestCertificateSelector* selector_ = nullptr;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(CertificateSelectorTest, GetSelectedCert) {
  EXPECT_EQ(client_1_.get(), selector_->GetSelectedCert());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              false, false, false));
  EXPECT_EQ(client_2_.get(), selector_->GetSelectedCert());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_UP, false,
                                              false, false, false));
  EXPECT_EQ(client_1_.get(), selector_->GetSelectedCert());
}
