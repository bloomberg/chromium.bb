// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "net/base/net_log.h"
#include "net/base/sdch_manager.h"
#include "net/sdch/sdch_owner.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

static const char generic_url[] = "http://www.example.com";
static const char generic_domain[] = "www.example.com";

static std::string NewSdchDictionary(size_t dictionary_size) {
  std::string dictionary;
  dictionary.append("Domain: ");
  dictionary.append(generic_domain);
  dictionary.append("\n");
  dictionary.append("\n");

  size_t original_dictionary_size = dictionary.size();
  dictionary.resize(dictionary_size);
  for (size_t i = original_dictionary_size; i < dictionary_size; ++i)
    dictionary[i] = static_cast<char>((i % 127) + 1);

  return dictionary;
}

int outstanding_url_request_error_counting_jobs = 0;
base::Closure* empty_url_request_jobs_callback = 0;

// Variation of URLRequestErrorJob to count number of outstanding
// instances and notify when that goes to zero.
class URLRequestErrorCountingJob : public URLRequestJob {
 public:
  URLRequestErrorCountingJob(URLRequest* request,
                             NetworkDelegate* network_delegate,
                             int error)
      : URLRequestJob(request, network_delegate),
        error_(error),
        weak_factory_(this) {
    ++outstanding_url_request_error_counting_jobs;
  }

  void Start() override {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&URLRequestErrorCountingJob::StartAsync,
                              weak_factory_.GetWeakPtr()));
  }

 private:
  ~URLRequestErrorCountingJob() override {
    --outstanding_url_request_error_counting_jobs;
    if (0 == outstanding_url_request_error_counting_jobs &&
        empty_url_request_jobs_callback) {
      empty_url_request_jobs_callback->Run();
    }
  }

  void StartAsync() {
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, error_));
  }

  int error_;

  base::WeakPtrFactory<URLRequestErrorCountingJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestErrorCountingJob);
};

static int error_jobs_created = 0;

class MockURLRequestJobFactory : public URLRequestJobFactory {
 public:
  MockURLRequestJobFactory() {}

  ~MockURLRequestJobFactory() override {}

  URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    ++error_jobs_created;
    return new URLRequestErrorCountingJob(request, network_delegate,
                                          ERR_INTERNET_DISCONNECTED);
  }

  URLRequestJob* MaybeInterceptRedirect(URLRequest* request,
                                        NetworkDelegate* network_delegate,
                                        const GURL& location) const override {
    return nullptr;
  }

  URLRequestJob* MaybeInterceptResponse(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    return nullptr;
  }

  bool IsHandledProtocol(const std::string& scheme) const override {
    return scheme == "http";
  };

  bool IsHandledURL(const GURL& url) const override {
    return url.SchemeIs("http");
  }

  bool IsSafeRedirectTarget(const GURL& location) const override {
    return false;
  }
};

class SdchOwnerTest : public testing::Test {
 public:
  static const size_t kMaxSizeForTesting = 1000 * 50;
  static const size_t kMinFetchSpaceForTesting = 500;

  SdchOwnerTest()
      : last_jobs_created_(error_jobs_created),
        dictionary_creation_index_(0),
        sdch_owner_(&sdch_manager_, &url_request_context_) {
    // Any jobs created on this context will immediately error,
    // which leaves the test in control of signals to SdchOwner.
    url_request_context_.set_job_factory(&job_factory_);

    // Reduce sizes to reduce time for string operations.
    sdch_owner_.SetMaxTotalDictionarySize(kMaxSizeForTesting);
    sdch_owner_.SetMinSpaceForDictionaryFetch(kMinFetchSpaceForTesting);
  }

  SdchManager& sdch_manager() { return sdch_manager_; }
  SdchOwner& sdch_owner() { return sdch_owner_; }
  BoundNetLog& bound_net_log() { return net_log_; }

  int JobsRecentlyCreated() {
    int result = error_jobs_created - last_jobs_created_;
    last_jobs_created_ = error_jobs_created;
    return result;
  }

