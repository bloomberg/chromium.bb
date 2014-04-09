// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_state.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/sha1.h"
#include "base/strings/string_piece.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_OPENSSL)
#include "crypto/openssl_util.h"
#else
#include "crypto/nss_util.h"
#endif

namespace net {

class TransportSecurityStateTest : public testing::Test {
  virtual void SetUp() {
#if defined(USE_OPENSSL)
    crypto::EnsureOpenSSLInit();
#else
    crypto::EnsureNSSInit();
#endif
  }

 protected:
  std::string CanonicalizeHost(const std::string& host) {
    return TransportSecurityState::CanonicalizeHost(host);
  }

  bool GetStaticDomainState(TransportSecurityState* state,
                            const std::string& host,
                            bool sni_enabled,
                            TransportSecurityState::DomainState* result) {
    return state->GetStaticDomainState(host, sni_enabled, result);
  }

  void EnableHost(TransportSecurityState* state,
                  const std::string& host,
                  const TransportSecurityState::DomainState& domain_state) {
    return state->EnableHost(host, domain_state);
  }
};

TEST_F(TransportSecurityStateTest, SimpleMatches) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.GetDomainState("yahoo.com", true, &domain_state));
  bool include_subdomains = false;
  state.AddHSTS("yahoo.com", expiry, include_subdomains);
  EXPECT_TRUE(state.GetDomainState("yahoo.com", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, MatchesCase1) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.GetDomainState("yahoo.com", true, &domain_state));
  bool include_subdomains = false;
  state.AddHSTS("YAhoo.coM", expiry, include_subdomains);
  EXPECT_TRUE(state.GetDomainState("yahoo.com", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, MatchesCase2) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.GetDomainState("YAhoo.coM", true, &domain_state));
  bool include_subdomains = false;
  state.AddHSTS("yahoo.com", expiry, include_subdomains);
  EXPECT_TRUE(state.GetDomainState("YAhoo.coM", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, SubdomainMatches) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.GetDomainState("yahoo.com", true, &domain_state));
  bool include_subdomains = true;
  state.AddHSTS("yahoo.com", expiry, include_subdomains);
  EXPECT_TRUE(state.GetDomainState("yahoo.com", true, &domain_state));
  EXPECT_TRUE(state.GetDomainState("foo.yahoo.com", true, &domain_state));
  EXPECT_TRUE(state.GetDomainState("foo.bar.yahoo.com", true, &domain_state));
  EXPECT_TRUE(state.GetDomainState("foo.bar.baz.yahoo.com", true,
                                   &domain_state));
  EXPECT_FALSE(state.GetDomainState("com", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, InvalidDomains) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.GetDomainState("yahoo.com", true, &domain_state));
  bool include_subdomains = true;
  state.AddHSTS("yahoo.com", expiry, include_subdomains);
  EXPECT_TRUE(state.GetDomainState("www-.foo.yahoo.com", true, &domain_state));
  EXPECT_TRUE(state.GetDomainState("2\x01.foo.yahoo.com", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, DeleteAllDynamicDataSince) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  const base::Time older = current_time - base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.GetDomainState("yahoo.com", true, &domain_state));
  bool include_subdomains = false;
  state.AddHSTS("yahoo.com", expiry, include_subdomains);

  state.DeleteAllDynamicDataSince(expiry);
  EXPECT_TRUE(state.GetDomainState("yahoo.com", true, &domain_state));
  state.DeleteAllDynamicDataSince(older);
  EXPECT_FALSE(state.GetDomainState("yahoo.com", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, DeleteDynamicDataForHost) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  bool include_subdomains = false;
  state.AddHSTS("yahoo.com", expiry, include_subdomains);

  EXPECT_TRUE(state.GetDomainState("yahoo.com", true, &domain_state));
  EXPECT_FALSE(state.GetDomainState("example.com", true, &domain_state));
  EXPECT_TRUE(state.DeleteDynamicDataForHost("yahoo.com"));
  EXPECT_FALSE(state.GetDomainState("yahoo.com", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, IsPreloaded) {
  const std::string paypal = CanonicalizeHost("paypal.com");
  const std::string www_paypal = CanonicalizeHost("www.paypal.com");
  const std::string foo_paypal = CanonicalizeHost("foo.paypal.com");
  const std::string a_www_paypal = CanonicalizeHost("a.www.paypal.com");
  const std::string abc_paypal = CanonicalizeHost("a.b.c.paypal.com");
  const std::string example = CanonicalizeHost("example.com");
  const std::string aypal = CanonicalizeHost("aypal.com");

  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;

  EXPECT_TRUE(GetStaticDomainState(&state, paypal, true, &domain_state));
  EXPECT_TRUE(GetStaticDomainState(&state, www_paypal, true, &domain_state));
  EXPECT_FALSE(domain_state.sts_include_subdomains);
  EXPECT_FALSE(domain_state.pkp_include_subdomains);
  EXPECT_FALSE(GetStaticDomainState(&state, a_www_paypal, true, &domain_state));
  EXPECT_FALSE(GetStaticDomainState(&state, abc_paypal, true, &domain_state));
  EXPECT_FALSE(GetStaticDomainState(&state, example, true, &domain_state));
  EXPECT_FALSE(GetStaticDomainState(&state, aypal, true, &domain_state));
}

TEST_F(TransportSecurityStateTest, PreloadedDomainSet) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;

  // The domain wasn't being set, leading to a blank string in the
  // chrome://net-internals/#hsts UI. So test that.
  EXPECT_TRUE(state.GetDomainState("market.android.com", true, &domain_state));
  EXPECT_EQ(domain_state.domain, "market.android.com");
  EXPECT_TRUE(state.GetDomainState("sub.market.android.com", true,
                                   &domain_state));
  EXPECT_EQ(domain_state.domain, "market.android.com");
}

static bool ShouldRedirect(const char* hostname) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  return state.GetDomainState(hostname, true /* SNI ok */, &domain_state) &&
         domain_state.ShouldUpgradeToSSL();
}

