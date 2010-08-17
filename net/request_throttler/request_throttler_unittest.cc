// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/test_completion_callback.h"
#include "net/request_throttler/request_throttler_manager.h"
#include "net/request_throttler/request_throttler_header_interface.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;

namespace {
class MockRequestThrottlerManager;

class MockRequestThrottlerEntry : public RequestThrottlerEntry {
 public :
  MockRequestThrottlerEntry() {}
  MockRequestThrottlerEntry(TimeTicks release_time, TimeTicks fake_now)
      : fake_time_now_(fake_now) {
    release_time_ = release_time;
  }
  virtual ~MockRequestThrottlerEntry() {}

  void ResetToBlank(TimeTicks time_now) {
    fake_time_now_ = time_now;
    release_time_ = time_now;
    num_times_delayed_ = 0;
  }

  // Overloaded for test
  virtual TimeTicks GetTimeNow() const { return fake_time_now_; }

  TimeTicks release_time() const { return release_time_; }
  void set_release_time(TimeTicks time) { release_time_ = time; }

  TimeTicks fake_time_now_;
};

class MockRequestThrottlerHeaderAdapter
    : public RequestThrottlerHeaderInterface {
 public:
  MockRequestThrottlerHeaderAdapter()
      : fake_retry_value_("0.0"),
        fake_response_code_(0) {
  }

  MockRequestThrottlerHeaderAdapter(const std::string& retry_value,
                                    const int response_code)
      : fake_retry_value_(retry_value),
        fake_response_code_(response_code) {
  }

  ~MockRequestThrottlerHeaderAdapter() {}

  virtual std::string GetNormalizedValue(const std::string& key) const {
    if (key == MockRequestThrottlerEntry::kRetryHeaderName)
      return fake_retry_value_;
    return "";
  }

  virtual int GetResponseCode() const { return fake_response_code_; }

  std::string fake_retry_value_;
  int fake_response_code_;
};

class MockRequestThrottlerManager : public RequestThrottlerManager {
 public:
  MockRequestThrottlerManager() {}
  virtual ~MockRequestThrottlerManager() {}

  // Method to process the url using RequestThrottlerManager protected method.
  std::string DoGetUrlIdFromUrl(const GURL& url) { return GetIdFromUrl(url); }

  // Method to use the garbage collecting method of RequestThrottlerManager.
  void DoGarbageCollectEntries() { GarbageCollectEntries(); }

  // Returns the number of entries in the host map.
  int GetNumberOfEntries() { return url_entries_.size(); }

  void CreateEntry(const bool is_outdated);

