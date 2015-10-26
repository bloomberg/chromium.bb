// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_impl.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address_number.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class ListValue;
}

namespace net {

const int kMaxSupportsSpdyServerHosts = 500;

class HttpServerPropertiesImplPeer {
 public:
  static void AddBrokenAlternativeServiceWithExpirationTime(
      HttpServerPropertiesImpl& impl,
      AlternativeService alternative_service,
      base::TimeTicks when) {
    impl.broken_alternative_services_.insert(
        std::make_pair(alternative_service, when));
    ++impl.recently_broken_alternative_services_[alternative_service];
  }

  static void ExpireBrokenAlternateProtocolMappings(
      HttpServerPropertiesImpl& impl) {
    impl.ExpireBrokenAlternateProtocolMappings();
  }
};

void PrintTo(const AlternativeService& alternative_service, std::ostream* os) {
  *os << alternative_service.ToString();
}

namespace {

class HttpServerPropertiesImplTest : public testing::Test {
 protected:
  bool HasAlternativeService(const HostPortPair& origin) {
    const AlternativeServiceVector alternative_service_vector =
        impl_.GetAlternativeServices(origin);
    return !alternative_service_vector.empty();
  }

  bool SetAlternativeService(const HostPortPair& origin,
                             const AlternativeService& alternative_service,
                             double alternative_probability) {
    const base::Time expiration =
        base::Time::Now() + base::TimeDelta::FromDays(1);
    return impl_.SetAlternativeService(origin, alternative_service,
                                       alternative_probability, expiration);
  }

  HttpServerPropertiesImpl impl_;
};

typedef HttpServerPropertiesImplTest SpdyServerPropertiesTest;

TEST_F(SpdyServerPropertiesTest, Initialize) {
  HostPortPair spdy_server_google("www.google.com", 443);
  std::string spdy_server_g = spdy_server_google.ToString();

  HostPortPair spdy_server_docs("docs.google.com", 443);
  std::string spdy_server_d = spdy_server_docs.ToString();

  // Check by initializing NULL spdy servers.
  impl_.InitializeSpdyServers(NULL, true);
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_google));

  // Check by initializing empty spdy servers.
  std::vector<std::string> spdy_servers;
  impl_.InitializeSpdyServers(&spdy_servers, true);
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_google));

  // Check by initializing with www.google.com:443 spdy server.
  std::vector<std::string> spdy_servers1;
  spdy_servers1.push_back(spdy_server_g);
  impl_.InitializeSpdyServers(&spdy_servers1, true);
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));

  // Check by initializing with www.google.com:443 and docs.google.com:443 spdy
  // servers.
  std::vector<std::string> spdy_servers2;
  spdy_servers2.push_back(spdy_server_g);
  spdy_servers2.push_back(spdy_server_d);
  impl_.InitializeSpdyServers(&spdy_servers2, true);

  // Verify spdy_server_g and spdy_server_d are in the list in the same order.
  base::ListValue spdy_server_list;
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  EXPECT_EQ(2U, spdy_server_list.GetSize());
  std::string string_value_g;
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);
  std::string string_value_d;
  ASSERT_TRUE(spdy_server_list.GetString(1, &string_value_d));
  ASSERT_EQ(spdy_server_d, string_value_d);
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_docs));
}

TEST_F(SpdyServerPropertiesTest, SupportsRequestPriorityTest) {
  HostPortPair spdy_server_empty(std::string(), 443);
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_empty));

  // Add www.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_google("www.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_google, true);
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));

  // Add mail.google.com:443 as not supporting SPDY.
  HostPortPair spdy_server_mail("mail.google.com", 443);
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_mail));

  // Add docs.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_docs("docs.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_docs, true);
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_docs));

  // Add www.youtube.com:443 as supporting QUIC.
  HostPortPair quic_server_youtube("www.youtube.com", 443);
  const AlternativeService alternative_service1(QUIC, "www.youtube.com", 443);
  SetAlternativeService(quic_server_youtube, alternative_service1, 1.0);
  EXPECT_TRUE(impl_.SupportsRequestPriority(quic_server_youtube));

  // Add www.example.com:443 with two alternative services, one supporting QUIC.
  HostPortPair quic_server_example("www.example.com", 443);
  const AlternativeService alternative_service2(NPN_HTTP_2, "", 443);
  SetAlternativeService(quic_server_example, alternative_service2, 1.0);
  SetAlternativeService(quic_server_example, alternative_service1, 1.0);
  EXPECT_TRUE(impl_.SupportsRequestPriority(quic_server_example));

  // Verify all the entries are the same after additions.
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_mail));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_docs));
  EXPECT_TRUE(impl_.SupportsRequestPriority(quic_server_youtube));
  EXPECT_TRUE(impl_.SupportsRequestPriority(quic_server_example));
}

TEST_F(SpdyServerPropertiesTest, Clear) {
  // Add www.google.com:443 and mail.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_google("www.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_google, true);
  HostPortPair spdy_server_mail("mail.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_mail, true);

  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_mail));

  impl_.Clear();
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_mail));
}

TEST_F(SpdyServerPropertiesTest, GetSpdyServerList) {
  base::ListValue spdy_server_list;

  // Check there are no spdy_servers.
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  EXPECT_EQ(0U, spdy_server_list.GetSize());

  // Check empty server is not added.
  HostPortPair spdy_server_empty(std::string(), 443);
  impl_.SetSupportsSpdy(spdy_server_empty, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  EXPECT_EQ(0U, spdy_server_list.GetSize());

  std::string string_value_g;
  std::string string_value_m;
  HostPortPair spdy_server_google("www.google.com", 443);
  std::string spdy_server_g = spdy_server_google.ToString();
  HostPortPair spdy_server_mail("mail.google.com", 443);
  std::string spdy_server_m = spdy_server_mail.ToString();

  // Add www.google.com:443 as not supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, false);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  EXPECT_EQ(0U, spdy_server_list.GetSize());

  // Add www.google.com:443 as supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(1U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Add mail.google.com:443 as not supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_mail, false);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(1U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Add mail.google.com:443 as supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_mail, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(2U, spdy_server_list.GetSize());

  // Verify www.google.com:443 and mail.google.com:443 are in the list.
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_m));
  ASSERT_EQ(spdy_server_m, string_value_m);
  ASSERT_TRUE(spdy_server_list.GetString(1, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Request for only one server and verify that we get only one server.
  impl_.GetSpdyServerList(&spdy_server_list, 1);
  ASSERT_EQ(1U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_m));
  ASSERT_EQ(spdy_server_m, string_value_m);
}

