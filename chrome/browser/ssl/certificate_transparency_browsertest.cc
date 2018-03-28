// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/sth_distributor_provider.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/certificate_transparency/features.h"
#include "components/certificate_transparency/single_tree_tracker.h"
#include "components/certificate_transparency/tree_state_tracker.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/signed_tree_head.h"
#include "net/cert/sth_distributor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

using net::ct::DigitallySigned;
using net::ct::SignedTreeHead;
using net::test::IsOk;

constexpr base::TimeDelta kZeroTTL;

// Decodes a base64-encoded "DigitallySigned" TLS struct into |*sig_out|.
// See https://tools.ietf.org/html/rfc5246#section-4.7.
// |sig_out| must not be null.
bool DecodeDigitallySigned(base::StringPiece base64_data,
                           DigitallySigned* sig_out) {
  std::string data;
  if (!base::Base64Decode(base64_data, &data))
    return false;

  base::StringPiece data_ptr = data;
  if (!net::ct::DecodeDigitallySigned(&data_ptr, sig_out))
    return false;

  return true;
}

// Populates |*sth_out| with the given information.
// |sth_out| must not be null.
bool BuildSignedTreeHead(base::Time timestamp,
                         uint64_t tree_size,
                         base::StringPiece root_hash_base64,
                         base::StringPiece signature_base64,
                         base::StringPiece log_id_base64,
                         net::ct::SignedTreeHead* sth_out) {
  sth_out->version = SignedTreeHead::V1;
  sth_out->timestamp = timestamp;
  sth_out->tree_size = tree_size;

  std::string root_hash;
  if (!base::Base64Decode(root_hash_base64, &root_hash)) {
    return false;
  }
  root_hash.copy(sth_out->sha256_root_hash, net::ct::kSthRootHashLength);

  return DecodeDigitallySigned(signature_base64, &sth_out->signature) &&
         base::Base64Decode(log_id_base64, &sth_out->log_id);
}

// Runs |closure| on the IO thread and waits until it completes.
void RunOnIOThreadBlocking(base::OnceClosure closure) {
  base::RunLoop run_loop;

  content::BrowserThread::PostTaskAndReply(content::BrowserThread::IO,
                                           FROM_HERE, std::move(closure),
                                           run_loop.QuitClosure());

  run_loop.Run();
}

// Adds an entry to the HostCache used by a particular URLRequestContext.
// The entry will map |hostname| to |ip_literal|, as though a DNS lookup had
// been performed and an an A/AAAA record had been found for this mapping.
void AddCacheEntryOnIOThread(net::URLRequestContextGetter* context_getter,
                             const std::string& hostname,
                             const std::string& ip_literal) {
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  net::HostCache* cache =
      context_getter->GetURLRequestContext()->host_resolver()->GetHostCache();
  ASSERT_TRUE(cache);

  net::AddressList address_list;
  ASSERT_THAT(net::ParseAddressList(ip_literal, hostname, &address_list),
              IsOk());

  cache->Set(net::HostCache::Key(hostname, net::ADDRESS_FAMILY_UNSPECIFIED, 0),
             net::HostCache::Entry(net::OK, address_list,
                                   net::HostCache::Entry::SOURCE_DNS),
             base::TimeTicks::Now(), kZeroTTL);
}