  bool DictionaryPresentInManager(const std::string& server_hash) {
    // Presumes all tests use generic url.
    SdchProblemCode tmp;
    scoped_ptr<SdchManager::DictionarySet> set(
        sdch_manager_.GetDictionarySetByHash(GURL(generic_url), server_hash,
                                             &tmp));
    return !!set.get();
  }

  void SignalGetDictionaryAndClearJobs(GURL request_url, GURL dictionary_url) {
    sdch_owner().OnGetDictionary(&sdch_manager_, request_url, dictionary_url);
    if (outstanding_url_request_error_counting_jobs == 0)
      return;
    base::RunLoop run_loop;
    base::Closure quit_closure(run_loop.QuitClosure());
    empty_url_request_jobs_callback = &quit_closure;
    run_loop.Run();
    empty_url_request_jobs_callback = NULL;
  }

  // Create a unique (by hash) dictionary of the given size,
  // associate it with a unique URL, add it to the manager through
  // SdchOwner::OnDictionaryFetched(), and return whether that
  // addition was successful or not.
  bool CreateAndAddDictionary(size_t size, std::string* server_hash_p) {
    GURL dictionary_url(
        base::StringPrintf("%s/d%d", generic_url, dictionary_creation_index_));
    std::string dictionary_text(NewSdchDictionary(size - 4));
    dictionary_text += base::StringPrintf("%04d", dictionary_creation_index_);
    ++dictionary_creation_index_;
    std::string client_hash;
    std::string server_hash;
    SdchManager::GenerateHash(dictionary_text, &client_hash, &server_hash);

    if (DictionaryPresentInManager(server_hash))
      return false;
    sdch_owner().OnDictionaryFetched(dictionary_text, dictionary_url, net_log_);
    if (server_hash_p)
      *server_hash_p = server_hash;
    return DictionaryPresentInManager(server_hash);
  }

 private:
  int last_jobs_created_;
  BoundNetLog net_log_;
  int dictionary_creation_index_;

  // The dependencies of these objects (sdch_owner_ -> {sdch_manager_,
  // url_request_context_}, url_request_context_->job_factory_) require
  // this order for correct destruction semantics.
  MockURLRequestJobFactory job_factory_;
  URLRequestContext url_request_context_;
  SdchManager sdch_manager_;
  SdchOwner sdch_owner_;

  DISALLOW_COPY_AND_ASSIGN(SdchOwnerTest);
};

// Does OnGetDictionary result in a fetch when there's enough space, and not
// when there's not?
TEST_F(SdchOwnerTest, OnGetDictionary_Fetching) {
  GURL request_url(std::string(generic_url) + "/r1");

  // Fetch generated when empty.
  GURL dict_url1(std::string(generic_url) + "/d1");
  EXPECT_EQ(0, JobsRecentlyCreated());
  SignalGetDictionaryAndClearJobs(request_url, dict_url1);
  EXPECT_EQ(1, JobsRecentlyCreated());

  // Fetch generated when half full.
  GURL dict_url2(std::string(generic_url) + "/d2");
  std::string dictionary1(NewSdchDictionary(kMaxSizeForTesting / 2));
  sdch_owner().OnDictionaryFetched(dictionary1, dict_url1, bound_net_log());
  EXPECT_EQ(0, JobsRecentlyCreated());
  SignalGetDictionaryAndClearJobs(request_url, dict_url2);
  EXPECT_EQ(1, JobsRecentlyCreated());

  // Fetch not generated when close to completely full.
  GURL dict_url3(std::string(generic_url) + "/d3");
  std::string dictionary2(NewSdchDictionary(
      (kMaxSizeForTesting / 2 - kMinFetchSpaceForTesting / 2)));
  sdch_owner().OnDictionaryFetched(dictionary2, dict_url2, bound_net_log());
  EXPECT_EQ(0, JobsRecentlyCreated());
  SignalGetDictionaryAndClearJobs(request_url, dict_url3);
  EXPECT_EQ(0, JobsRecentlyCreated());
}