TEST_F(SpdyServerPropertiesTest, MRUOfGetSpdyServerList) {
  base::ListValue spdy_server_list;

  std::string string_value_g;
  std::string string_value_m;
  HostPortPair spdy_server_google("www.google.com", 443);
  std::string spdy_server_g = spdy_server_google.ToString();
  HostPortPair spdy_server_mail("mail.google.com", 443);
  std::string spdy_server_m = spdy_server_mail.ToString();

  // Add www.google.com:443 as supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(1U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Add mail.google.com:443 as supporting SPDY. Verify mail.google.com:443 and
  // www.google.com:443 are in the list.
  impl_.SetSupportsSpdy(spdy_server_mail, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(2U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_m));
  ASSERT_EQ(spdy_server_m, string_value_m);
  ASSERT_TRUE(spdy_server_list.GetString(1, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Get www.google.com:443 should reorder SpdyServerHostPortMap. Verify that it
  // is www.google.com:443 is the MRU server.
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(2U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);
  ASSERT_TRUE(spdy_server_list.GetString(1, &string_value_m));
  ASSERT_EQ(spdy_server_m, string_value_m);
}

typedef HttpServerPropertiesImplTest AlternateProtocolServerPropertiesTest;

TEST_F(AlternateProtocolServerPropertiesTest, Basic) {
  HostPortPair test_host_port_pair("foo", 80);
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));

  AlternativeService alternative_service(NPN_HTTP_2, "foo", 443);
  SetAlternativeService(test_host_port_pair, alternative_service, 1.0);
  const AlternativeServiceVector alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(1u, alternative_service_vector.size());
  EXPECT_EQ(alternative_service, alternative_service_vector[0]);

  impl_.Clear();
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, ExcludeOrigin) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  // Same hostname, same port, TCP: should be ignored.
  AlternativeService alternative_service1(NPN_HTTP_2, "foo", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service1, 1.0, expiration));
  // Different hostname: GetAlternativeServices should return this one.
  AlternativeService alternative_service2(NPN_HTTP_2, "bar", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service2, 1.0, expiration));
  // Different port: GetAlternativeServices should return this one too.
  AlternativeService alternative_service3(NPN_HTTP_2, "foo", 80);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service3, 1.0, expiration));
  // QUIC: GetAlternativeServices should return this one too.
  AlternativeService alternative_service4(QUIC, "foo", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service4, 1.0, expiration));

  HostPortPair test_host_port_pair("foo", 443);
  impl_.SetAlternativeServices(test_host_port_pair,
                               alternative_service_info_vector);

  const AlternativeServiceVector alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(3u, alternative_service_vector.size());
  EXPECT_EQ(alternative_service2, alternative_service_vector[0]);
  EXPECT_EQ(alternative_service3, alternative_service_vector[1]);
  EXPECT_EQ(alternative_service4, alternative_service_vector[2]);
}

TEST_F(AlternateProtocolServerPropertiesTest, DefaultProbabilityExcluded) {
  HostPortPair test_host_port_pair("foo", 80);
  const AlternativeService alternative_service(NPN_HTTP_2, "foo", 443);
  SetAlternativeService(test_host_port_pair, alternative_service, 0.99);

  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

// GetAlternativeServices and HasAlternativeServices should only return the ones
// with probability greater than or equal to the threshold.
TEST_F(AlternateProtocolServerPropertiesTest, Probability) {
  impl_.SetAlternativeServiceProbabilityThreshold(0.5);

  AlternativeServiceInfoVector alternative_service_info_vector;
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  const AlternativeService alternative_service1(NPN_HTTP_2, "foo", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service1, 0.3, expiration));
  const AlternativeService alternative_service2(QUIC, "bar", 123);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service2, 0.7, expiration));
  const AlternativeService alternative_service3(NPN_SPDY_3_1, "baz", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service3, 0.4, expiration));
  const AlternativeService alternative_service4(NPN_HTTP_2, "qux", 1234);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service4, 0.6, expiration));

  HostPortPair test_host_port_pair("foo", 80);
  impl_.SetAlternativeServices(test_host_port_pair,
                               alternative_service_info_vector);

  const AlternativeServiceVector alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(2u, alternative_service_vector.size());
  EXPECT_EQ(alternative_service2, alternative_service_vector[0]);
  EXPECT_EQ(alternative_service4, alternative_service_vector[1]);
}

