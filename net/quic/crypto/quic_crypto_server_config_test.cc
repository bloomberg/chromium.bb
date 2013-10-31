// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/quic_crypto_server_config.h"

#include <stdarg.h>

#include "base/stl_util.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_server_config_protobuf.h"
#include "net/quic/crypto/quic_random.h"
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

TEST(QuicCryptoServerConfigTest, ServerConfig) {
  QuicRandom* rand = QuicRandom::GetInstance();
  QuicCryptoServerConfig server(QuicCryptoServerConfig::TESTING, rand);
  MockClock clock;

  scoped_ptr<CryptoHandshakeMessage>(
      server.AddDefaultConfig(rand, &clock,
                              QuicCryptoServerConfig::ConfigOptions()));
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
  //   // a |primary_time| of 900, and another with a |primary_time| of 1000.
  //   CheckConfigs(
  //     "id1", 900,
  //     "id2", 1000,
  //     NULL);
  //
  // If the server config id starts with "INVALID" then the generated protobuf
  // will be invalid.
  void SetConfigs(const char* server_config_id1, ...) {
    va_list ap;
    va_start(ap, server_config_id1);
    bool has_invalid = false;

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

      int primary_time = va_arg(ap, int);

      QuicCryptoServerConfig::ConfigOptions options;
      options.id = server_config_id;
      QuicServerConfigProtobuf* protobuf(
          QuicCryptoServerConfig::DefaultConfig(rand_, &clock_, options));
      protobuf->set_primary_time(primary_time);
      if (string(server_config_id).find("INVALID") == 0) {
        protobuf->clear_key();
        has_invalid = true;
      }
      protobufs.push_back(protobuf);
    }

    ASSERT_EQ(!has_invalid, config_.SetConfigs(protobufs, clock_.WallNow()));
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
  SetConfigs("a", 1100,
             "b", 900,
             NULL);
  test_peer_.CheckConfigs(
      "a", false,
      "b", true,
      NULL);
}

TEST_F(CryptoServerConfigsTest, MakePrimarySecond) {
  // Make sure that a remains primary after b is added.
  SetConfigs("a", 900,
             "b", 1100,
             NULL);
  test_peer_.CheckConfigs(
      "a", true,
      "b", false,
      NULL);
}

TEST_F(CryptoServerConfigsTest, Delete) {
  // Ensure that configs get deleted when removed.
  SetConfigs("a", 800,
             "b", 900,
             "c", 1100,
             NULL);
  SetConfigs("b", 900,
             "c", 1100,
             NULL);
  test_peer_.CheckConfigs(
      "b", true,
      "c", false,
      NULL);
}

TEST_F(CryptoServerConfigsTest, DontDeletePrimary) {
  // Ensure that the primary config isn't deleted when removed.
  SetConfigs("a", 800,
             "b", 900,
             "c", 1100,
             NULL);
  SetConfigs("a", 800,
             "c", 1100,
             NULL);
  test_peer_.CheckConfigs(
      "a", false,
      "b", true,
      "c", false,
      NULL);
}

TEST_F(CryptoServerConfigsTest, AdvancePrimary) {
  // Check that a new primary config is enabled at the right time.
  SetConfigs("a", 900,
             "b", 1100,
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
  SetConfigs("a", 800,
             "b", 900,
             "c", 1100,
             NULL);
  test_peer_.CheckConfigs(
      "a", false,
      "b", true,
      "c", false,
      NULL);
  SetConfigs("a", 800,
             "c", 1100,
             "INVALID1", 1000,
             NULL);
  test_peer_.CheckConfigs(
      "a", false,
      "b", true,
      "c", false,
      NULL);
}

}  // namespace test
}  // namespace net
