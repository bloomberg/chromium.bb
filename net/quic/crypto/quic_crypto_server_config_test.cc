// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/quic_crypto_server_config.h"

#include <stdarg.h>

#include "base/stl_util.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/crypto/crypto_handshake_message.h"
#include "net/quic/crypto/crypto_server_config_protobuf.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/crypto/strike_register_client.h"
#include "net/quic/quic_time.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::make_pair;
using std::map;
using std::pair;
using std::string;
using std::vector;

namespace net {
namespace test {

class QuicCryptoServerConfigPeer {
 public:
  explicit QuicCryptoServerConfigPeer(QuicCryptoServerConfig* server_config)
      : server_config_(server_config) {}

  string NewSourceAddressToken(IPEndPoint ip,
                               QuicRandom* rand,
                               QuicWallTime now) {
    return server_config_->NewSourceAddressToken(ip, rand, now);
  }

  bool ValidateSourceAddressToken(StringPiece srct,
                                  IPEndPoint ip,
                                  QuicWallTime now) {
    return server_config_->ValidateSourceAddressToken(srct, ip, now);
  }

  base::Lock* GetStrikeRegisterClientLock() {
    return &server_config_->strike_register_client_lock_;
  }

  // CheckConfigs compares the state of the Configs in |server_config_| to the
  // description given as arguments. The arguments are given as NULL-terminated
  // pairs. The first of each pair is the server config ID of a Config. The
  // second is a boolean describing whether the config is the primary. For
  // example:
  //   CheckConfigs(NULL);  // checks that no Configs are loaded.
  //
  //   // Checks that exactly three Configs are loaded with the given IDs and
  //   // status.
  //   CheckConfigs(
  //     "id1", false,
  //     "id2", true,
  //     "id3", false,
  //     NULL);
  void CheckConfigs(const char* server_config_id1, ...) {
    va_list ap;
    va_start(ap, server_config_id1);

    vector<pair<ServerConfigID, bool> > expected;
    bool first = true;
    for (;;) {
      const char* server_config_id;
      if (first) {
        server_config_id = server_config_id1;
        first = false;
      } else {
        server_config_id = va_arg(ap, const char*);
      }

      if (!server_config_id) {
        break;
      }

      // varargs will promote the value to an int so we have to read that from
      // the stack and cast down.
      const bool is_primary = static_cast<bool>(va_arg(ap, int));
      expected.push_back(make_pair(server_config_id, is_primary));
    }

    va_end(ap);

    base::AutoLock locked(server_config_->configs_lock_);

    ASSERT_EQ(expected.size(), server_config_->configs_.size())
        << ConfigsDebug();

    for (QuicCryptoServerConfig::ConfigMap::const_iterator
             i = server_config_->configs_.begin();
         i != server_config_->configs_.end(); ++i) {
      bool found = false;
      for (vector<pair<ServerConfigID, bool> >::iterator j = expected.begin();
           j != expected.end(); ++j) {
        if (i->first == j->first && i->second->is_primary == j->second) {
          found = true;
          j->first.clear();
          break;
        }
      }

      ASSERT_TRUE(found) << "Failed to find match for " << i->first
                         << " in configs:\n" << ConfigsDebug();
    }
  }

  // ConfigsDebug returns a string that contains debugging information about
  // the set of Configs loaded in |server_config_| and their status.
  // ConfigsDebug() should be called after acquiring
  // server_config_->configs_lock_.
  string ConfigsDebug() {
    if (server_config_->configs_.empty()) {
      return "No Configs in QuicCryptoServerConfig";
    }

    string s;

    for (QuicCryptoServerConfig::ConfigMap::const_iterator
             i = server_config_->configs_.begin();
         i != server_config_->configs_.end(); ++i) {
      const scoped_refptr<QuicCryptoServerConfig::Config> config = i->second;
      if (config->is_primary) {
        s += "(primary) ";
      } else {
        s += "          ";
      }
      s += config->id;
      s += "\n";
    }

    return s;
  }

  void SelectNewPrimaryConfig(int seconds) {
    base::AutoLock locked(server_config_->configs_lock_);
    server_config_->SelectNewPrimaryConfig(
        QuicWallTime::FromUNIXSeconds(seconds));
  }