TEST_F(AlternateProtocolServerPropertiesTest, ProbabilityExcluded) {
  impl_.SetAlternativeServiceProbabilityThreshold(0.75);

  HostPortPair test_host_port_pair("foo", 80);
  const AlternativeService alternative_service(NPN_HTTP_2, "foo", 443);
  SetAlternativeService(test_host_port_pair, alternative_service, 0.5);
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, Initialize) {
  // |test_host_port_pair1| has one alternative service, which is non-broken,
  // and thus will be removed by InitializeAlternativeServiceServers().
  HostPortPair test_host_port_pair1("foo1", 80);
  const AlternativeService alternative_service1(NPN_HTTP_2, "bar1", 443);
  const base::Time now = base::Time::Now();
  base::Time expiration1 = now + base::TimeDelta::FromDays(1);
  impl_.SetAlternativeService(test_host_port_pair1, alternative_service1, 1.0,
                              expiration1);

  // |test_host_port_pair2| has two alternative services.  The broken one will
  // remain, the non-broken one will be removed by
  // InitializeAlternativeServiceServers().
  AlternativeServiceInfoVector alternative_service_info_vector;
  const AlternativeService alternative_service2(NPN_SPDY_3_1, "bar2", 443);
  base::Time expiration2 = now + base::TimeDelta::FromDays(2);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service2, 1.0, expiration2));
  const AlternativeService alternative_service3(NPN_SPDY_3_1, "bar3", 1234);
  base::Time expiration3 = now + base::TimeDelta::FromDays(3);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service3, 0.8, expiration3));
  HostPortPair test_host_port_pair2("foo2", 80);
  impl_.SetAlternativeServices(test_host_port_pair2,
                               alternative_service_info_vector);
  impl_.MarkAlternativeServiceBroken(alternative_service2);

  // Prepare |alternative_service_map| to be loaded by
  // InitializeAlternativeServiceServers().
  AlternativeServiceMap alternative_service_map(
      AlternativeServiceMap::NO_AUTO_EVICT);
  const AlternativeService alternative_service4(NPN_HTTP_2, "bar4", 123);
  base::Time expiration4 = now + base::TimeDelta::FromDays(4);
  const AlternativeServiceInfo alternative_service_info1(alternative_service4,
                                                         0.7, expiration4);
  alternative_service_map.Put(
      test_host_port_pair2,
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info1));

  HostPortPair test_host_port_pair3("foo3", 80);
  const AlternativeService alternative_service5(NPN_HTTP_2, "bar5", 1234);
  base::Time expiration5 = now + base::TimeDelta::FromDays(5);
  const AlternativeServiceInfo alternative_service_info2(alternative_service5,
                                                         0.2, expiration5);
  alternative_service_map.Put(
      test_host_port_pair3,
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info2));

  impl_.InitializeAlternativeServiceServers(&alternative_service_map);

  // Verify alternative_service_map.
  const AlternativeServiceMap& map = impl_.alternative_service_map();
  ASSERT_EQ(2u, map.size());
  AlternativeServiceMap::const_iterator map_it = map.begin();
  EXPECT_TRUE(map_it->first.Equals(test_host_port_pair3));
  ASSERT_EQ(1u, map_it->second.size());
  EXPECT_EQ(alternative_service5, map_it->second[0].alternative_service);
  EXPECT_EQ(0.2, map_it->second[0].probability);
  EXPECT_EQ(expiration5, map_it->second[0].expiration);
  ++map_it;
  EXPECT_TRUE(map_it->first.Equals(test_host_port_pair2));
  ASSERT_EQ(2u, map_it->second.size());
  EXPECT_EQ(alternative_service2, map_it->second[0].alternative_service);
  EXPECT_EQ(1.0, map_it->second[0].probability);
  EXPECT_EQ(expiration2, map_it->second[0].expiration);
  EXPECT_EQ(alternative_service4, map_it->second[1].alternative_service);
  EXPECT_EQ(0.7, map_it->second[1].probability);
  EXPECT_EQ(expiration4, map_it->second[1].expiration);
}

// Regression test for https://crbug.com/504032:
// InitializeAlternativeServiceServers() should not crash if there is an empty
// hostname is the mapping.
TEST_F(AlternateProtocolServerPropertiesTest, InitializeWithEmptyHostname) {
  const HostPortPair host_port_pair("foo", 443);
  const AlternativeService alternative_service_with_empty_hostname(NPN_HTTP_2,
                                                                   "", 1234);
  const AlternativeService alternative_service_with_foo_hostname(NPN_HTTP_2,
                                                                 "foo", 1234);
  SetAlternativeService(host_port_pair, alternative_service_with_empty_hostname,
                        1.0);
  impl_.MarkAlternativeServiceBroken(alternative_service_with_foo_hostname);

  AlternativeServiceMap alternative_service_map(
      AlternativeServiceMap::NO_AUTO_EVICT);
  impl_.InitializeAlternativeServiceServers(&alternative_service_map);

  EXPECT_TRUE(
      impl_.IsAlternativeServiceBroken(alternative_service_with_foo_hostname));
  const AlternativeServiceVector alternative_service_vector =
      impl_.GetAlternativeServices(host_port_pair);
  ASSERT_EQ(1u, alternative_service_vector.size());
  EXPECT_EQ(alternative_service_with_foo_hostname,
            alternative_service_vector[0]);
}

// Regression test for https://crbug.com/516486:
// GetAlternativeServices() should remove |alternative_service_map_| elements
// with empty value.
TEST_F(AlternateProtocolServerPropertiesTest, EmptyVector) {
  HostPortPair host_port_pair("foo", 443);
  const AlternativeService alternative_service(NPN_HTTP_2, "bar", 443);
  base::Time expiration = base::Time::Now() - base::TimeDelta::FromDays(1);
  const AlternativeServiceInfo alternative_service_info(alternative_service,
                                                        1.0, expiration);
  AlternativeServiceMap alternative_service_map(
      AlternativeServiceMap::NO_AUTO_EVICT);
  alternative_service_map.Put(
      host_port_pair,
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info));

  // Prepare |alternative_service_map_| with a single key that has a single
  // AlternativeServiceInfo with identical hostname and port.
  impl_.InitializeAlternativeServiceServers(&alternative_service_map);

  // GetAlternativeServices() should remove such AlternativeServiceInfo from
  // |alternative_service_map_|, emptying the AlternativeServiceInfoVector
  // corresponding to |host_port_pair|.
  AlternativeServiceVector alternative_service_vector =
      impl_.GetAlternativeServices(host_port_pair);
  ASSERT_TRUE(alternative_service_vector.empty());

  // GetAlternativeServices() should remove this key from
  // |alternative_service_map_|, and SetAlternativeServices() should not crash.
  impl_.SetAlternativeServices(
      host_port_pair,
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info));

  // There should still be no alternative service assigned to |host_port_pair|.
  alternative_service_vector = impl_.GetAlternativeServices(host_port_pair);
  ASSERT_TRUE(alternative_service_vector.empty());
}