static bool HasState(const char* hostname) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  return state.GetDomainState(hostname, true /* SNI ok */, &domain_state);
}

static bool HasPublicKeyPins(const char* hostname, bool sni_enabled) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  if (!state.GetDomainState(hostname, sni_enabled, &domain_state))
    return false;

  return domain_state.HasPublicKeyPins();
}

static bool HasPublicKeyPins(const char* hostname) {
  return HasPublicKeyPins(hostname, true);
}

static bool OnlyPinning(const char *hostname) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  if (!state.GetDomainState(hostname, true /* SNI ok */, &domain_state))
    return false;

  return (domain_state.static_spki_hashes.size() > 0 ||
          domain_state.bad_static_spki_hashes.size() > 0 ||
          domain_state.dynamic_spki_hashes.size() > 0) &&
         !domain_state.ShouldUpgradeToSSL();
}

TEST_F(TransportSecurityStateTest, Preloaded) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;

  // We do more extensive checks for the first domain.
  EXPECT_TRUE(state.GetDomainState("www.paypal.com", true, &domain_state));
  EXPECT_EQ(domain_state.upgrade_mode,
            TransportSecurityState::DomainState::MODE_FORCE_HTTPS);
  EXPECT_FALSE(domain_state.sts_include_subdomains);
  EXPECT_FALSE(domain_state.pkp_include_subdomains);

  EXPECT_TRUE(HasState("paypal.com"));
  EXPECT_FALSE(HasState("www2.paypal.com"));
  EXPECT_FALSE(HasState("www2.paypal.com"));

  // Google hosts:

  EXPECT_TRUE(ShouldRedirect("chrome.google.com"));
  EXPECT_TRUE(ShouldRedirect("checkout.google.com"));
  EXPECT_TRUE(ShouldRedirect("wallet.google.com"));
  EXPECT_TRUE(ShouldRedirect("docs.google.com"));
  EXPECT_TRUE(ShouldRedirect("sites.google.com"));
  EXPECT_TRUE(ShouldRedirect("drive.google.com"));
  EXPECT_TRUE(ShouldRedirect("spreadsheets.google.com"));
  EXPECT_TRUE(ShouldRedirect("appengine.google.com"));
  EXPECT_TRUE(ShouldRedirect("market.android.com"));
  EXPECT_TRUE(ShouldRedirect("encrypted.google.com"));
  EXPECT_TRUE(ShouldRedirect("accounts.google.com"));
  EXPECT_TRUE(ShouldRedirect("profiles.google.com"));
  EXPECT_TRUE(ShouldRedirect("mail.google.com"));
  EXPECT_TRUE(ShouldRedirect("chatenabled.mail.google.com"));
  EXPECT_TRUE(ShouldRedirect("talkgadget.google.com"));
  EXPECT_TRUE(ShouldRedirect("hostedtalkgadget.google.com"));
  EXPECT_TRUE(ShouldRedirect("talk.google.com"));
  EXPECT_TRUE(ShouldRedirect("plus.google.com"));
  EXPECT_TRUE(ShouldRedirect("groups.google.com"));
  EXPECT_TRUE(ShouldRedirect("apis.google.com"));
  EXPECT_FALSE(ShouldRedirect("chart.apis.google.com"));
  EXPECT_TRUE(ShouldRedirect("ssl.google-analytics.com"));
  EXPECT_TRUE(ShouldRedirect("gmail.com"));
  EXPECT_TRUE(ShouldRedirect("www.gmail.com"));
  EXPECT_TRUE(ShouldRedirect("googlemail.com"));
  EXPECT_TRUE(ShouldRedirect("www.googlemail.com"));
  EXPECT_TRUE(ShouldRedirect("googleplex.com"));
  EXPECT_TRUE(ShouldRedirect("www.googleplex.com"));
  EXPECT_FALSE(HasState("m.gmail.com"));
  EXPECT_FALSE(HasState("m.googlemail.com"));

  EXPECT_TRUE(OnlyPinning("www.google.com"));
  EXPECT_TRUE(OnlyPinning("foo.google.com"));
  EXPECT_TRUE(OnlyPinning("google.com"));
  EXPECT_TRUE(OnlyPinning("www.youtube.com"));
  EXPECT_TRUE(OnlyPinning("youtube.com"));
  EXPECT_TRUE(OnlyPinning("i.ytimg.com"));
  EXPECT_TRUE(OnlyPinning("ytimg.com"));
  EXPECT_TRUE(OnlyPinning("googleusercontent.com"));
  EXPECT_TRUE(OnlyPinning("www.googleusercontent.com"));
  EXPECT_TRUE(OnlyPinning("www.google-analytics.com"));
  EXPECT_TRUE(OnlyPinning("googleapis.com"));
  EXPECT_TRUE(OnlyPinning("googleadservices.com"));
  EXPECT_TRUE(OnlyPinning("googlecode.com"));
  EXPECT_TRUE(OnlyPinning("appspot.com"));
  EXPECT_TRUE(OnlyPinning("googlesyndication.com"));
  EXPECT_TRUE(OnlyPinning("doubleclick.net"));
  EXPECT_TRUE(OnlyPinning("googlegroups.com"));

  // Tests for domains that don't work without SNI.
  EXPECT_FALSE(state.GetDomainState("gmail.com", false, &domain_state));
  EXPECT_FALSE(state.GetDomainState("www.gmail.com", false, &domain_state));
  EXPECT_FALSE(state.GetDomainState("m.gmail.com", false, &domain_state));
  EXPECT_FALSE(state.GetDomainState("googlemail.com", false, &domain_state));
  EXPECT_FALSE(state.GetDomainState("www.googlemail.com", false,
                                    &domain_state));
  EXPECT_FALSE(state.GetDomainState("m.googlemail.com", false, &domain_state));

  // Other hosts:

  EXPECT_TRUE(ShouldRedirect("aladdinschools.appspot.com"));

  EXPECT_TRUE(ShouldRedirect("ottospora.nl"));
  EXPECT_TRUE(ShouldRedirect("www.ottospora.nl"));

  EXPECT_TRUE(ShouldRedirect("www.paycheckrecords.com"));

  EXPECT_TRUE(ShouldRedirect("lastpass.com"));
  EXPECT_TRUE(ShouldRedirect("www.lastpass.com"));
  EXPECT_FALSE(HasState("blog.lastpass.com"));

  EXPECT_TRUE(ShouldRedirect("keyerror.com"));
  EXPECT_TRUE(ShouldRedirect("www.keyerror.com"));

  EXPECT_TRUE(ShouldRedirect("entropia.de"));
  EXPECT_TRUE(ShouldRedirect("www.entropia.de"));
  EXPECT_FALSE(HasState("foo.entropia.de"));

  EXPECT_TRUE(ShouldRedirect("www.elanex.biz"));
  EXPECT_FALSE(HasState("elanex.biz"));
  EXPECT_FALSE(HasState("foo.elanex.biz"));

  EXPECT_TRUE(ShouldRedirect("sunshinepress.org"));
  EXPECT_TRUE(ShouldRedirect("www.sunshinepress.org"));
  EXPECT_TRUE(ShouldRedirect("a.b.sunshinepress.org"));

  EXPECT_TRUE(ShouldRedirect("www.noisebridge.net"));
  EXPECT_FALSE(HasState("noisebridge.net"));
  EXPECT_FALSE(HasState("foo.noisebridge.net"));

  EXPECT_TRUE(ShouldRedirect("neg9.org"));
  EXPECT_FALSE(HasState("www.neg9.org"));

  EXPECT_TRUE(ShouldRedirect("riseup.net"));
  EXPECT_TRUE(ShouldRedirect("foo.riseup.net"));

  EXPECT_TRUE(ShouldRedirect("factor.cc"));
  EXPECT_FALSE(HasState("www.factor.cc"));

  EXPECT_TRUE(ShouldRedirect("members.mayfirst.org"));
  EXPECT_TRUE(ShouldRedirect("support.mayfirst.org"));
  EXPECT_TRUE(ShouldRedirect("id.mayfirst.org"));
  EXPECT_TRUE(ShouldRedirect("lists.mayfirst.org"));
  EXPECT_FALSE(HasState("www.mayfirst.org"));

  EXPECT_TRUE(ShouldRedirect("romab.com"));
  EXPECT_TRUE(ShouldRedirect("www.romab.com"));
  EXPECT_TRUE(ShouldRedirect("foo.romab.com"));

  EXPECT_TRUE(ShouldRedirect("logentries.com"));
  EXPECT_TRUE(ShouldRedirect("www.logentries.com"));
  EXPECT_FALSE(HasState("foo.logentries.com"));

  EXPECT_TRUE(ShouldRedirect("stripe.com"));
  EXPECT_TRUE(ShouldRedirect("foo.stripe.com"));

  EXPECT_TRUE(ShouldRedirect("cloudsecurityalliance.org"));
  EXPECT_TRUE(ShouldRedirect("foo.cloudsecurityalliance.org"));

  EXPECT_TRUE(ShouldRedirect("login.sapo.pt"));
  EXPECT_TRUE(ShouldRedirect("foo.login.sapo.pt"));

  EXPECT_TRUE(ShouldRedirect("mattmccutchen.net"));
  EXPECT_TRUE(ShouldRedirect("foo.mattmccutchen.net"));

  EXPECT_TRUE(ShouldRedirect("betnet.fr"));
  EXPECT_TRUE(ShouldRedirect("foo.betnet.fr"));

  EXPECT_TRUE(ShouldRedirect("uprotect.it"));
  EXPECT_TRUE(ShouldRedirect("foo.uprotect.it"));

  EXPECT_TRUE(ShouldRedirect("squareup.com"));
  EXPECT_FALSE(HasState("foo.squareup.com"));

  EXPECT_TRUE(ShouldRedirect("cert.se"));
  EXPECT_TRUE(ShouldRedirect("foo.cert.se"));

  EXPECT_TRUE(ShouldRedirect("crypto.is"));
  EXPECT_TRUE(ShouldRedirect("foo.crypto.is"));

  EXPECT_TRUE(ShouldRedirect("simon.butcher.name"));
  EXPECT_TRUE(ShouldRedirect("foo.simon.butcher.name"));

  EXPECT_TRUE(ShouldRedirect("linx.net"));
  EXPECT_TRUE(ShouldRedirect("foo.linx.net"));

  EXPECT_TRUE(ShouldRedirect("dropcam.com"));
  EXPECT_TRUE(ShouldRedirect("www.dropcam.com"));
  EXPECT_FALSE(HasState("foo.dropcam.com"));

  EXPECT_TRUE(state.GetDomainState("torproject.org", false, &domain_state));
  EXPECT_FALSE(domain_state.static_spki_hashes.empty());
  EXPECT_TRUE(state.GetDomainState("www.torproject.org", false,
                                   &domain_state));
  EXPECT_FALSE(domain_state.static_spki_hashes.empty());
  EXPECT_TRUE(state.GetDomainState("check.torproject.org", false,
                                   &domain_state));
  EXPECT_FALSE(domain_state.static_spki_hashes.empty());
  EXPECT_TRUE(state.GetDomainState("blog.torproject.org", false,
                                   &domain_state));
  EXPECT_FALSE(domain_state.static_spki_hashes.empty());
  EXPECT_TRUE(ShouldRedirect("ebanking.indovinabank.com.vn"));
  EXPECT_TRUE(ShouldRedirect("foo.ebanking.indovinabank.com.vn"));

  EXPECT_TRUE(ShouldRedirect("epoxate.com"));
  EXPECT_FALSE(HasState("foo.epoxate.com"));

  EXPECT_TRUE(HasPublicKeyPins("torproject.org"));
  EXPECT_TRUE(HasPublicKeyPins("www.torproject.org"));
  EXPECT_TRUE(HasPublicKeyPins("check.torproject.org"));
  EXPECT_TRUE(HasPublicKeyPins("blog.torproject.org"));
  EXPECT_FALSE(HasState("foo.torproject.org"));

  EXPECT_TRUE(ShouldRedirect("www.moneybookers.com"));
  EXPECT_FALSE(HasState("moneybookers.com"));

  EXPECT_TRUE(ShouldRedirect("ledgerscope.net"));
  EXPECT_TRUE(ShouldRedirect("www.ledgerscope.net"));
  EXPECT_FALSE(HasState("status.ledgerscope.net"));

  EXPECT_TRUE(ShouldRedirect("foo.app.recurly.com"));
  EXPECT_TRUE(ShouldRedirect("foo.api.recurly.com"));

  EXPECT_TRUE(ShouldRedirect("greplin.com"));
  EXPECT_TRUE(ShouldRedirect("www.greplin.com"));
  EXPECT_FALSE(HasState("foo.greplin.com"));

  EXPECT_TRUE(ShouldRedirect("luneta.nearbuysystems.com"));
  EXPECT_TRUE(ShouldRedirect("foo.luneta.nearbuysystems.com"));

  EXPECT_TRUE(ShouldRedirect("ubertt.org"));
  EXPECT_TRUE(ShouldRedirect("foo.ubertt.org"));

  EXPECT_TRUE(ShouldRedirect("pixi.me"));
  EXPECT_TRUE(ShouldRedirect("www.pixi.me"));

  EXPECT_TRUE(ShouldRedirect("grepular.com"));
  EXPECT_TRUE(ShouldRedirect("www.grepular.com"));

  EXPECT_TRUE(ShouldRedirect("mydigipass.com"));
  EXPECT_FALSE(ShouldRedirect("foo.mydigipass.com"));
  EXPECT_TRUE(ShouldRedirect("www.mydigipass.com"));
  EXPECT_FALSE(ShouldRedirect("foo.www.mydigipass.com"));
  EXPECT_TRUE(ShouldRedirect("developer.mydigipass.com"));
  EXPECT_FALSE(ShouldRedirect("foo.developer.mydigipass.com"));
  EXPECT_TRUE(ShouldRedirect("www.developer.mydigipass.com"));
  EXPECT_FALSE(ShouldRedirect("foo.www.developer.mydigipass.com"));
  EXPECT_TRUE(ShouldRedirect("sandbox.mydigipass.com"));
  EXPECT_FALSE(ShouldRedirect("foo.sandbox.mydigipass.com"));
  EXPECT_TRUE(ShouldRedirect("www.sandbox.mydigipass.com"));
  EXPECT_FALSE(ShouldRedirect("foo.www.sandbox.mydigipass.com"));

  EXPECT_TRUE(ShouldRedirect("crypto.cat"));
  EXPECT_FALSE(ShouldRedirect("foo.crypto.cat"));

  EXPECT_TRUE(ShouldRedirect("bigshinylock.minazo.net"));
  EXPECT_TRUE(ShouldRedirect("foo.bigshinylock.minazo.net"));

  EXPECT_TRUE(ShouldRedirect("crate.io"));
  EXPECT_TRUE(ShouldRedirect("foo.crate.io"));

  EXPECT_TRUE(HasPublicKeyPins("www.twitter.com"));
}