// Make sure attempts to add dictionaries do what they should.
TEST_F(SdchOwnerTest, OnDictionaryFetched_Fetching) {
  GURL request_url(std::string(generic_url) + "/r1");
  std::string client_hash;
  std::string server_hash;

  // Add successful when empty.
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 2, nullptr));
  EXPECT_EQ(0, JobsRecentlyCreated());

  // Add successful when half full.
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 2, nullptr));
  EXPECT_EQ(0, JobsRecentlyCreated());

  // Add unsuccessful when full.
  EXPECT_FALSE(CreateAndAddDictionary(kMaxSizeForTesting / 2, nullptr));
  EXPECT_EQ(0, JobsRecentlyCreated());
}

// Confirm auto-eviction happens if space is needed.
TEST_F(SdchOwnerTest, ConfirmAutoEviction) {
  std::string server_hash_d1;
  std::string server_hash_d2;
  std::string server_hash_d3;

  // Add two dictionaries, one recent, one more than a day in the past.
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 2, &server_hash_d1));

  scoped_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  clock->SetNow(base::Time::Now() - base::TimeDelta::FromDays(2));
  sdch_owner().SetClockForTesting(clock.Pass());

  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 2, &server_hash_d2));

  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d2));

  // The addition of a new dictionary should succeed, evicting the old one.
  clock.reset(new base::SimpleTestClock);
  clock->SetNow(base::Time::Now());
  sdch_owner().SetClockForTesting(clock.Pass());

  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 2, &server_hash_d3));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_FALSE(DictionaryPresentInManager(server_hash_d2));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d3));
}

// Confirm auto-eviction happens if space is needed, with a more complicated
// situation
TEST_F(SdchOwnerTest, ConfirmAutoEviction_2) {
  std::string server_hash_d1;
  std::string server_hash_d2;
  std::string server_hash_d3;

  // Add dictionaries, one recent, two more than a day in the past that
  // between them add up to the space needed.
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 2, &server_hash_d1));

  scoped_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  clock->SetNow(base::Time::Now() - base::TimeDelta::FromDays(2));
  sdch_owner().SetClockForTesting(clock.Pass());

  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 4, &server_hash_d2));
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 4, &server_hash_d3));

  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d2));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d3));

  // The addition of a new dictionary should succeed, evicting the old one.
  clock.reset(new base::SimpleTestClock);
  clock->SetNow(base::Time::Now());
  sdch_owner().SetClockForTesting(clock.Pass());

  std::string server_hash_d4;
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 2, &server_hash_d4));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_FALSE(DictionaryPresentInManager(server_hash_d2));
  EXPECT_FALSE(DictionaryPresentInManager(server_hash_d3));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d4));
}

// Confirm if only one dictionary needs to be evicted it's the oldest.
TEST_F(SdchOwnerTest, ConfirmAutoEviction_Oldest) {
  std::string server_hash_d1;
  std::string server_hash_d2;
  std::string server_hash_d3;

  // Add dictionaries, one recent, one two days in the past, and one
  // four days in the past.
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 4, &server_hash_d1));

  scoped_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  clock->SetNow(base::Time::Now() - base::TimeDelta::FromDays(2));
  sdch_owner().SetClockForTesting(clock.Pass());
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 4, &server_hash_d2));

  clock.reset(new base::SimpleTestClock);
  clock->SetNow(base::Time::Now() - base::TimeDelta::FromDays(4));
  sdch_owner().SetClockForTesting(clock.Pass());
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 4, &server_hash_d3));

  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d2));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d3));

  // The addition of a new dictionary should succeed, evicting only the
  // oldest one.
  clock.reset(new base::SimpleTestClock);
  clock->SetNow(base::Time::Now());
  sdch_owner().SetClockForTesting(clock.Pass());

  std::string server_hash_d4;
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 2, &server_hash_d4));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d2));
  EXPECT_FALSE(DictionaryPresentInManager(server_hash_d3));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d4));
}