// Regression test for https://crbug.com/516486 for the canonical host case.
TEST_F(AlternateProtocolServerPropertiesTest, EmptyVectorForCanonical) {
  HostPortPair host_port_pair("foo.c.youtube.com", 443);
  HostPortPair canonical_host_port_pair("bar.c.youtube.com", 443);
  const AlternativeService alternative_service(NPN_HTTP_2, "", 443);
  base::Time expiration = base::Time::Now() - base::TimeDelta::FromDays(1);
  const AlternativeServiceInfo alternative_service_info(alternative_service,
                                                        1.0, expiration);
  AlternativeServiceMap alternative_service_map(
      AlternativeServiceMap::NO_AUTO_EVICT);
  alternative_service_map.Put(
      canonical_host_port_pair,
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info));

  // Prepare |alternative_service_map_| with a single key that has a single
  // AlternativeServiceInfo with identical hostname and port.
  impl_.InitializeAlternativeServiceServers(&alternative_service_map);

  // GetAlternativeServices() should remove such AlternativeServiceInfo from
  // |alternative_service_map_|, emptying the AlternativeServiceInfoVector
  // corresponding to |canonical_host_port_pair|, even when looking up
  // alternative services for |host_port_pair|.
  AlternativeServiceVector alternative_service_vector =
      impl_.GetAlternativeServices(host_port_pair);
  ASSERT_TRUE(alternative_service_vector.empty());

  // GetAlternativeServices() should remove this key from
  // |alternative_service_map_|, and SetAlternativeServices() should not crash.
  impl_.SetAlternativeServices(
      canonical_host_port_pair,
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info));

  // There should still be no alternative service assigned to
  // |canonical_host_port_pair|.
  alternative_service_vector =
      impl_.GetAlternativeServices(canonical_host_port_pair);
  ASSERT_TRUE(alternative_service_vector.empty());
}

TEST_F(AlternateProtocolServerPropertiesTest, MRUOfGetAlternativeServices) {
  HostPortPair test_host_port_pair1("foo1", 80);
  const AlternativeService alternative_service1(NPN_SPDY_3_1, "foo1", 443);
  SetAlternativeService(test_host_port_pair1, alternative_service1, 1.0);
  HostPortPair test_host_port_pair2("foo2", 80);
  const AlternativeService alternative_service2(NPN_HTTP_2, "foo2", 1234);
  SetAlternativeService(test_host_port_pair2, alternative_service2, 1.0);

  const AlternativeServiceMap& map = impl_.alternative_service_map();
  AlternativeServiceMap::const_iterator it = map.begin();
  EXPECT_TRUE(it->first.Equals(test_host_port_pair2));
  ASSERT_EQ(1u, it->second.size());
  EXPECT_EQ(alternative_service2, it->second[0].alternative_service);

  const AlternativeServiceVector alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair1);
  ASSERT_EQ(1u, alternative_service_vector.size());
  EXPECT_EQ(alternative_service1, alternative_service_vector[0]);

  // GetAlternativeServices should reorder the AlternateProtocol map.
  it = map.begin();
  EXPECT_TRUE(it->first.Equals(test_host_port_pair1));
  ASSERT_EQ(1u, it->second.size());
  EXPECT_EQ(alternative_service1, it->second[0].alternative_service);
}

TEST_F(AlternateProtocolServerPropertiesTest, SetBroken) {
  HostPortPair test_host_port_pair("foo", 80);
  const AlternativeService alternative_service1(NPN_HTTP_2, "foo", 443);
  SetAlternativeService(test_host_port_pair, alternative_service1, 1.0);
  AlternativeServiceVector alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(1u, alternative_service_vector.size());
  EXPECT_EQ(alternative_service1, alternative_service_vector[0]);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service1));

  // GetAlternativeServices should return the broken alternative service.
  impl_.MarkAlternativeServiceBroken(alternative_service1);
  alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(1u, alternative_service_vector.size());
  EXPECT_EQ(alternative_service1, alternative_service_vector[0]);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1));

  // SetAlternativeServices should add a broken alternative service to the map.
  AlternativeServiceInfoVector alternative_service_info_vector;
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service1, 1.0, expiration));
  const AlternativeService alternative_service2(NPN_HTTP_2, "foo", 1234);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service2, 1.0, expiration));
  impl_.SetAlternativeServices(test_host_port_pair,
                               alternative_service_info_vector);
  alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(2u, alternative_service_vector.size());
  EXPECT_EQ(alternative_service1, alternative_service_vector[0]);
  EXPECT_EQ(alternative_service2, alternative_service_vector[1]);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service_vector[0]));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service_vector[1]));

  // SetAlternativeService should add a broken alternative service to the map.
  SetAlternativeService(test_host_port_pair, alternative_service1, 1.0);
  alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(1u, alternative_service_vector.size());
  EXPECT_EQ(alternative_service1, alternative_service_vector[0]);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service_vector[0]));
}

TEST_F(AlternateProtocolServerPropertiesTest, MaxAge) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  base::Time now = base::Time::Now();
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);

  // First alternative service expired one day ago, should not be returned by
  // GetAlternativeServices().
  const AlternativeService alternative_service1(NPN_SPDY_3_1, "foo", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service1, 1.0, now - one_day));

  // Second alterrnative service will expire one day from now, should be
  // returned by GetAlternativeSerices().
  const AlternativeService alternative_service2(NPN_HTTP_2, "bar", 1234);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service2, 1.0, now + one_day));

  HostPortPair test_host_port_pair("foo", 80);
  impl_.SetAlternativeServices(test_host_port_pair,
                               alternative_service_info_vector);

  AlternativeServiceVector alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(1u, alternative_service_vector.size());
  EXPECT_EQ(alternative_service2, alternative_service_vector[0]);
}