 private:
  const QuicCryptoServerConfig* server_config_;
};

class TestStrikeRegisterClient : public StrikeRegisterClient {
 public:
  explicit TestStrikeRegisterClient(QuicCryptoServerConfig* config)
      : config_(config),
        is_known_orbit_called_(false) {
  }

  virtual bool IsKnownOrbit(StringPiece orbit) const OVERRIDE {
    // Ensure that the strike register client lock is not held.
    QuicCryptoServerConfigPeer peer(config_);
    base::Lock* m = peer.GetStrikeRegisterClientLock();
    // In Chromium, we will dead lock if the lock is held by the current thread.
    // Chromium doesn't have AssertNotHeld API call.
    // m->AssertNotHeld();
    base::AutoLock lock(*m);

    is_known_orbit_called_ = true;
    return true;
  }

  virtual void VerifyNonceIsValidAndUnique(
      StringPiece nonce,
      QuicWallTime now,
      ResultCallback* cb) OVERRIDE {
    LOG(FATAL) << "Not implemented";
  }

  bool is_known_orbit_called() { return is_known_orbit_called_; }

 private:
  QuicCryptoServerConfig* config_;
  mutable bool is_known_orbit_called_;
};

TEST(QuicCryptoServerConfigTest, ServerConfig) {
  QuicRandom* rand = QuicRandom::GetInstance();
  QuicCryptoServerConfig server(QuicCryptoServerConfig::TESTING, rand);
  MockClock clock;

  scoped_ptr<CryptoHandshakeMessage>(
      server.AddDefaultConfig(rand, &clock,
                              QuicCryptoServerConfig::ConfigOptions()));
}

TEST(QuicCryptoServerConfigTest, GetOrbitIsCalledWithoutTheStrikeRegisterLock) {
  QuicRandom* rand = QuicRandom::GetInstance();
  QuicCryptoServerConfig server(QuicCryptoServerConfig::TESTING, rand);
  MockClock clock;

  TestStrikeRegisterClient* strike_register =
      new TestStrikeRegisterClient(&server);
  server.SetStrikeRegisterClient(strike_register);

  QuicCryptoServerConfig::ConfigOptions options;
  scoped_ptr<CryptoHandshakeMessage>(
      server.AddDefaultConfig(rand, &clock, options));
  EXPECT_TRUE(strike_register->is_known_orbit_called());
}

TEST(QuicCryptoServerConfigTest, SourceAddressTokens) {
  QuicRandom* rand = QuicRandom::GetInstance();
  QuicCryptoServerConfig server(QuicCryptoServerConfig::TESTING, rand);
  IPAddressNumber ip;
  CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
  IPEndPoint ip4 = IPEndPoint(ip, 1);
  CHECK(ParseIPLiteralToNumber("2001:db8:0::42", &ip));
  IPEndPoint ip6 = IPEndPoint(ip, 2);
  MockClock clock;
  clock.AdvanceTime(QuicTime::Delta::FromSeconds(1000000));
  QuicCryptoServerConfigPeer peer(&server);

  QuicWallTime now = clock.WallNow();
  const QuicWallTime original_time = now;

  const string token4 = peer.NewSourceAddressToken(ip4, rand, now);
  const string token6 = peer.NewSourceAddressToken(ip6, rand, now);
  EXPECT_TRUE(peer.ValidateSourceAddressToken(token4, ip4, now));
  EXPECT_FALSE(peer.ValidateSourceAddressToken(token4, ip6, now));
  EXPECT_TRUE(peer.ValidateSourceAddressToken(token6, ip6, now));

  now = original_time.Add(QuicTime::Delta::FromSeconds(86400 * 7));
  EXPECT_FALSE(peer.ValidateSourceAddressToken(token4, ip4, now));

  now = original_time.Subtract(QuicTime::Delta::FromSeconds(3600 * 2));
  EXPECT_FALSE(peer.ValidateSourceAddressToken(token4, ip4, now));
}

class CryptoServerConfigsTest : public ::testing::Test {
 public:
  CryptoServerConfigsTest()
      : rand_(QuicRandom::GetInstance()),
        config_(QuicCryptoServerConfig::TESTING, rand_),
        test_peer_(&config_) {}

  virtual void SetUp() {
    clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1000));
  }