// A test fixture that can enable Certificate Transparency checks, then initiate
// a connection to a test HTTPS server and intercept certificate verification so
// that those checks can be performed successfully. This involves substituting a
// test certificate for a real certificate containing Signed Certificate
// Timestamps. Certificate verification is hard-coded to succeed, so the real
// certificate will never be treated as expired.
class CertificateTransparencyBrowserTest : public CertVerifierBrowserTest {
 public:
  CertificateTransparencyBrowserTest()
      : CertVerifierBrowserTest(),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatures({certificate_transparency::kCTLogAuditing},
                                   {});
  }

 protected:
  void TestCTHistogramsArePopulated(bool use_profile) {
    net::URLRequestContextGetter* request_context = nullptr;
    if (use_profile) {
      request_context = browser()->profile()->GetRequestContext();
    } else {
      request_context = g_browser_process->system_request_context();
    }

    // Provide an STH from Google's Pilot log that can be used to prove
    // inclusion for an SCT later in the test.
    SignedTreeHead pilot_sth;
    ASSERT_TRUE(BuildSignedTreeHead(
        base::Time::FromJsTime(1512419914170), 181871752,
        "bvgljSy3Yg32Y6J8qL5WmUA3jn2WnOrEFDqxD0AxUvs=",
        "BAMARjBEAiAwEXve2RBk3XkUR+6nACSETTgzKFaEeginxuj5U9BI/"
        "wIgBPuQS5ACxsro6TtpY4bQyE6WlMdcSMiMd/SSGraOBOg=",
        "pLkJkLQYWBSHuxOizGdwCjw1mAT5G9+443fNDsgN3BA=", &pilot_sth));
    chrome_browser_net::GetGlobalSTHDistributor()->NewSTHObserved(pilot_sth);

    // Provide an STH from Google's Aviator log that is not recent enough to
    // prove inclusion for an SCT later in the test.
    SignedTreeHead aviator_sth;
    ASSERT_TRUE(BuildSignedTreeHead(
        base::Time::FromJsTime(1442652106945), 8502329,
        "bfG+gWZcHl9fqtNo0Z/uggs8E5YqGOtJQ0Z5zVZDRxI=",
        "BAMARjBEAiA6elcNQoShmKLHj/"
        "IA649UIbaQtWJEpj0Eot0q7G6fEgIgYChb7U6Reuvt0nO5PionH+3UciOxKV3Cy8/"
        "eq59lSYY=",
        "aPaY+B9kgr46jO65KB1M/HFRXWeT1ETRCmesu09P+8Q=", &aviator_sth));
    chrome_browser_net::GetGlobalSTHDistributor()->NewSTHObserved(aviator_sth);

    // TODO(robpercival): Override CT log list to ensure it includes Google's
    // Pilot and Aviator CT logs, as well as DigiCert's CT log.

    // Connect to https_server_ using the name "localhost", rather than by IP
    // address. CT inclusion checks are skipped if a host was not looked up
    // using DNS, so a hostname must be used.
    https_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
    ASSERT_TRUE(https_server_.Start());

    // Add "localhost" to the HostCache, so that it appears that a DNS lookup
    // was performed for this hostname. This will make the SCTs provided by the
    // server eligible for inclusion checks.
    RunOnIOThreadBlocking(base::BindOnce(&AddCacheEntryOnIOThread,
                                         base::RetainedRef(request_context),
                                         "localhost", "127.0.0.1"));

    // Swap the normal test cert for one that contains 3 SCTs and fulfills
    // Chrome's CT policy. To do so, setup the mock_cert_verifier to swap the
    // HTTP server's certificate for a different one during the verification
    // process, and return a CertVerifyResult that indicates that this
    // replacement cert is valid and issued by a known root (CT checks are
    // skipped for private roots).
    net::CertVerifyResult verify_result;
    verify_result.is_issued_by_known_root = true;
    verify_result.cert_status = 0;

    {
      base::ScopedAllowBlockingForTesting allow_blocking_for_loading_cert;

      verify_result.verified_cert = net::CreateCertificateChainFromFile(
          net::GetTestCertsDirectory(), "comodo-chain.pem",
          net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
      ASSERT_TRUE(verify_result.verified_cert);
    }

    mock_cert_verifier()->AddResultForCert(https_server_.GetCertificate(),
                                           verify_result, net::OK);

    const int kNumSCTs = 3;  // Number of SCTs in verified_cert
    base::HistogramTester histograms;

    // Navigate to a test server URL, which should trigger a CT inclusion check
    // thanks to the cert mock_cert_verifier swaps in.
    GURL url = https_server_.GetURL("/");
    if (use_profile) {
      ui_test_utils::NavigateToURL(browser(), url);
    } else {
      content::LoadBasicRequest(
          g_browser_process->system_network_context_manager()->GetContext(),
          url);
    }

    // Find out how many connections Chrome made (it may make more than one).
    const base::HistogramBase::Count connection_count =
        histograms.GetBucketCount("Net.SSL_Connection_Error", net::OK);
    ASSERT_TRUE(connection_count > 0);

    // Expect 3 SCTs in each connection.
    EXPECT_THAT(histograms.GetBucketCount(
                    "Net.CertificateTransparency.SCTsPerConnection", kNumSCTs),
                connection_count);

    // Expect that the SCTs were embedded in the certificate.
    EXPECT_THAT(histograms.GetBucketCount(
                    "Net.CertificateTransparency.SCTOrigin",
                    static_cast<int>(
                        net::ct::SignedCertificateTimestamp::SCT_EMBEDDED)),
                kNumSCTs * connection_count);

    // The certificate contains a sufficient number and diversity of SCTs.
    // 2 from Google CT logs (Pilot, Aviator) and 1 from a non-Google CT log
    // (DigiCert).
    histograms.ExpectUniqueSample(
        "Net.CertificateTransparency.ConnectionComplianceStatus2.SSL",
        static_cast<int>(
            net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS),
        connection_count);

    // The CanInclusionCheckSCT histogram should only have a single sample in
    // each bucket, because SCTs should be de-duplicated prior to inclusion
    // checks.

    // The Pilot SCT should be eligible for inclusion checking, because a recent
    // enough Pilot STH is available.
    histograms.ExpectBucketCount(
        "Net.CertificateTransparency.CanInclusionCheckSCT",
        certificate_transparency::CAN_BE_CHECKED, 1);
    // The Aviator SCT should not be eligible for inclusion checking, because
    // there is not a recent enough Aviator STH available.
    histograms.ExpectBucketCount(
        "Net.CertificateTransparency.CanInclusionCheckSCT",
        certificate_transparency::NEWER_STH_REQUIRED, 1);
    // The DigiCert SCT should not be eligible for inclusion checking, because
    // there is no DigiCert STH available.
    histograms.ExpectBucketCount(
        "Net.CertificateTransparency.CanInclusionCheckSCT",
        certificate_transparency::VALID_STH_REQUIRED, 1);
  }

  net::EmbeddedTestServer https_server_;

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CertificateTransparencyBrowserTest);
};

// Tests that wiring is correctly setup in ProfileIOData to pass Signed
// Certificate Timestamps (SCTs) through from the SSL socket code to the CT
// verification and inclusion checking code. This is assessed by checking that
// histograms are correctly populated.
IN_PROC_BROWSER_TEST_F(CertificateTransparencyBrowserTest, ProfileRequest) {
  ASSERT_NO_FATAL_FAILURE(TestCTHistogramsArePopulated(true));
}

// Tests that wiring is correctly setup in IOThread to pass Signed Certificate
// Timestamps (SCTs) through from the SSL socket code to the CT verification and
// inclusion checking code. This is assessed by checking that histograms are
// correctly populated.
IN_PROC_BROWSER_TEST_F(CertificateTransparencyBrowserTest, SystemRequest) {
  ASSERT_NO_FATAL_FAILURE(TestCTHistogramsArePopulated(false));
}

}  // namespace