TEST_F(AlternateProtocolServerPropertiesTest, MaxAgeCanonical) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  base::Time now = base::Time::Now();
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);

  // First alternative service expired one day ago, should not be returned by
  // GetAlternativeServices().
  const AlternativeService alternative_service1(NPN_SPDY_3_1, "foo", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service1, 1.0, now - one_day));

  // Second alterrnative service will expire one day from now, should be
  // returned by GetAlternativeSerices().
  const AlternativeService alternative_service2(NPN_HTTP_2, "bar", 1234);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service2, 1.0, now + one_day));

  HostPortPair canonical_host_port_pair("bar.c.youtube.com", 80);
  impl_.SetAlternativeServices(canonical_host_port_pair,
                               alternative_service_info_vector);

  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  AlternativeServiceVector alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(1u, alternative_service_vector.size());
  EXPECT_EQ(alternative_service2, alternative_service_vector[0]);
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearAlternativeServices) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  const AlternativeService alternative_service1(NPN_SPDY_3_1, "foo", 443);
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service1, 1.0, expiration));
  const AlternativeService alternative_service2(NPN_HTTP_2, "bar", 1234);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(alternative_service2, 1.0, expiration));
  HostPortPair test_host_port_pair("foo", 80);
  impl_.SetAlternativeServices(test_host_port_pair,
                               alternative_service_info_vector);

  const net::AlternativeServiceMap& map = impl_.alternative_service_map();
  net::AlternativeServiceMap::const_iterator it = map.begin();
  EXPECT_TRUE(it->first.Equals(test_host_port_pair));
  ASSERT_EQ(2u, it->second.size());
  EXPECT_EQ(alternative_service1, it->second[0].alternative_service);
  EXPECT_EQ(alternative_service2, it->second[1].alternative_service);

  impl_.ClearAlternativeServices(test_host_port_pair);
  EXPECT_TRUE(map.empty());
}

// A broken alternative service in the mapping carries meaningful information,
// therefore it should not be ignored by SetAlternativeService().  In
// particular, an alternative service mapped to an origin shadows alternative
// services of canonical hosts.
TEST_F(AlternateProtocolServerPropertiesTest, BrokenShadowsCanonical) {
  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  HostPortPair canonical_host_port_pair("bar.c.youtube.com", 80);
  AlternativeService canonical_alternative_service(QUIC, "bar.c.youtube.com",
                                                   1234);
  SetAlternativeService(canonical_host_port_pair, canonical_alternative_service,
                        1.0);
  AlternativeServiceVector alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(1u, alternative_service_vector.size());
  EXPECT_EQ(canonical_alternative_service, alternative_service_vector[0]);

  const AlternativeService broken_alternative_service(NPN_HTTP_2, "foo", 443);
  impl_.MarkAlternativeServiceBroken(broken_alternative_service);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(broken_alternative_service));

  SetAlternativeService(test_host_port_pair, broken_alternative_service, 1.0);
  alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(1u, alternative_service_vector.size());
  EXPECT_EQ(broken_alternative_service, alternative_service_vector[0]);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(broken_alternative_service));
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearBroken) {
  HostPortPair test_host_port_pair("foo", 80);
  const AlternativeService alternative_service(NPN_HTTP_2, "foo", 443);
  SetAlternativeService(test_host_port_pair, alternative_service, 1.0);
  impl_.MarkAlternativeServiceBroken(alternative_service);
  ASSERT_TRUE(HasAlternativeService(test_host_port_pair));
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
  // ClearAlternativeServices should leave a broken alternative service marked
  // as such.
  impl_.ClearAlternativeServices(test_host_port_pair);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
}

TEST_F(AlternateProtocolServerPropertiesTest, MarkRecentlyBroken) {
  HostPortPair host_port_pair("foo", 80);
  const AlternativeService alternative_service(NPN_HTTP_2, "foo", 443);
  SetAlternativeService(host_port_pair, alternative_service, 1.0);

  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  impl_.MarkAlternativeServiceRecentlyBroken(alternative_service);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  impl_.ConfirmAlternativeService(alternative_service);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));
}

TEST_F(AlternateProtocolServerPropertiesTest, Canonical) {
  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));

  HostPortPair canonical_host_port_pair("bar.c.youtube.com", 80);
  EXPECT_FALSE(HasAlternativeService(canonical_host_port_pair));

  AlternativeServiceInfoVector alternative_service_info_vector;
  const AlternativeService canonical_alternative_service1(
      QUIC, "bar.c.youtube.com", 1234);
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(canonical_alternative_service1, 1.0, expiration));
  const AlternativeService canonical_alternative_service2(NPN_HTTP_2, "", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo(canonical_alternative_service2, 1.0, expiration));
  impl_.SetAlternativeServices(canonical_host_port_pair,
                               alternative_service_info_vector);

  // Since |test_host_port_pair| does not have an alternative service itself,
  // GetAlternativeServices should return those of |canonical_host_port_pair|.
  AlternativeServiceVector alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(2u, alternative_service_vector.size());
  EXPECT_EQ(canonical_alternative_service1, alternative_service_vector[0]);

  // Since |canonical_alternative_service2| has an empty host,
  // GetAlternativeServices should substitute the hostname of its |origin|
  // argument.
  EXPECT_EQ(test_host_port_pair.host(), alternative_service_vector[1].host);
  EXPECT_EQ(canonical_alternative_service2.protocol,
            alternative_service_vector[1].protocol);
  EXPECT_EQ(canonical_alternative_service2.port,
            alternative_service_vector[1].port);

  // Verify the canonical suffix.
  EXPECT_EQ(".c.youtube.com",
            impl_.GetCanonicalSuffix(test_host_port_pair.host()));
  EXPECT_EQ(".c.youtube.com",
            impl_.GetCanonicalSuffix(canonical_host_port_pair.host()));
}