  // SetConfigs constructs suitable config protobufs and calls SetConfigs on
  // |config_|. The arguments are given as NULL-terminated pairs. The first of
  // each pair is the server config ID of a Config. The second is the
  // |primary_time| of that Config, given in epoch seconds. (Although note
  // that, in these tests, time is set to 1000 seconds since the epoch.) For
  // example:
  //   SetConfigs(NULL);  // calls |config_.SetConfigs| with no protobufs.
  //
  //   // Calls |config_.SetConfigs| with two protobufs: one for a Config with
  //   // a |primary_time| of 900 and priority 1, and another with
  //   // a |primary_time| of 1000 and priority 2.

  //   CheckConfigs(
  //     "id1", 900, 1,
  //     "id2", 1000, 2,
  //     NULL);
  //
  // If the server config id starts with "INVALID" then the generated protobuf
  // will be invalid.
  void SetConfigs(const char* server_config_id1, ...) {
    const char kOrbit[] = "12345678";

    va_list ap;
    va_start(ap, server_config_id1);
    bool has_invalid = false;
    bool is_empty = true;

    vector<QuicServerConfigProtobuf*> protobufs;
    bool first = true;
    for (;;) {
      const char* server_config_id;
      if (first) {
        server_config_id = server_config_id1;
        first = false;
      } else {
        server_config_id = va_arg(ap, const char*);
      }

      if (!server_config_id) {
        break;
      }

      is_empty = false;
      int primary_time = va_arg(ap, int);
      int priority = va_arg(ap, int);

      QuicCryptoServerConfig::ConfigOptions options;
      options.id = server_config_id;
      options.orbit = kOrbit;
      QuicServerConfigProtobuf* protobuf(
          QuicCryptoServerConfig::GenerateConfig(rand_, &clock_, options));
      protobuf->set_primary_time(primary_time);
      protobuf->set_priority(priority);
      if (string(server_config_id).find("INVALID") == 0) {
        protobuf->clear_key();
        has_invalid = true;
      }
      protobufs.push_back(protobuf);
    }

    ASSERT_EQ(!has_invalid && !is_empty,
              config_.SetConfigs(protobufs, clock_.WallNow()));
    STLDeleteElements(&protobufs);
  }

 protected:
  QuicRandom* const rand_;
  MockClock clock_;
  QuicCryptoServerConfig config_;
  QuicCryptoServerConfigPeer test_peer_;
};

TEST_F(CryptoServerConfigsTest, NoConfigs) {
  test_peer_.CheckConfigs(NULL);
}

TEST_F(CryptoServerConfigsTest, MakePrimaryFirst) {
  // Make sure that "b" is primary even though "a" comes first.
  SetConfigs("a", 1100, 1,
             "b", 900, 1,
             NULL);
  test_peer_.CheckConfigs(
      "a", false,
      "b", true,
      NULL);
}

TEST_F(CryptoServerConfigsTest, MakePrimarySecond) {
  // Make sure that a remains primary after b is added.
  SetConfigs("a", 900, 1,
             "b", 1100, 1,
             NULL);
  test_peer_.CheckConfigs(
      "a", true,
      "b", false,
      NULL);
}

TEST_F(CryptoServerConfigsTest, Delete) {
  // Ensure that configs get deleted when removed.
  SetConfigs("a", 800, 1,
             "b", 900, 1,
             "c", 1100, 1,
             NULL);
  test_peer_.CheckConfigs(
      "a", false,
      "b", true,
      "c", false,
      NULL);
  SetConfigs("b", 900, 1,
             "c", 1100, 1,
             NULL);
  test_peer_.CheckConfigs(
      "b", true,
      "c", false,
      NULL);
}

TEST_F(CryptoServerConfigsTest, DeletePrimary) {
  // Ensure that deleting the primary config works.
  SetConfigs("a", 800, 1,
             "b", 900, 1,
             "c", 1100, 1,
             NULL);
  test_peer_.CheckConfigs(
      "a", false,
      "b", true,
      "c", false,
      NULL);
  SetConfigs("a", 800, 1,
             "c", 1100, 1,
             NULL);
  test_peer_.CheckConfigs(
      "a", true,
      "c", false,
      NULL);
}

