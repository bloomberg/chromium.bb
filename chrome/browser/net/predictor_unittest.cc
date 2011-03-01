// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include <algorithm>
#include <sstream>
#include <string>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/timer.h"
#include "base/values.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/net/url_info.h"
#include "chrome/common/net/predictor_common.h"
#include "content/browser/browser_thread.h"
#include "net/base/address_list.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/winsock_init.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace chrome_browser_net {

class WaitForResolutionHelper;

typedef base::RepeatingTimer<WaitForResolutionHelper> HelperTimer;

class WaitForResolutionHelper {
 public:
  WaitForResolutionHelper(Predictor* predictor, const UrlList& hosts,
                          HelperTimer* timer)
      : predictor_(predictor),
        hosts_(hosts),
        timer_(timer) {
  }

  void Run() {
    for (UrlList::const_iterator i = hosts_.begin(); i != hosts_.end(); ++i)
      if (predictor_->GetResolutionDuration(*i) ==
          UrlInfo::kNullDuration)
        return;  // We don't have resolution for that host.

    // When all hostnames have been resolved, exit the loop.
    timer_->Stop();
    MessageLoop::current()->Quit();
    delete timer_;
    delete this;
  }

 private:
  Predictor* predictor_;
  const UrlList hosts_;
  HelperTimer* timer_;
};

class PredictorTest : public testing::Test {
 public:
  PredictorTest()
      : io_thread_(BrowserThread::IO, &loop_),
        host_resolver_(new net::MockCachingHostResolver()),
        default_max_queueing_delay_(TimeDelta::FromMilliseconds(
            PredictorInit::kMaxSpeculativeResolveQueueDelayMs)) {
  }

 protected:
  virtual void SetUp() {
#if defined(OS_WIN)
    net::EnsureWinsockInit();
#endif
    // Since we are using a caching HostResolver, the following latencies will
    // only be incurred by the first request, after which the result will be
    // cached internally by |host_resolver_|.
    net::RuleBasedHostResolverProc* rules = host_resolver_->rules();
    rules->AddRuleWithLatency("www.google.com", "127.0.0.1", 50);
    rules->AddRuleWithLatency("gmail.google.com.com", "127.0.0.1", 70);
    rules->AddRuleWithLatency("mail.google.com", "127.0.0.1", 44);
    rules->AddRuleWithLatency("gmail.com", "127.0.0.1", 63);
  }

  void WaitForResolution(Predictor* predictor, const UrlList& hosts) {
    HelperTimer* timer = new HelperTimer();
    timer->Start(TimeDelta::FromMilliseconds(100),
                 new WaitForResolutionHelper(predictor, hosts, timer),
                 &WaitForResolutionHelper::Run);
    MessageLoop::current()->Run();
  }

 private:
  // IMPORTANT: do not move this below |host_resolver_|; the host resolver
  // must not outlive the message loop, otherwise bad things can happen
  // (like posting to a deleted message loop).
  MessageLoop loop_;
  BrowserThread io_thread_;

 protected:
  scoped_ptr<net::MockCachingHostResolver> host_resolver_;

  // Shorthand to access TimeDelta of PredictorInit::kMaxQueueingDelayMs.
  // (It would be a static constant... except style rules preclude that :-/ ).
  const TimeDelta default_max_queueing_delay_;
};

//------------------------------------------------------------------------------

TEST_F(PredictorTest, StartupShutdownTest) {
  scoped_refptr<Predictor> testing_master(
      new Predictor(host_resolver_.get(),
                    default_max_queueing_delay_,
                    PredictorInit::kMaxSpeculativeParallelResolves,
                    false));
  testing_master->Shutdown();
}


TEST_F(PredictorTest, ShutdownWhenResolutionIsPendingTest) {
  scoped_refptr<net::WaitingHostResolverProc> resolver_proc(
      new net::WaitingHostResolverProc(NULL));
  host_resolver_->Reset(resolver_proc);

  scoped_refptr<Predictor> testing_master(
      new Predictor(host_resolver_.get(),
                    default_max_queueing_delay_,
                    PredictorInit::kMaxSpeculativeParallelResolves,
                    false));

  GURL localhost("http://localhost:80");
  UrlList names;
  names.push_back(localhost);

  testing_master->ResolveList(names, UrlInfo::PAGE_SCAN_MOTIVATED);

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(), 500);
  MessageLoop::current()->Run();

  EXPECT_FALSE(testing_master->WasFound(localhost));

  testing_master->Shutdown();

  // Clean up after ourselves.
  resolver_proc->Signal();
  MessageLoop::current()->RunAllPending();
}