TEST_F(AlternateProtocolServerPropertiesTest, CanonicalBelowThreshold) {
  impl_.SetAlternativeServiceProbabilityThreshold(0.02);

  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  HostPortPair canonical_host_port_pair("bar.c.youtube.com", 80);
  AlternativeService canonical_alternative_service(QUIC, "bar.c.youtube.com",
                                                   1234);

  SetAlternativeService(canonical_host_port_pair, canonical_alternative_service,
                        0.01);
  EXPECT_FALSE(HasAlternativeService(canonical_host_port_pair));
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, CanonicalAboveThreshold) {
  impl_.SetAlternativeServiceProbabilityThreshold(0.02);

  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  HostPortPair canonical_host_port_pair("bar.c.youtube.com", 80);
  AlternativeService canonical_alternative_service(QUIC, "bar.c.youtube.com",
                                                   1234);

  SetAlternativeService(canonical_host_port_pair, canonical_alternative_service,
                        0.03);
  EXPECT_TRUE(HasAlternativeService(canonical_host_port_pair));
  EXPECT_TRUE(HasAlternativeService(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearCanonical) {
  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  HostPortPair canonical_host_port_pair("bar.c.youtube.com", 80);
  AlternativeService canonical_alternative_service(QUIC, "bar.c.youtube.com",
                                                   1234);

  SetAlternativeService(canonical_host_port_pair, canonical_alternative_service,
                        1.0);
  impl_.ClearAlternativeServices(canonical_host_port_pair);
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, CanonicalBroken) {
  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  HostPortPair canonical_host_port_pair("bar.c.youtube.com", 80);
  AlternativeService canonical_alternative_service(QUIC, "bar.c.youtube.com",
                                                   1234);

  SetAlternativeService(canonical_host_port_pair, canonical_alternative_service,
                        1.0);
  impl_.MarkAlternativeServiceBroken(canonical_alternative_service);
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

// Adding an alternative service for a new host overrides canonical host.
TEST_F(AlternateProtocolServerPropertiesTest, CanonicalOverride) {
  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  HostPortPair bar_host_port_pair("bar.c.youtube.com", 80);
  AlternativeService bar_alternative_service(QUIC, "bar.c.youtube.com", 1234);
  SetAlternativeService(bar_host_port_pair, bar_alternative_service, 1.0);
  AlternativeServiceVector alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(1u, alternative_service_vector.size());
  EXPECT_EQ(bar_alternative_service, alternative_service_vector[0]);

  HostPortPair qux_host_port_pair("qux.c.youtube.com", 80);
  AlternativeService qux_alternative_service(QUIC, "qux.c.youtube.com", 443);
  SetAlternativeService(qux_host_port_pair, qux_alternative_service, 1.0);
  alternative_service_vector =
      impl_.GetAlternativeServices(test_host_port_pair);
  ASSERT_EQ(1u, alternative_service_vector.size());
  EXPECT_EQ(qux_alternative_service, alternative_service_vector[0]);
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearWithCanonical) {
  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  HostPortPair canonical_host_port_pair("bar.c.youtube.com", 80);
  AlternativeService canonical_alternative_service(QUIC, "bar.c.youtube.com",
                                                   1234);

  SetAlternativeService(canonical_host_port_pair, canonical_alternative_service,
                        1.0);
  impl_.Clear();
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       ExpireBrokenAlternateProtocolMappings) {
  HostPortPair host_port_pair("foo", 443);
  AlternativeService alternative_service(QUIC, "foo", 443);
  SetAlternativeService(host_port_pair, alternative_service, 1.0);
  EXPECT_TRUE(HasAlternativeService(host_port_pair));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  base::TimeTicks past =
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(42);
  HttpServerPropertiesImplPeer::AddBrokenAlternativeServiceWithExpirationTime(
      impl_, alternative_service, past);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  HttpServerPropertiesImplPeer::ExpireBrokenAlternateProtocolMappings(impl_);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));
}

// Regression test for https://crbug.com/505413.
TEST_F(AlternateProtocolServerPropertiesTest, RemoveExpiredBrokenAltSvc) {
  HostPortPair foo_host_port_pair("foo", 443);
  AlternativeService bar_alternative_service(QUIC, "bar", 443);
  SetAlternativeService(foo_host_port_pair, bar_alternative_service, 1.0);
  EXPECT_TRUE(HasAlternativeService(foo_host_port_pair));

  HostPortPair bar_host_port_pair1("bar", 80);
  AlternativeService nohost_alternative_service(QUIC, "", 443);
  SetAlternativeService(bar_host_port_pair1, nohost_alternative_service, 1.0);
  EXPECT_TRUE(HasAlternativeService(bar_host_port_pair1));

  HostPortPair bar_host_port_pair2("bar", 443);
  AlternativeService baz_alternative_service(QUIC, "baz", 1234);
  SetAlternativeService(bar_host_port_pair2, baz_alternative_service, 1.0);
  EXPECT_TRUE(HasAlternativeService(bar_host_port_pair2));

  // Mark "bar:443" as broken.
  base::TimeTicks past =
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(42);
  HttpServerPropertiesImplPeer::AddBrokenAlternativeServiceWithExpirationTime(
      impl_, bar_alternative_service, past);

  // Expire brokenness of "bar:443".
  HttpServerPropertiesImplPeer::ExpireBrokenAlternateProtocolMappings(impl_);

  // "foo:443" should have no alternative service now.
  EXPECT_FALSE(HasAlternativeService(foo_host_port_pair));
  // "bar:80" should have no alternative service now.
  EXPECT_FALSE(HasAlternativeService(bar_host_port_pair1));
  // The alternative service of "bar:443" should be unaffected.
  EXPECT_TRUE(HasAlternativeService(bar_host_port_pair2));

  EXPECT_TRUE(
      impl_.WasAlternativeServiceRecentlyBroken(bar_alternative_service));
  EXPECT_FALSE(
      impl_.WasAlternativeServiceRecentlyBroken(baz_alternative_service));
}

typedef HttpServerPropertiesImplTest SpdySettingsServerPropertiesTest;

TEST_F(SpdySettingsServerPropertiesTest, Initialize) {
  HostPortPair spdy_server_google("www.google.com", 443);

  // Check by initializing empty spdy settings.
  SpdySettingsMap spdy_settings_map(SpdySettingsMap::NO_AUTO_EVICT);
  impl_.InitializeSpdySettingsServers(&spdy_settings_map);
  EXPECT_TRUE(impl_.GetSpdySettings(spdy_server_google).empty());

  // Check by initializing with www.google.com:443 spdy server settings.
  SettingsMap settings_map;
  const SpdySettingsIds id = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags = SETTINGS_FLAG_PERSISTED;
  const uint32 value = 31337;
  SettingsFlagsAndValue flags_and_value(flags, value);
  settings_map[id] = flags_and_value;
  spdy_settings_map.Put(spdy_server_google, settings_map);
  impl_.InitializeSpdySettingsServers(&spdy_settings_map);

  const SettingsMap& settings_map2 = impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map2.size());
  SettingsMap::const_iterator it = settings_map2.find(id);
  EXPECT_TRUE(it != settings_map2.end());
  SettingsFlagsAndValue flags_and_value2 = it->second;
  EXPECT_EQ(flags, flags_and_value2.first);
  EXPECT_EQ(value, flags_and_value2.second);
}

