// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/quota_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;
using content::BrowserThread;

namespace extensions {

typedef QuotaLimitHeuristic::Bucket Bucket;
typedef QuotaLimitHeuristic::Config Config;
typedef QuotaLimitHeuristic::BucketList BucketList;
typedef QuotaService::TimedLimit TimedLimit;
typedef QuotaService::SustainedLimit SustainedLimit;

namespace {

const char kGenericName[] = "name";
const Config kFrozenConfig = {0, TimeDelta::FromDays(0)};
const Config k2PerMinute = {2, TimeDelta::FromMinutes(1)};
const Config k20PerHour = {20, TimeDelta::FromHours(1)};
const TimeTicks kStartTime = TimeTicks();
const TimeTicks k1MinuteAfterStart = kStartTime + TimeDelta::FromMinutes(1);

class Mapper : public QuotaLimitHeuristic::BucketMapper {
 public:
  Mapper() {}
  virtual ~Mapper() { STLDeleteValues(&buckets_); }
  virtual void GetBucketsForArgs(const base::ListValue* args,
                                 BucketList* buckets) OVERRIDE {
    for (size_t i = 0; i < args->GetSize(); i++) {
      int id;
      ASSERT_TRUE(args->GetInteger(i, &id));
      if (buckets_.find(id) == buckets_.end())
        buckets_[id] = new Bucket();
      buckets->push_back(buckets_[id]);
    }
  }

 private:
  typedef std::map<int, Bucket*> BucketMap;
  BucketMap buckets_;
  DISALLOW_COPY_AND_ASSIGN(Mapper);
};

class MockMapper : public QuotaLimitHeuristic::BucketMapper {
 public:
  virtual void GetBucketsForArgs(const base::ListValue* args,
                                 BucketList* buckets) OVERRIDE {}
};

class MockFunction : public ExtensionFunction {
 public:
  explicit MockFunction(const std::string& name) { set_name(name); }

  virtual void SetArgs(const base::ListValue* args) OVERRIDE {}
  virtual std::string GetError() const OVERRIDE { return std::string(); }
  virtual void SetError(const std::string& error) OVERRIDE {}
  virtual void Destruct() const OVERRIDE { delete this; }
  virtual ResponseAction Run() OVERRIDE { return RespondLater(); }
  virtual void SendResponse(bool) OVERRIDE {}

 protected:
  virtual ~MockFunction() {}
};

class TimedLimitMockFunction : public MockFunction {
 public:
  explicit TimedLimitMockFunction(const std::string& name)
      : MockFunction(name) {}
  virtual void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const
      OVERRIDE {
    heuristics->push_back(
        new TimedLimit(k2PerMinute, new Mapper(), kGenericName));
  }

 private:
  virtual ~TimedLimitMockFunction() {}
};

class ChainedLimitsMockFunction : public MockFunction {
 public:
  explicit ChainedLimitsMockFunction(const std::string& name)
      : MockFunction(name) {}
  virtual void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const
      OVERRIDE {
    // No more than 2 per minute sustained over 5 minutes.
    heuristics->push_back(new SustainedLimit(
        TimeDelta::FromMinutes(5), k2PerMinute, new Mapper(), kGenericName));
    // No more than 20 per hour.
    heuristics->push_back(
        new TimedLimit(k20PerHour, new Mapper(), kGenericName));
  }

 private:
  virtual ~ChainedLimitsMockFunction() {}
};

class FrozenMockFunction : public MockFunction {
 public:
  explicit FrozenMockFunction(const std::string& name) : MockFunction(name) {}
  virtual void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const
      OVERRIDE {
    heuristics->push_back(
        new TimedLimit(kFrozenConfig, new Mapper(), kGenericName));
  }