TEST_F(PredictorTest, SingleLookupTest) {
  scoped_refptr<Predictor> testing_master(
      new Predictor(host_resolver_.get(),
                    default_max_queueing_delay_,
                    PredictorInit::kMaxSpeculativeParallelResolves,
                    false));

  GURL goog("http://www.google.com:80");

  UrlList names;
  names.push_back(goog);

  // Try to flood the predictor with many concurrent requests.
  for (int i = 0; i < 10; i++)
    testing_master->ResolveList(names, UrlInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(testing_master, names);

  EXPECT_TRUE(testing_master->WasFound(goog));

  MessageLoop::current()->RunAllPending();

  EXPECT_GT(testing_master->peak_pending_lookups(), names.size() / 2);
  EXPECT_LE(testing_master->peak_pending_lookups(), names.size());
  EXPECT_LE(testing_master->peak_pending_lookups(),
            testing_master->max_concurrent_dns_lookups());

  testing_master->Shutdown();
}

TEST_F(PredictorTest, ConcurrentLookupTest) {
  host_resolver_->rules()->AddSimulatedFailure("*.notfound");

  scoped_refptr<Predictor> testing_master(
      new Predictor(host_resolver_.get(),
                    default_max_queueing_delay_,
                    PredictorInit::kMaxSpeculativeParallelResolves,
                    false));

  GURL goog("http://www.google.com:80"),
      goog2("http://gmail.google.com.com:80"),
      goog3("http://mail.google.com:80"),
      goog4("http://gmail.com:80");
  GURL bad1("http://bad1.notfound:80"),
      bad2("http://bad2.notfound:80");

  UrlList names;
  names.push_back(goog);
  names.push_back(goog3);
  names.push_back(bad1);
  names.push_back(goog2);
  names.push_back(bad2);
  names.push_back(goog4);
  names.push_back(goog);

  // Try to flood the predictor with many concurrent requests.
  for (int i = 0; i < 10; i++)
    testing_master->ResolveList(names, UrlInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(testing_master, names);

  EXPECT_TRUE(testing_master->WasFound(goog));
  EXPECT_TRUE(testing_master->WasFound(goog3));
  EXPECT_TRUE(testing_master->WasFound(goog2));
  EXPECT_TRUE(testing_master->WasFound(goog4));
  EXPECT_FALSE(testing_master->WasFound(bad1));
  EXPECT_FALSE(testing_master->WasFound(bad2));

  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(testing_master->WasFound(bad1));
  EXPECT_FALSE(testing_master->WasFound(bad2));

  EXPECT_LE(testing_master->peak_pending_lookups(), names.size());
  EXPECT_LE(testing_master->peak_pending_lookups(),
            testing_master->max_concurrent_dns_lookups());

  testing_master->Shutdown();
}

TEST_F(PredictorTest, MassiveConcurrentLookupTest) {
  host_resolver_->rules()->AddSimulatedFailure("*.notfound");

  scoped_refptr<Predictor> testing_master(
      new Predictor(host_resolver_.get(),
                    default_max_queueing_delay_,
                    PredictorInit::kMaxSpeculativeParallelResolves,
                    false));

  UrlList names;
  for (int i = 0; i < 100; i++)
    names.push_back(GURL(
        "http://host" + base::IntToString(i) + ".notfound:80"));

  // Try to flood the predictor with many concurrent requests.
  for (int i = 0; i < 10; i++)
    testing_master->ResolveList(names, UrlInfo::PAGE_SCAN_MOTIVATED);

  WaitForResolution(testing_master, names);

  MessageLoop::current()->RunAllPending();

  EXPECT_LE(testing_master->peak_pending_lookups(), names.size());
  EXPECT_LE(testing_master->peak_pending_lookups(),
            testing_master->max_concurrent_dns_lookups());

  testing_master->Shutdown();
}

//------------------------------------------------------------------------------
// Functions to help synthesize and test serializations of subresource referrer
// lists.

// Return a motivation_list if we can find one for the given motivating_host (or
// NULL if a match is not found).
static ListValue* FindSerializationMotivation(
    const GURL& motivation, const ListValue& referral_list) {
  CHECK_LT(0u, referral_list.GetSize());  // Room for version.
  int format_version = -1;
  CHECK(referral_list.GetInteger(0, &format_version));
  CHECK_EQ(Predictor::PREDICTOR_REFERRER_VERSION, format_version);
  ListValue* motivation_list(NULL);
  for (size_t i = 1; i < referral_list.GetSize(); ++i) {
    referral_list.GetList(i, &motivation_list);
    std::string existing_spec;
    EXPECT_TRUE(motivation_list->GetString(0, &existing_spec));
    if (motivation == GURL(existing_spec))
      return motivation_list;
  }
  return NULL;
}

// Create a new empty serialization list.
static ListValue* NewEmptySerializationList() {
  ListValue* list = new ListValue;
  list->Append(new FundamentalValue(Predictor::PREDICTOR_REFERRER_VERSION));
  return list;
}

// Add a motivating_url and a subresource_url to a serialized list, using
// this given latency. This is a helper function for quickly building these
// lists.
static void AddToSerializedList(const GURL& motivation,
                                const GURL& subresource,
                                double use_rate,
                                ListValue* referral_list ) {
  // Find the motivation if it is already used.
  ListValue* motivation_list = FindSerializationMotivation(motivation,
                                                           *referral_list);
  if (!motivation_list) {
    // This is the first mention of this motivation, so build a list.
    motivation_list = new ListValue;
    motivation_list->Append(new StringValue(motivation.spec()));
    // Provide empty subresource list.
    motivation_list->Append(new ListValue());

    // ...and make it part of the serialized referral_list.
    referral_list->Append(motivation_list);
  }

  ListValue* subresource_list(NULL);
  // 0 == url; 1 == subresource_list.
  EXPECT_TRUE(motivation_list->GetList(1, &subresource_list));

  // We won't bother to check for the subresource being there already.  Worst
  // case, during deserialization, the latency value we supply plus the
  // existing value(s) will be added to the referrer.

  subresource_list->Append(new StringValue(subresource.spec()));
  subresource_list->Append(new FundamentalValue(use_rate));
}

static const int kLatencyNotFound = -1;

// For a given motivation, and subresource, find what latency is currently
// listed.  This assume a well formed serialization, which has at most one such
// entry for any pair of names.  If no such pair is found, then return false.
// Data is written into use_rate arguments.
static bool GetDataFromSerialization(const GURL& motivation,
                                     const GURL& subresource,
                                     const ListValue& referral_list,
                                     double* use_rate) {
  ListValue* motivation_list = FindSerializationMotivation(motivation,
                                                           referral_list);
  if (!motivation_list)
    return false;
  ListValue* subresource_list;
  EXPECT_TRUE(motivation_list->GetList(1, &subresource_list));
  for (size_t i = 0; i < subresource_list->GetSize();) {
    std::string url_spec;
    EXPECT_TRUE(subresource_list->GetString(i++, &url_spec));
    EXPECT_TRUE(subresource_list->GetDouble(i++, use_rate));
    if (subresource == GURL(url_spec)) {
      return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------------

// Make sure nil referral lists really have no entries, and no latency listed.
TEST_F(PredictorTest, ReferrerSerializationNilTest) {
  scoped_refptr<Predictor> predictor(
      new Predictor(host_resolver_.get(),
                    default_max_queueing_delay_,
                    PredictorInit::kMaxSpeculativeParallelResolves,
                    false));
  scoped_ptr<ListValue> referral_list(NewEmptySerializationList());
  predictor->SerializeReferrers(referral_list.get());
  EXPECT_EQ(1U, referral_list->GetSize());
  EXPECT_FALSE(GetDataFromSerialization(
    GURL("http://a.com:79"), GURL("http://b.com:78"),
      *referral_list.get(), NULL));

  predictor->Shutdown();
}

// Make sure that when a serialization list includes a value, that it can be
// deserialized into the database, and can be extracted back out via
// serialization without being changed.
TEST_F(PredictorTest, ReferrerSerializationSingleReferrerTest) {
  scoped_refptr<Predictor> predictor(
      new Predictor(host_resolver_.get(),
                    default_max_queueing_delay_,
                    PredictorInit::kMaxSpeculativeParallelResolves,
                    false));
  const GURL motivation_url("http://www.google.com:91");
  const GURL subresource_url("http://icons.google.com:90");
  const double kUseRate = 23.4;
  scoped_ptr<ListValue> referral_list(NewEmptySerializationList());

  AddToSerializedList(motivation_url, subresource_url,
      kUseRate, referral_list.get());

  predictor->DeserializeReferrers(*referral_list.get());

  ListValue recovered_referral_list;
  predictor->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(2U, recovered_referral_list.GetSize());
  double rate;
  EXPECT_TRUE(GetDataFromSerialization(
      motivation_url, subresource_url, recovered_referral_list, &rate));
  EXPECT_EQ(rate, kUseRate);

  predictor->Shutdown();
}

// Make sure the Trim() functionality works as expected.
TEST_F(PredictorTest, ReferrerSerializationTrimTest) {
  scoped_refptr<Predictor> predictor(
      new Predictor(host_resolver_.get(),
                    default_max_queueing_delay_,
                    PredictorInit::kMaxSpeculativeParallelResolves,
                    false));
  GURL motivation_url("http://www.google.com:110");

  GURL icon_subresource_url("http://icons.google.com:111");
  const double kRateIcon = 16.0 * Predictor::kPersistWorthyExpectedValue;
  GURL img_subresource_url("http://img.google.com:118");
  const double kRateImg = 8.0 * Predictor::kPersistWorthyExpectedValue;

  scoped_ptr<ListValue> referral_list(NewEmptySerializationList());
  AddToSerializedList(
      motivation_url, icon_subresource_url, kRateIcon, referral_list.get());
  AddToSerializedList(
      motivation_url, img_subresource_url, kRateImg, referral_list.get());

  predictor->DeserializeReferrers(*referral_list.get());

  ListValue recovered_referral_list;
  predictor->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(2U, recovered_referral_list.GetSize());
  double rate;
  EXPECT_TRUE(GetDataFromSerialization(
      motivation_url, icon_subresource_url, recovered_referral_list,
      &rate));
  EXPECT_EQ(rate, kRateIcon);

  EXPECT_TRUE(GetDataFromSerialization(
      motivation_url, img_subresource_url, recovered_referral_list, &rate));
  EXPECT_EQ(rate, kRateImg);

  // Each time we Trim, the user_rate figures should reduce by a factor of two,
  // until they both are small, an then a trim will delete the whole entry.
  predictor->TrimReferrers();
  predictor->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(2U, recovered_referral_list.GetSize());
  EXPECT_TRUE(GetDataFromSerialization(
      motivation_url, icon_subresource_url, recovered_referral_list, &rate));
  EXPECT_EQ(rate, kRateIcon / 2);

  EXPECT_TRUE(GetDataFromSerialization(
      motivation_url, img_subresource_url, recovered_referral_list, &rate));
  EXPECT_EQ(rate, kRateImg / 2);

  predictor->TrimReferrers();
  predictor->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(2U, recovered_referral_list.GetSize());
  EXPECT_TRUE(GetDataFromSerialization(
      motivation_url, icon_subresource_url, recovered_referral_list, &rate));
  EXPECT_EQ(rate, kRateIcon / 4);
  EXPECT_TRUE(GetDataFromSerialization(
      motivation_url, img_subresource_url, recovered_referral_list, &rate));
  EXPECT_EQ(rate, kRateImg / 4);

  predictor->TrimReferrers();
  predictor->SerializeReferrers(&recovered_referral_list);
  EXPECT_EQ(2U, recovered_referral_list.GetSize());
  EXPECT_TRUE(GetDataFromSerialization(
      motivation_url, icon_subresource_url, recovered_referral_list, &rate));
  EXPECT_EQ(rate, kRateIcon / 8);

  // Img is below threshold, and so it gets deleted.
  EXPECT_FALSE(GetDataFromSerialization(
      motivation_url, img_subresource_url, recovered_referral_list, &rate));

  predictor->TrimReferrers();
  predictor->SerializeReferrers(&recovered_referral_list);
  // Icon is also trimmed away, so entire set gets discarded.
  EXPECT_EQ(1U, recovered_referral_list.GetSize());
  EXPECT_FALSE(GetDataFromSerialization(
      motivation_url, icon_subresource_url, recovered_referral_list, &rate));
  EXPECT_FALSE(GetDataFromSerialization(
      motivation_url, img_subresource_url, recovered_referral_list, &rate));

  predictor->Shutdown();
}


TEST_F(PredictorTest, PriorityQueuePushPopTest) {
  Predictor::HostNameQueue queue;

  GURL first("http://first:80"), second("http://second:90");

  // First check high priority queue FIFO functionality.
  EXPECT_TRUE(queue.IsEmpty());
  queue.Push(first, UrlInfo::LEARNED_REFERAL_MOTIVATED);
  EXPECT_FALSE(queue.IsEmpty());
  queue.Push(second, UrlInfo::MOUSE_OVER_MOTIVATED);
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.Pop(), first);
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.Pop(), second);
  EXPECT_TRUE(queue.IsEmpty());

  // Then check low priority queue FIFO functionality.
  queue.Push(first, UrlInfo::PAGE_SCAN_MOTIVATED);
  EXPECT_FALSE(queue.IsEmpty());
  queue.Push(second, UrlInfo::OMNIBOX_MOTIVATED);
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.Pop(), first);
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.Pop(), second);
  EXPECT_TRUE(queue.IsEmpty());
}

TEST_F(PredictorTest, PriorityQueueReorderTest) {
  Predictor::HostNameQueue queue;

  // Push all the low priority items.
  GURL low1("http://low1:80"),
      low2("http://low2:80"),
      low3("http://low3:443"),
      low4("http://low4:80"),
      low5("http://low5:80"),
      hi1("http://hi1:80"),
      hi2("http://hi2:80"),
      hi3("http://hi3:80");

  EXPECT_TRUE(queue.IsEmpty());
  queue.Push(low1, UrlInfo::PAGE_SCAN_MOTIVATED);
  queue.Push(low2, UrlInfo::UNIT_TEST_MOTIVATED);
  queue.Push(low3, UrlInfo::LINKED_MAX_MOTIVATED);
  queue.Push(low4, UrlInfo::OMNIBOX_MOTIVATED);
  queue.Push(low5, UrlInfo::STARTUP_LIST_MOTIVATED);
  queue.Push(low4, UrlInfo::OMNIBOX_MOTIVATED);

  // Push all the high prority items
  queue.Push(hi1, UrlInfo::LEARNED_REFERAL_MOTIVATED);
  queue.Push(hi2, UrlInfo::STATIC_REFERAL_MOTIVATED);
  queue.Push(hi3, UrlInfo::MOUSE_OVER_MOTIVATED);

  // Check that high priority stuff comes out first, and in FIFO order.
  EXPECT_EQ(queue.Pop(), hi1);
  EXPECT_EQ(queue.Pop(), hi2);
  EXPECT_EQ(queue.Pop(), hi3);

  // ...and then low priority strings.
  EXPECT_EQ(queue.Pop(), low1);
  EXPECT_EQ(queue.Pop(), low2);
  EXPECT_EQ(queue.Pop(), low3);
  EXPECT_EQ(queue.Pop(), low4);
  EXPECT_EQ(queue.Pop(), low5);
  EXPECT_EQ(queue.Pop(), low4);

  EXPECT_TRUE(queue.IsEmpty());
}

TEST_F(PredictorTest, CanonicalizeUrl) {
  // Base case, only handles HTTP and HTTPS.
  EXPECT_EQ(GURL(), Predictor::CanonicalizeUrl(GURL("ftp://anything")));

  // Remove path testing.
  GURL long_url("http://host:999/path?query=value");
  EXPECT_EQ(Predictor::CanonicalizeUrl(long_url), long_url.GetWithEmptyPath());

  // Default port cannoncalization.
  GURL implied_port("http://test");
  GURL explicit_port("http://test:80");
  EXPECT_EQ(Predictor::CanonicalizeUrl(implied_port),
            Predictor::CanonicalizeUrl(explicit_port));

  // Port is still maintained.
  GURL port_80("http://test:80");
  GURL port_90("http://test:90");
  EXPECT_NE(Predictor::CanonicalizeUrl(port_80),
            Predictor::CanonicalizeUrl(port_90));

  // Host is still maintained.
  GURL host_1("http://test_1");
  GURL host_2("http://test_2");
  EXPECT_NE(Predictor::CanonicalizeUrl(host_1),
            Predictor::CanonicalizeUrl(host_2));

  // Scheme is maintained (mismatch identified).
  GURL http("http://test");
  GURL https("https://test");
  EXPECT_NE(Predictor::CanonicalizeUrl(http),
            Predictor::CanonicalizeUrl(https));

  // Https works fine.
  GURL long_https("https://host:999/path?query=value");
  EXPECT_EQ(Predictor::CanonicalizeUrl(long_https),
            long_https.GetWithEmptyPath());
}

}  // namespace chrome_browser_net