TEST_F(SpdySettingsServerPropertiesTest, SetSpdySetting) {
  HostPortPair spdy_server_empty(std::string(), 443);
  const SettingsMap& settings_map0 = impl_.GetSpdySettings(spdy_server_empty);
  EXPECT_EQ(0U, settings_map0.size());  // Returns kEmptySettingsMap.

  // Add www.google.com:443 as persisting.
  HostPortPair spdy_server_google("www.google.com", 443);
  const SpdySettingsIds id1 = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags1 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_google, id1, flags1, value1));
  // Check the values.
  const SettingsMap& settings_map1_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map1_ret.size());
  SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Add mail.google.com:443 as not persisting.
  HostPortPair spdy_server_mail("mail.google.com", 443);
  const SpdySettingsIds id2 = SETTINGS_DOWNLOAD_BANDWIDTH;
  const SpdySettingsFlags flags2 = SETTINGS_FLAG_NONE;
  const uint32 value2 = 62667;
  EXPECT_FALSE(impl_.SetSpdySetting(spdy_server_mail, id2, flags2, value2));
  const SettingsMap& settings_map2_ret =
      impl_.GetSpdySettings(spdy_server_mail);
  EXPECT_EQ(0U, settings_map2_ret.size());  // Returns kEmptySettingsMap.

  // Add docs.google.com:443 as persisting
  HostPortPair spdy_server_docs("docs.google.com", 443);
  const SpdySettingsIds id3 = SETTINGS_ROUND_TRIP_TIME;
  const SpdySettingsFlags flags3 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value3 = 93997;
  SettingsFlagsAndValue flags_and_value3(flags3, value3);
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_docs, id3, flags3, value3));
  // Check the values.
  const SettingsMap& settings_map3_ret =
      impl_.GetSpdySettings(spdy_server_docs);
  ASSERT_EQ(1U, settings_map3_ret.size());
  SettingsMap::const_iterator it3_ret = settings_map3_ret.find(id3);
  EXPECT_TRUE(it3_ret != settings_map3_ret.end());
  SettingsFlagsAndValue flags_and_value3_ret = it3_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value3_ret.first);
  EXPECT_EQ(value3, flags_and_value3_ret.second);

  // Check data for www.google.com:443 (id1).
  const SettingsMap& settings_map4_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map4_ret.size());
  SettingsMap::const_iterator it4_ret = settings_map4_ret.find(id1);
  EXPECT_TRUE(it4_ret != settings_map4_ret.end());
  SettingsFlagsAndValue flags_and_value4_ret = it4_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value4_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Clear www.google.com:443 as persisting.
  impl_.ClearSpdySettings(spdy_server_google);
  // Check the values.
  const SettingsMap& settings_map5_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(0U, settings_map5_ret.size());

  // Clear all settings.
  ASSERT_GT(impl_.spdy_settings_map().size(), 0U);
  impl_.ClearAllSpdySettings();
  ASSERT_EQ(0U, impl_.spdy_settings_map().size());
}

TEST_F(SpdySettingsServerPropertiesTest, Clear) {
  // Add www.google.com:443 as persisting.
  HostPortPair spdy_server_google("www.google.com", 443);
  const SpdySettingsIds id1 = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags1 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_google, id1, flags1, value1));
  // Check the values.
  const SettingsMap& settings_map1_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map1_ret.size());
  SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Add docs.google.com:443 as persisting
  HostPortPair spdy_server_docs("docs.google.com", 443);
  const SpdySettingsIds id3 = SETTINGS_ROUND_TRIP_TIME;
  const SpdySettingsFlags flags3 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value3 = 93997;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_docs, id3, flags3, value3));
  // Check the values.
  const SettingsMap& settings_map3_ret =
      impl_.GetSpdySettings(spdy_server_docs);
  ASSERT_EQ(1U, settings_map3_ret.size());
  SettingsMap::const_iterator it3_ret = settings_map3_ret.find(id3);
  EXPECT_TRUE(it3_ret != settings_map3_ret.end());
  SettingsFlagsAndValue flags_and_value3_ret = it3_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value3_ret.first);
  EXPECT_EQ(value3, flags_and_value3_ret.second);

  impl_.Clear();
  EXPECT_EQ(0U, impl_.GetSpdySettings(spdy_server_google).size());
  EXPECT_EQ(0U, impl_.GetSpdySettings(spdy_server_docs).size());
}

TEST_F(SpdySettingsServerPropertiesTest, MRUOfGetSpdySettings) {
  // Add www.google.com:443 as persisting.
  HostPortPair spdy_server_google("www.google.com", 443);
  const SpdySettingsIds id1 = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags1 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_google, id1, flags1, value1));

  // Add docs.google.com:443 as persisting
  HostPortPair spdy_server_docs("docs.google.com", 443);
  const SpdySettingsIds id2 = SETTINGS_ROUND_TRIP_TIME;
  const SpdySettingsFlags flags2 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value2 = 93997;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_docs, id2, flags2, value2));

  // Verify the first element is docs.google.com:443.
  const SpdySettingsMap& map = impl_.spdy_settings_map();
  SpdySettingsMap::const_iterator it = map.begin();
  EXPECT_TRUE(it->first.Equals(spdy_server_docs));
  const SettingsMap& settings_map2_ret = it->second;
  ASSERT_EQ(1U, settings_map2_ret.size());
  SettingsMap::const_iterator it2_ret = settings_map2_ret.find(id2);
  EXPECT_TRUE(it2_ret != settings_map2_ret.end());
  SettingsFlagsAndValue flags_and_value2_ret = it2_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value2_ret.first);
  EXPECT_EQ(value2, flags_and_value2_ret.second);

  // GetSpdySettings should reorder the SpdySettingsMap.
  const SettingsMap& settings_map1_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map1_ret.size());
  SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Check the first entry is spdy_server_google by accessing it via iterator.
  it = map.begin();
  EXPECT_TRUE(it->first.Equals(spdy_server_google));
  const SettingsMap& settings_map1_it_ret = it->second;
  ASSERT_EQ(1U, settings_map1_it_ret.size());
  it1_ret = settings_map1_it_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_it_ret.end());
  flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);
}