// Confirm using a dictionary changes eviction behavior properly.
TEST_F(SdchOwnerTest, UseChangesEviction) {
  std::string server_hash_d1;
  std::string server_hash_d2;
  std::string server_hash_d3;

  // Add dictionaries, one recent, one two days in the past, and one
  // four days in the past.
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 4, &server_hash_d1));

  scoped_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  clock->SetNow(base::Time::Now() - base::TimeDelta::FromDays(2));
  sdch_owner().SetClockForTesting(clock.Pass());
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 4, &server_hash_d2));

  clock.reset(new base::SimpleTestClock);
  clock->SetNow(base::Time::Now() - base::TimeDelta::FromDays(4));
  sdch_owner().SetClockForTesting(clock.Pass());
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 4, &server_hash_d3));

  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d2));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d3));

  clock.reset(new base::SimpleTestClock);
  clock->SetNow(base::Time::Now());
  sdch_owner().SetClockForTesting(clock.Pass());

  // Use the oldest dictionary.
  sdch_owner().OnDictionaryUsed(&sdch_manager(), server_hash_d3);

  // The addition of a new dictionary should succeed, evicting only the
  // newer stale one.
  std::string server_hash_d4;
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 2, &server_hash_d4));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_FALSE(DictionaryPresentInManager(server_hash_d2));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d3));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d4));
}

// Confirm using a dictionary can prevent the addition of a new dictionary.
TEST_F(SdchOwnerTest, UsePreventsAddition) {
  std::string server_hash_d1;
  std::string server_hash_d2;
  std::string server_hash_d3;

  // Add dictionaries, one recent, one two days in the past, and one
  // four days in the past.
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 4, &server_hash_d1));

  scoped_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  clock->SetNow(base::Time::Now() - base::TimeDelta::FromDays(2));
  sdch_owner().SetClockForTesting(clock.Pass());
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 4, &server_hash_d2));

  clock.reset(new base::SimpleTestClock);
  clock->SetNow(base::Time::Now() - base::TimeDelta::FromDays(4));
  sdch_owner().SetClockForTesting(clock.Pass());
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting / 4, &server_hash_d3));

  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d2));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d3));

  clock.reset(new base::SimpleTestClock);
  clock->SetNow(base::Time::Now());
  sdch_owner().SetClockForTesting(clock.Pass());

  // Use the older dictionaries.
  sdch_owner().OnDictionaryUsed(&sdch_manager(), server_hash_d2);
  sdch_owner().OnDictionaryUsed(&sdch_manager(), server_hash_d3);

  // The addition of a new dictionary should fail, not evicting anything.
  std::string server_hash_d4;
  EXPECT_FALSE(CreateAndAddDictionary(kMaxSizeForTesting / 2, &server_hash_d4));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d2));
  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d3));
  EXPECT_FALSE(DictionaryPresentInManager(server_hash_d4));
}

// Confirm clear gets all the space back.
TEST_F(SdchOwnerTest, ClearReturnsSpace) {
  std::string server_hash_d1;
  std::string server_hash_d2;

  // Take up all the space.
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting, &server_hash_d1));

  // Addition should fail.
  EXPECT_FALSE(CreateAndAddDictionary(kMaxSizeForTesting, &server_hash_d2));

  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_FALSE(DictionaryPresentInManager(server_hash_d2));

  sdch_manager().ClearData();
  EXPECT_FALSE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_FALSE(DictionaryPresentInManager(server_hash_d2));

  // Addition should now succeed.
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting, nullptr));
}

// Confirm memory pressure gets all the space back.
// TODO(rmcilroy) Disabled while investigating http://crbug.com/447208 -
// re-enable once fixed.
TEST_F(SdchOwnerTest, DISABLED_MemoryPressureReturnsSpace) {
  std::string server_hash_d1;
  std::string server_hash_d2;

  // Take up all the space.
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting, &server_hash_d1));

  // Addition should fail.
  EXPECT_FALSE(CreateAndAddDictionary(kMaxSizeForTesting, &server_hash_d2));

  EXPECT_TRUE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_FALSE(DictionaryPresentInManager(server_hash_d2));

  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  // The notification may have (implementation note: does :-}) use a PostTask,
  // so we drain the local message queue.  This should be safe (i.e. not have
  // an inifinite number of messages) in a unit test.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(DictionaryPresentInManager(server_hash_d1));
  EXPECT_FALSE(DictionaryPresentInManager(server_hash_d2));

  // Addition should now succeed.
  EXPECT_TRUE(CreateAndAddDictionary(kMaxSizeForTesting, nullptr));
}

}  // namespace net