  static int create_entry_index;
};

/*------------------ Mock request throttler manager --------------------------*/
int MockRequestThrottlerManager::create_entry_index = 0;

void MockRequestThrottlerManager::CreateEntry(const bool is_outdated) {
  TimeTicks time = TimeTicks::Now();
  if (is_outdated) {
    time -= TimeDelta::FromSeconds(
        MockRequestThrottlerEntry::kEntryLifetimeSec);
    time -= TimeDelta::FromSeconds(1);
  }
  std::string index = base::IntToString(create_entry_index++);
  url_entries_[index] = new MockRequestThrottlerEntry(time, TimeTicks::Now());
}

/* ---------------- Request throttler entry test -----------------------------*/
struct TimeAndBool {
  TimeAndBool(TimeTicks time_value, bool expected, int line_num) {
    time = time_value;
    result = expected;
    line = line_num;
  }
  TimeTicks time;
  bool result;
  int line;
};

struct GurlAndString {
  GurlAndString(GURL url_value, const std::string& expected, int line_num) {
    url = url_value;
    result = expected;
    line = line_num;
  }
  GURL url;
  std::string result;
  int line;
};

class RequestThrottlerEntryTest : public ::testing::Test {
 protected:
  virtual void SetUp();
  TimeTicks now_;
  TimeDelta delay_;
  scoped_refptr<MockRequestThrottlerEntry> entry_;
};

void RequestThrottlerEntryTest::SetUp() {
  now_ = TimeTicks::Now();
  entry_ = new MockRequestThrottlerEntry();
  entry_->ResetToBlank(now_);
}

std::ostream& operator<<(std::ostream& out, const base::TimeTicks& time) {
  return out << time.ToInternalValue();
}

TEST_F(RequestThrottlerEntryTest, InterfaceRequestNotAllowed) {
  entry_->set_release_time(entry_->fake_time_now_ +
      TimeDelta::FromMilliseconds(1));
  EXPECT_FALSE(entry_->IsRequestAllowed());
}

TEST_F(RequestThrottlerEntryTest, InterfaceRequestAllowed) {
  entry_->set_release_time(entry_->fake_time_now_);
  EXPECT_TRUE(entry_->IsRequestAllowed());
  entry_->set_release_time(entry_->fake_time_now_ -
      TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(entry_->IsRequestAllowed());
}

TEST_F(RequestThrottlerEntryTest, InterfaceUpdateRetryAfter) {
  // If the response we received as a retry-after field,
  // the request should be delayed.
  MockRequestThrottlerHeaderAdapter header_w_delay_header("5.5", 200);
  entry_->UpdateWithResponse(&header_w_delay_header);
  EXPECT_GT(entry_->release_time(), entry_->fake_time_now_) <<
      "When the server put a positive value in retry-after we should "
      "increase release_time";

  entry_->ResetToBlank(now_);
  header_w_delay_header.fake_retry_value_ = "-5.5";
  EXPECT_EQ(entry_->release_time(), entry_->fake_time_now_) <<
      "When given a negative value, it should not change the release_time";
}

TEST_F(RequestThrottlerEntryTest, InterfaceUpdateFailure) {
  MockRequestThrottlerHeaderAdapter failure_response("0", 505);
  entry_->UpdateWithResponse(&failure_response);
  EXPECT_GT(entry_->release_time(), entry_->fake_time_now_) <<
      "A failure should increase the release_time";
}

TEST_F(RequestThrottlerEntryTest, InterfaceUpdateSuccess) {
  MockRequestThrottlerHeaderAdapter success_response("0", 200);
  entry_->UpdateWithResponse(&success_response);
  EXPECT_EQ(entry_->release_time(), entry_->fake_time_now_) <<
      "A success should not add any delay";
}

TEST_F(RequestThrottlerEntryTest, InterfaceUpdateSuccessThenFailure) {
  MockRequestThrottlerHeaderAdapter failure_response("0", 500),
                                    success_response("0", 200);
  entry_->UpdateWithResponse(&success_response);
  entry_->UpdateWithResponse(&failure_response);
  EXPECT_GT(entry_->release_time(), entry_->fake_time_now_) <<
      "This scenario should add delay";
}

TEST_F(RequestThrottlerEntryTest, IsEntryReallyOutdated) {
  TimeDelta lifetime = TimeDelta::FromSeconds(
      MockRequestThrottlerEntry::kEntryLifetimeSec);
  const TimeDelta kFiveMs = TimeDelta::FromMilliseconds(5);

  TimeAndBool test_values[] = {
      TimeAndBool(now_, false, __LINE__),
      TimeAndBool(now_ - kFiveMs, false, __LINE__),
      TimeAndBool(now_ + kFiveMs, false, __LINE__),
      TimeAndBool(now_ - lifetime, false, __LINE__),
      TimeAndBool(now_ - (lifetime + kFiveMs), true, __LINE__)};

  for (unsigned int i = 0; i < arraysize(test_values); ++i) {
    entry_->set_release_time(test_values[i].time);
    EXPECT_EQ(entry_->IsEntryOutdated(), test_values[i].result) <<
        "Test case #" << i << " line " << test_values[i].line << " failed";
  }
}

TEST_F(RequestThrottlerEntryTest, MaxAllowedBackoff) {
  for (int i = 0; i < 30; ++i) {
    MockRequestThrottlerHeaderAdapter response_adapter("0.0", 505);
    entry_->UpdateWithResponse(&response_adapter);
  }

  delay_ = entry_->release_time() - now_;
  EXPECT_EQ(delay_.InMilliseconds(),
      MockRequestThrottlerEntry::kMaximumBackoffMs);
}

TEST_F(RequestThrottlerEntryTest, MalformedContent) {
  for (int i = 0; i < 5; ++i) {
    MockRequestThrottlerHeaderAdapter response_adapter("0.0", 505);
    entry_->UpdateWithResponse(&response_adapter);
  }
  TimeTicks release_after_failures = entry_->release_time();

  // Send a success code to reset backoff.
  MockRequestThrottlerHeaderAdapter response_adapter("0.0", 200);
  entry_->UpdateWithResponse(&response_adapter);
  EXPECT_EQ(entry_->release_time(), release_after_failures);

  // Then inform the entry that previous package was malformed
  // it is suppose to regenerate previous state.
  entry_->ReceivedContentWasMalformed();
  EXPECT_GT(entry_->release_time(), release_after_failures);
}

TEST(RequestThrottlerManager, IsUrlStandardised) {
  MockRequestThrottlerManager manager;
  GurlAndString test_values[] = {
      GurlAndString(GURL("http://www.example.com"),
                    std::string("http://www.example.com/"), __LINE__),
      GurlAndString(GURL("http://www.Example.com"),
                    std::string("http://www.example.com/"), __LINE__),
      GurlAndString(GURL("http://www.ex4mple.com/Pr4c71c41"),
                    std::string("http://www.ex4mple.com/pr4c71c41"), __LINE__),
      GurlAndString(GURL("http://www.example.com/0/token/false"),
                    std::string("http://www.example.com/0/token/false"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php?code=javascript"),
                    std::string("http://www.example.com/index.php"), __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php?code=1#superEntry"),
                    std::string("http://www.example.com/index.php"),
                    __LINE__)};

  for (unsigned int i = 0; i < arraysize(test_values); ++i) {
    std::string temp = manager.DoGetUrlIdFromUrl(test_values[i].url);
    EXPECT_EQ(temp, test_values[i].result) <<
        "Test case #" << i << " line " << test_values[i].line << " failed";
  }
}

TEST(RequestThrottlerManager, AreEntriesBeingCollected ) {
  MockRequestThrottlerManager manager;

  manager.CreateEntry(true);  // true = Entry is outdated.
  manager.CreateEntry(true);
  manager.CreateEntry(true);
  manager.DoGarbageCollectEntries();
  EXPECT_EQ(0, manager.GetNumberOfEntries());

  manager.CreateEntry(false);
  manager.CreateEntry(false);
  manager.CreateEntry(false);
  manager.CreateEntry(true);
  manager.DoGarbageCollectEntries();
  EXPECT_EQ(3, manager.GetNumberOfEntries());
}

TEST(RequestThrottlerManager, IsHostBeingRegistered) {
  MockRequestThrottlerManager manager;

  manager.RegisterRequestUrl(GURL("http://www.example.com/"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0?code=1"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0#lolsaure"));

  EXPECT_EQ(3, manager.GetNumberOfEntries());
}

}  // namespace