typedef HttpServerPropertiesImplTest SupportsQuicServerPropertiesTest;

TEST_F(SupportsQuicServerPropertiesTest, Initialize) {
  HostPortPair quic_server_google("www.google.com", 443);

  // Check by initializing empty address.
  IPAddressNumber initial_address;
  impl_.InitializeSupportsQuic(&initial_address);

  IPAddressNumber address;
  EXPECT_FALSE(impl_.GetSupportsQuic(&address));
  EXPECT_TRUE(address.empty());

  // Check by initializing with a valid address.
  CHECK(ParseIPLiteralToNumber("127.0.0.1", &initial_address));
  impl_.InitializeSupportsQuic(&initial_address);

  EXPECT_TRUE(impl_.GetSupportsQuic(&address));
  EXPECT_EQ(initial_address, address);
}

TEST_F(SupportsQuicServerPropertiesTest, SetSupportsQuic) {
  IPAddressNumber address;
  EXPECT_FALSE(impl_.GetSupportsQuic(&address));
  EXPECT_TRUE(address.empty());

  IPAddressNumber actual_address;
  CHECK(ParseIPLiteralToNumber("127.0.0.1", &actual_address));
  impl_.SetSupportsQuic(true, actual_address);

  EXPECT_TRUE(impl_.GetSupportsQuic(&address));
  EXPECT_EQ(actual_address, address);

  impl_.Clear();

  EXPECT_FALSE(impl_.GetSupportsQuic(&address));
}

typedef HttpServerPropertiesImplTest ServerNetworkStatsServerPropertiesTest;

TEST_F(ServerNetworkStatsServerPropertiesTest, Initialize) {
  HostPortPair google_server("www.google.com", 443);

  // Check by initializing empty ServerNetworkStats.
  ServerNetworkStatsMap server_network_stats_map(
      ServerNetworkStatsMap::NO_AUTO_EVICT);
  impl_.InitializeServerNetworkStats(&server_network_stats_map);
  const ServerNetworkStats* stats = impl_.GetServerNetworkStats(google_server);
  EXPECT_EQ(NULL, stats);

  // Check by initializing with www.google.com:443.
  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(10);
  stats1.bandwidth_estimate = QuicBandwidth::FromBitsPerSecond(100);
  server_network_stats_map.Put(google_server, stats1);
  impl_.InitializeServerNetworkStats(&server_network_stats_map);

  const ServerNetworkStats* stats2 = impl_.GetServerNetworkStats(google_server);
  EXPECT_EQ(10, stats2->srtt.ToInternalValue());
  EXPECT_EQ(100, stats2->bandwidth_estimate.ToBitsPerSecond());
}

TEST_F(ServerNetworkStatsServerPropertiesTest, SetServerNetworkStats) {
  HostPortPair foo_server("foo", 80);
  const ServerNetworkStats* stats = impl_.GetServerNetworkStats(foo_server);
  EXPECT_EQ(NULL, stats);

  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(10);
  stats1.bandwidth_estimate = QuicBandwidth::FromBitsPerSecond(100);
  impl_.SetServerNetworkStats(foo_server, stats1);

  const ServerNetworkStats* stats2 = impl_.GetServerNetworkStats(foo_server);
  EXPECT_EQ(10, stats2->srtt.ToInternalValue());
  EXPECT_EQ(100, stats2->bandwidth_estimate.ToBitsPerSecond());

  impl_.Clear();
  const ServerNetworkStats* stats3 = impl_.GetServerNetworkStats(foo_server);
  EXPECT_EQ(NULL, stats3);
}

typedef HttpServerPropertiesImplTest QuicServerInfoServerPropertiesTest;

TEST_F(QuicServerInfoServerPropertiesTest, Initialize) {
  HostPortPair google_server("www.google.com", 443);
  QuicServerId quic_server_id(google_server, PRIVACY_MODE_ENABLED);

  // Check empty map.
  QuicServerInfoMap quic_server_info_map(QuicServerInfoMap::NO_AUTO_EVICT);
  impl_.InitializeQuicServerInfoMap(&quic_server_info_map);
  EXPECT_EQ(0u, impl_.quic_server_info_map().size());

  // Check by adding a QuicServerInfo into the map.
  std::string quic_server_info1("quic_server_info1");
  quic_server_info_map.Put(quic_server_id, quic_server_info1);
  impl_.InitializeQuicServerInfoMap(&quic_server_info_map);

  EXPECT_EQ(1u, impl_.quic_server_info_map().size());
  EXPECT_EQ(quic_server_info1, *impl_.GetQuicServerInfo(quic_server_id));
}

TEST_F(QuicServerInfoServerPropertiesTest, SetQuicServerInfo) {
  HostPortPair foo_server("foo", 80);
  QuicServerId quic_server_id(foo_server, PRIVACY_MODE_ENABLED);
  EXPECT_EQ(0u, impl_.quic_server_info_map().size());

  std::string quic_server_info1("quic_server_info1");
  impl_.SetQuicServerInfo(quic_server_id, quic_server_info1);

  EXPECT_EQ(1u, impl_.quic_server_info_map().size());
  EXPECT_EQ(quic_server_info1, *(impl_.GetQuicServerInfo(quic_server_id)));

  impl_.Clear();
  EXPECT_EQ(0u, impl_.quic_server_info_map().size());
  EXPECT_EQ(nullptr, impl_.GetQuicServerInfo(quic_server_id));
}

}  // namespace

}  // namespace net