 private:
  virtual ~FrozenMockFunction() {}
};
}  // namespace

class QuotaServiceTest : public testing::Test {
 public:
  QuotaServiceTest()
      : extension_a_("a"),
        extension_b_("b"),
        extension_c_("c"),
        loop_(),
        ui_thread_(BrowserThread::UI, &loop_) {}
  virtual void SetUp() { service_.reset(new QuotaService()); }
  virtual void TearDown() {
    loop_.RunUntilIdle();
    service_.reset();
  }

 protected:
  std::string extension_a_;
  std::string extension_b_;
  std::string extension_c_;
  scoped_ptr<QuotaService> service_;
  base::MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
};

class QuotaLimitHeuristicTest : public testing::Test {
 public:
  static void DoMoreThan2PerMinuteFor5Minutes(const TimeTicks& start_time,
                                              QuotaLimitHeuristic* lim,
                                              Bucket* b,
                                              int an_unexhausted_minute) {
    for (int i = 0; i < 5; i++) {
      // Perform one operation in each minute.
      int m = i * 60;
      EXPECT_TRUE(lim->Apply(b, start_time + TimeDelta::FromSeconds(10 + m)));
      EXPECT_TRUE(b->has_tokens());

      if (i == an_unexhausted_minute)
        continue;  // Don't exhaust all tokens this minute.

      EXPECT_TRUE(lim->Apply(b, start_time + TimeDelta::FromSeconds(15 + m)));
      EXPECT_FALSE(b->has_tokens());

      // These are OK because we haven't exhausted all buckets.
      EXPECT_TRUE(lim->Apply(b, start_time + TimeDelta::FromSeconds(20 + m)));
      EXPECT_FALSE(b->has_tokens());
      EXPECT_TRUE(lim->Apply(b, start_time + TimeDelta::FromSeconds(50 + m)));
      EXPECT_FALSE(b->has_tokens());
    }
  }
};

TEST_F(QuotaLimitHeuristicTest, Timed) {
  TimedLimit lim(k2PerMinute, new MockMapper(), kGenericName);
  Bucket b;

  b.Reset(k2PerMinute, kStartTime);
  EXPECT_TRUE(lim.Apply(&b, kStartTime));
  EXPECT_TRUE(b.has_tokens());
  EXPECT_TRUE(lim.Apply(&b, kStartTime + TimeDelta::FromSeconds(30)));
  EXPECT_FALSE(b.has_tokens());
  EXPECT_FALSE(lim.Apply(&b, k1MinuteAfterStart));

  b.Reset(k2PerMinute, kStartTime);
  EXPECT_TRUE(lim.Apply(&b, k1MinuteAfterStart - TimeDelta::FromSeconds(1)));
  EXPECT_TRUE(lim.Apply(&b, k1MinuteAfterStart));
  EXPECT_TRUE(lim.Apply(&b, k1MinuteAfterStart + TimeDelta::FromSeconds(1)));
  EXPECT_TRUE(lim.Apply(&b, k1MinuteAfterStart + TimeDelta::FromSeconds(2)));
  EXPECT_FALSE(lim.Apply(&b, k1MinuteAfterStart + TimeDelta::FromSeconds(3)));
}

TEST_F(QuotaLimitHeuristicTest, Sustained) {
  SustainedLimit lim(
      TimeDelta::FromMinutes(5), k2PerMinute, new MockMapper(), kGenericName);
  Bucket bucket;

  bucket.Reset(k2PerMinute, kStartTime);
  DoMoreThan2PerMinuteFor5Minutes(kStartTime, &lim, &bucket, -1);
  // This straw breaks the camel's back.
  EXPECT_FALSE(lim.Apply(&bucket, kStartTime + TimeDelta::FromMinutes(6)));

  // The heuristic resets itself on a safe request.
  EXPECT_TRUE(lim.Apply(&bucket, kStartTime + TimeDelta::FromDays(1)));

  // Do the same as above except don't exhaust final bucket.
  bucket.Reset(k2PerMinute, kStartTime);
  DoMoreThan2PerMinuteFor5Minutes(kStartTime, &lim, &bucket, -1);
  EXPECT_TRUE(lim.Apply(&bucket, kStartTime + TimeDelta::FromMinutes(7)));

  // Do the same as above except don't exhaust the 3rd (w.l.o.g) bucket.
  bucket.Reset(k2PerMinute, kStartTime);
  DoMoreThan2PerMinuteFor5Minutes(kStartTime, &lim, &bucket, 3);
  // If the 3rd bucket were exhausted, this would fail (see first test).
  EXPECT_TRUE(lim.Apply(&bucket, kStartTime + TimeDelta::FromMinutes(6)));
}

TEST_F(QuotaServiceTest, NoHeuristic) {
  scoped_refptr<MockFunction> f(new MockFunction("foo"));
  base::ListValue args;
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), &args, kStartTime));
}