TEST_F(TransportSecurityStateTest, LongNames) {
  TransportSecurityState state;
  const char kLongName[] =
      "lookupByWaveIdHashAndWaveIdIdAndWaveIdDomainAndWaveletIdIdAnd"
      "WaveletIdDomainAndBlipBlipid";
  TransportSecurityState::DomainState domain_state;
  // Just checks that we don't hit a NOTREACHED.
  EXPECT_FALSE(state.GetDomainState(kLongName, true, &domain_state));
}

TEST_F(TransportSecurityStateTest, BuiltinCertPins) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;

  EXPECT_TRUE(state.GetDomainState("chrome.google.com", true, &domain_state));
  EXPECT_TRUE(HasPublicKeyPins("chrome.google.com"));

  HashValueVector hashes;
  std::string failure_log;
  // Checks that a built-in list does exist.
  EXPECT_FALSE(domain_state.CheckPublicKeyPins(hashes, &failure_log));
  EXPECT_FALSE(HasPublicKeyPins("www.paypal.com"));

  EXPECT_TRUE(HasPublicKeyPins("docs.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("1.docs.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("sites.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("drive.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("spreadsheets.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("wallet.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("checkout.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("appengine.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("market.android.com"));
  EXPECT_TRUE(HasPublicKeyPins("encrypted.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("accounts.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("profiles.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("mail.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("chatenabled.mail.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("talkgadget.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("hostedtalkgadget.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("talk.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("plus.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("groups.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("apis.google.com"));

  EXPECT_TRUE(HasPublicKeyPins("ssl.gstatic.com"));
  EXPECT_TRUE(HasPublicKeyPins("gstatic.com"));
  EXPECT_TRUE(HasPublicKeyPins("www.gstatic.com"));
  EXPECT_TRUE(HasPublicKeyPins("ssl.google-analytics.com"));
  EXPECT_TRUE(HasPublicKeyPins("www.googleplex.com"));

  // Disabled in order to help track down pinning failures --agl
  EXPECT_TRUE(HasPublicKeyPins("twitter.com"));
  EXPECT_FALSE(HasPublicKeyPins("foo.twitter.com"));
  EXPECT_TRUE(HasPublicKeyPins("www.twitter.com"));
  EXPECT_TRUE(HasPublicKeyPins("api.twitter.com"));
  EXPECT_TRUE(HasPublicKeyPins("oauth.twitter.com"));
  EXPECT_TRUE(HasPublicKeyPins("mobile.twitter.com"));
  EXPECT_TRUE(HasPublicKeyPins("dev.twitter.com"));
  EXPECT_TRUE(HasPublicKeyPins("business.twitter.com"));
  EXPECT_TRUE(HasPublicKeyPins("platform.twitter.com"));
  EXPECT_TRUE(HasPublicKeyPins("si0.twimg.com"));
}

static bool AddHash(const std::string& type_and_base64,
                    HashValueVector* out) {
  HashValue hash;
  if (!hash.FromString(type_and_base64))
    return false;

  out->push_back(hash);
  return true;
}

TEST_F(TransportSecurityStateTest, PinValidationWithoutRejectedCerts) {
  // kGoodPath is blog.torproject.org.
  static const char* kGoodPath[] = {
    "sha1/m9lHYJYke9k0GtVZ+bXSQYE8nDI=",
    "sha1/o5OZxATDsgmwgcIfIWIneMJ0jkw=",
    "sha1/wHqYaI2J+6sFZAwRfap9ZbjKzE4=",
    NULL,
  };

  // kBadPath is plus.google.com via Trustcenter, which is utterly wrong for
  // torproject.org.
  static const char* kBadPath[] = {
    "sha1/4BjDjn8v2lWeUFQnqSs0BgbIcrU=",
    "sha1/gzuEEAB/bkqdQS3EIjk2by7lW+k=",
    "sha1/SOZo+SvSspXXR9gjIBBPM5iQn9Q=",
    NULL,
  };

  HashValueVector good_hashes, bad_hashes;

  for (size_t i = 0; kGoodPath[i]; i++) {
    EXPECT_TRUE(AddHash(kGoodPath[i], &good_hashes));
  }
  for (size_t i = 0; kBadPath[i]; i++) {
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));
  }

  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(state.GetDomainState("blog.torproject.org", true, &domain_state));
  EXPECT_TRUE(domain_state.HasPublicKeyPins());

  std::string failure_log;
  EXPECT_TRUE(domain_state.CheckPublicKeyPins(good_hashes, &failure_log));
  EXPECT_FALSE(domain_state.CheckPublicKeyPins(bad_hashes, &failure_log));
}

TEST_F(TransportSecurityStateTest, OptionalHSTSCertPins) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;

  EXPECT_FALSE(ShouldRedirect("www.google-analytics.com"));

  EXPECT_FALSE(HasPublicKeyPins("www.google-analytics.com", false));
  EXPECT_TRUE(HasPublicKeyPins("www.google-analytics.com"));
  EXPECT_TRUE(HasPublicKeyPins("google.com"));
  EXPECT_TRUE(HasPublicKeyPins("www.google.com"));
  EXPECT_TRUE(HasPublicKeyPins("mail-attachment.googleusercontent.com"));
  EXPECT_TRUE(HasPublicKeyPins("www.youtube.com"));
  EXPECT_TRUE(HasPublicKeyPins("i.ytimg.com"));
  EXPECT_TRUE(HasPublicKeyPins("googleapis.com"));
  EXPECT_TRUE(HasPublicKeyPins("ajax.googleapis.com"));
  EXPECT_TRUE(HasPublicKeyPins("googleadservices.com"));
  EXPECT_TRUE(HasPublicKeyPins("pagead2.googleadservices.com"));
  EXPECT_TRUE(HasPublicKeyPins("googlecode.com"));
  EXPECT_TRUE(HasPublicKeyPins("kibbles.googlecode.com"));
  EXPECT_TRUE(HasPublicKeyPins("appspot.com"));
  EXPECT_TRUE(HasPublicKeyPins("googlesyndication.com"));
  EXPECT_TRUE(HasPublicKeyPins("doubleclick.net"));
  EXPECT_TRUE(HasPublicKeyPins("ad.doubleclick.net"));
  EXPECT_FALSE(HasPublicKeyPins("learn.doubleclick.net"));
  EXPECT_TRUE(HasPublicKeyPins("a.googlegroups.com"));
  EXPECT_FALSE(HasPublicKeyPins("a.googlegroups.com", false));
}

TEST_F(TransportSecurityStateTest, OverrideBuiltins) {
  EXPECT_TRUE(HasPublicKeyPins("google.com"));
  EXPECT_FALSE(ShouldRedirect("google.com"));
  EXPECT_FALSE(ShouldRedirect("www.google.com"));

  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  domain_state.upgrade_expiry = expiry;
  EnableHost(&state, "www.google.com", domain_state);

  EXPECT_TRUE(state.GetDomainState("www.google.com", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, GooglePinnedProperties) {
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.example.com", true));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.paypal.com", true));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "mail.twitter.com", true));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.google.com.int", true));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "jottit.com", true));
  // learn.doubleclick.net has a more specific match than
  // *.doubleclick.com, and has 0 or NULL for its required certs.
  // This test ensures that the exact-match-preferred behavior
  // works.
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "learn.doubleclick.net", true));

  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "encrypted.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "mail.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "accounts.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "doubleclick.net", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "ad.doubleclick.net", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "youtube.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "www.profiles.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "checkout.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "googleadservices.com", true));

  // Test with sni_enabled false:
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.example.com", false));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.paypal.com", false));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "checkout.google.com", false));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "googleadservices.com", false));

  // Test some SNI hosts:
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "gmail.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "googlegroups.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "www.googlegroups.com", true));
  // Expect to fail for SNI hosts when not searching the SNI list:
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "gmail.com", false));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "googlegroups.com", false));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.googlegroups.com", false));
}

}  // namespace net