TEST_F(CryptoServerConfigsTest, FailIfDeletingAllConfigs) {
  // Ensure that configs get deleted when removed.
  SetConfigs("a", 800, 1,
             "b", 900, 1,
             NULL);
  test_peer_.CheckConfigs(
      "a", false,
      "b", true,
      NULL);
  SetConfigs(NULL);
  // Config change is rejected, still using old configs.
  test_peer_.CheckConfigs(
      "a", false,
      "b", true,
      NULL);
}

TEST_F(CryptoServerConfigsTest, ChangePrimaryTime) {
  // Check that updates to primary time get picked up.
  SetConfigs("a", 400, 1,
             "b", 800, 1,
             "c", 1200, 1,
             NULL);
  test_peer_.SelectNewPrimaryConfig(500);
  test_peer_.CheckConfigs(
      "a", true,
      "b", false,
      "c", false,
      NULL);
  SetConfigs("a", 1200, 1,
             "b", 800, 1,
             "c", 400, 1,
             NULL);
  test_peer_.SelectNewPrimaryConfig(500);
  test_peer_.CheckConfigs(
      "a", false,
      "b", false,
      "c", true,
      NULL);
}

TEST_F(CryptoServerConfigsTest, AllConfigsInThePast) {
  // Check that the most recent config is selected.
  SetConfigs("a", 400, 1,
             "b", 800, 1,
             "c", 1200, 1,
             NULL);
  test_peer_.SelectNewPrimaryConfig(1500);
  test_peer_.CheckConfigs(
      "a", false,
      "b", false,
      "c", true,
      NULL);
}

TEST_F(CryptoServerConfigsTest, AllConfigsInTheFuture) {
  // Check that the first config is selected.
  SetConfigs("a", 400, 1,
             "b", 800, 1,
             "c", 1200, 1,
             NULL);
  test_peer_.SelectNewPrimaryConfig(100);
  test_peer_.CheckConfigs(
      "a", true,
      "b", false,
      "c", false,
      NULL);
}

TEST_F(CryptoServerConfigsTest, SortByPriority) {
  // Check that priority is used to decide on a primary config when
  // configs have the same primary time.
  SetConfigs("a", 900, 1,
             "b", 900, 2,
             "c", 900, 3,
             NULL);
  test_peer_.CheckConfigs(
      "a", true,
      "b", false,
      "c", false,
      NULL);
  test_peer_.SelectNewPrimaryConfig(800);
  test_peer_.CheckConfigs(
      "a", true,
      "b", false,
      "c", false,
      NULL);
  test_peer_.SelectNewPrimaryConfig(1000);
  test_peer_.CheckConfigs(
      "a", true,
      "b", false,
      "c", false,
      NULL);

  // Change priorities and expect sort order to change.
  SetConfigs("a", 900, 2,
             "b", 900, 1,
             "c", 900, 0,
             NULL);
  test_peer_.CheckConfigs(
      "a", false,
      "b", false,
      "c", true,
      NULL);
  test_peer_.SelectNewPrimaryConfig(800);
  test_peer_.CheckConfigs(
      "a", false,
      "b", false,
      "c", true,
      NULL);
  test_peer_.SelectNewPrimaryConfig(1000);
  test_peer_.CheckConfigs(
      "a", false,
      "b", false,
      "c", true,
      NULL);
}

TEST_F(CryptoServerConfigsTest, AdvancePrimary) {
  // Check that a new primary config is enabled at the right time.
  SetConfigs("a", 900, 1,
             "b", 1100, 1,
             NULL);
  test_peer_.SelectNewPrimaryConfig(1000);
  test_peer_.CheckConfigs(
      "a", true,
      "b", false,
      NULL);
  test_peer_.SelectNewPrimaryConfig(1101);
  test_peer_.CheckConfigs(
      "a", false,
      "b", true,
      NULL);
}

TEST_F(CryptoServerConfigsTest, InvalidConfigs) {
  // Ensure that invalid configs don't change anything.
  SetConfigs("a", 800, 1,
             "b", 900, 1,
             "c", 1100, 1,
             NULL);
  test_peer_.CheckConfigs(
      "a", false,
      "b", true,
      "c", false,
      NULL);
  SetConfigs("a", 800, 1,
             "c", 1100, 1,
             "INVALID1", 1000, 1,
             NULL);
  test_peer_.CheckConfigs(
      "a", false,
      "b", true,
      "c", false,
      NULL);
}

}  // namespace test
}  // namespace net