TEST_F(QuotaServiceTest, FrozenHeuristic) {
  scoped_refptr<MockFunction> f(new FrozenMockFunction("foo"));
  base::ListValue args;
  args.Append(new base::FundamentalValue(1));
  EXPECT_NE("", service_->Assess(extension_a_, f.get(), &args, kStartTime));
}

TEST_F(QuotaServiceTest, SingleHeuristic) {
  scoped_refptr<MockFunction> f(new TimedLimitMockFunction("foo"));
  base::ListValue args;
  args.Append(new base::FundamentalValue(1));
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), &args, kStartTime));
  EXPECT_EQ("",
            service_->Assess(extension_a_,
                             f.get(),
                             &args,
                             kStartTime + TimeDelta::FromSeconds(10)));
  EXPECT_NE("",
            service_->Assess(extension_a_,
                             f.get(),
                             &args,
                             kStartTime + TimeDelta::FromSeconds(15)));

  base::ListValue args2;
  args2.Append(new base::FundamentalValue(1));
  args2.Append(new base::FundamentalValue(2));
  EXPECT_EQ("", service_->Assess(extension_b_, f.get(), &args2, kStartTime));
  EXPECT_EQ("",
            service_->Assess(extension_b_,
                             f.get(),
                             &args2,
                             kStartTime + TimeDelta::FromSeconds(10)));

  TimeDelta peace = TimeDelta::FromMinutes(30);
  EXPECT_EQ("",
            service_->Assess(extension_b_, f.get(), &args, kStartTime + peace));
  EXPECT_EQ("",
            service_->Assess(extension_b_,
                             f.get(),
                             &args,
                             kStartTime + peace + TimeDelta::FromSeconds(10)));
  EXPECT_NE("",
            service_->Assess(extension_b_,
                             f.get(),
                             &args2,
                             kStartTime + peace + TimeDelta::FromSeconds(15)));

  // Test that items are independent.
  base::ListValue args3;
  args3.Append(new base::FundamentalValue(3));
  EXPECT_EQ("", service_->Assess(extension_c_, f.get(), &args, kStartTime));
  EXPECT_EQ("",
            service_->Assess(extension_c_,
                             f.get(),
                             &args3,
                             kStartTime + TimeDelta::FromSeconds(10)));
  EXPECT_EQ("",
            service_->Assess(extension_c_,
                             f.get(),
                             &args,
                             kStartTime + TimeDelta::FromSeconds(15)));
  EXPECT_EQ("",
            service_->Assess(extension_c_,
                             f.get(),
                             &args3,
                             kStartTime + TimeDelta::FromSeconds(20)));
  EXPECT_NE("",
            service_->Assess(extension_c_,
                             f.get(),
                             &args,
                             kStartTime + TimeDelta::FromSeconds(25)));
  EXPECT_NE("",
            service_->Assess(extension_c_,
                             f.get(),
                             &args3,
                             kStartTime + TimeDelta::FromSeconds(30)));
}

TEST_F(QuotaServiceTest, ChainedHeuristics) {
  scoped_refptr<MockFunction> f(new ChainedLimitsMockFunction("foo"));
  base::ListValue args;
  args.Append(new base::FundamentalValue(1));

  // First, test that the low limit can be avoided but the higher one is hit.
  // One event per minute for 20 minutes comes in under the sustained limit,
  // but is equal to the timed limit.
  for (int i = 0; i < 20; i++) {
    EXPECT_EQ(
        "",
        service_->Assess(extension_a_,
                         f.get(),
                         &args,
                         kStartTime + TimeDelta::FromSeconds(10 + i * 60)));
  }

  // This will bring us to 21 events in an hour, which is a violation.
  EXPECT_NE("",
            service_->Assess(extension_a_,
                             f.get(),
                             &args,
                             kStartTime + TimeDelta::FromMinutes(30)));

  // Now, check that we can still hit the lower limit.
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(
        "",
        service_->Assess(extension_b_,
                         f.get(),
                         &args,
                         kStartTime + TimeDelta::FromSeconds(10 + i * 60)));
    EXPECT_EQ(
        "",
        service_->Assess(extension_b_,
                         f.get(),
                         &args,
                         kStartTime + TimeDelta::FromSeconds(15 + i * 60)));
    EXPECT_EQ(
        "",
        service_->Assess(extension_b_,
                         f.get(),
                         &args,
                         kStartTime + TimeDelta::FromSeconds(20 + i * 60)));
  }

  EXPECT_NE("",
            service_->Assess(extension_b_,
                             f.get(),
                             &args,
                             kStartTime + TimeDelta::FromMinutes(6)));
}

TEST_F(QuotaServiceTest, MultipleFunctionsDontInterfere) {
  scoped_refptr<MockFunction> f(new TimedLimitMockFunction("foo"));
  scoped_refptr<MockFunction> g(new TimedLimitMockFunction("bar"));

  base::ListValue args_f;
  base::ListValue args_g;
  args_f.Append(new base::FundamentalValue(1));
  args_g.Append(new base::FundamentalValue(2));

  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), &args_f, kStartTime));
  EXPECT_EQ("", service_->Assess(extension_a_, g.get(), &args_g, kStartTime));
  EXPECT_EQ("",
            service_->Assess(extension_a_,
                             f.get(),
                             &args_f,
                             kStartTime + TimeDelta::FromSeconds(10)));
  EXPECT_EQ("",
            service_->Assess(extension_a_,
                             g.get(),
                             &args_g,
                             kStartTime + TimeDelta::FromSeconds(10)));
  EXPECT_NE("",
            service_->Assess(extension_a_,
                             f.get(),
                             &args_f,
                             kStartTime + TimeDelta::FromSeconds(15)));
  EXPECT_NE("",
            service_->Assess(extension_a_,
                             g.get(),
                             &args_g,
                             kStartTime + TimeDelta::FromSeconds(15)));
}

TEST_F(QuotaServiceTest, ViolatorsWillBeViolators) {
  scoped_refptr<MockFunction> f(new TimedLimitMockFunction("foo"));
  scoped_refptr<MockFunction> g(new TimedLimitMockFunction("bar"));
  base::ListValue arg;
  arg.Append(new base::FundamentalValue(1));
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), &arg, kStartTime));
  EXPECT_EQ("",
            service_->Assess(extension_a_,
                             f.get(),
                             &arg,
                             kStartTime + TimeDelta::FromSeconds(10)));
  EXPECT_NE("",
            service_->Assess(extension_a_,
                             f.get(),
                             &arg,
                             kStartTime + TimeDelta::FromSeconds(15)));

  // We don't allow this extension to use quota limited functions even if they
  // wait a while.
  EXPECT_NE(
      "",
      service_->Assess(
          extension_a_, f.get(), &arg, kStartTime + TimeDelta::FromDays(1)));
  EXPECT_NE(
      "",
      service_->Assess(
          extension_a_, g.get(), &arg, kStartTime + TimeDelta::FromDays(1)));
}

}  // namespace extensions
