// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_error_observer_mojo.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "mojo/common/common_type_converters.h"
#include "net/test/event_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class ErrorObserverClient : public interfaces::ProxyResolverErrorObserver {
 public:
  enum Event {
    ERROR_RECEIVED,
  };

  explicit ErrorObserverClient(
      mojo::InterfaceRequest<interfaces::ProxyResolverErrorObserver> request);

  EventWaiter<Event>& event_waiter() { return event_waiter_; }
  const std::vector<std::pair<int, base::string16>>& errors() const {
    return errors_;
  }

 private:
  void OnPacScriptError(int32_t line_number,
                        const mojo::String& error) override;

  mojo::Binding<interfaces::ProxyResolverErrorObserver> binding_;
  EventWaiter<Event> event_waiter_;
  std::vector<std::pair<int, base::string16>> errors_;

  DISALLOW_COPY_AND_ASSIGN(ErrorObserverClient);
};

ErrorObserverClient::ErrorObserverClient(
    mojo::InterfaceRequest<interfaces::ProxyResolverErrorObserver> request)
    : binding_(this, request.Pass()) {
}

void ErrorObserverClient::OnPacScriptError(int32_t line_number,
                                           const mojo::String& error) {
  errors_.push_back(std::make_pair(line_number, error.To<base::string16>()));
  event_waiter_.NotifyEvent(ERROR_RECEIVED);
}

}  // namespace

class ProxyResolverErrorObserverMojoTest : public testing::Test {
 public:
  ProxyResolverErrorObserver& error_observer() { return *error_observer_; }
  ErrorObserverClient& client() { return *client_; }

 private:
  void SetUp() override {
    interfaces::ProxyResolverErrorObserverPtr error_observer_ptr;
    client_.reset(new ErrorObserverClient(mojo::GetProxy(&error_observer_ptr)));
    error_observer_ =
        ProxyResolverErrorObserverMojo::Create(error_observer_ptr.Pass());
    ASSERT_TRUE(error_observer_);
  }

  scoped_ptr<ErrorObserverClient> client_;
  scoped_ptr<ProxyResolverErrorObserver> error_observer_;
};

TEST_F(ProxyResolverErrorObserverMojoTest, NullHandle) {
  EXPECT_FALSE(ProxyResolverErrorObserverMojo::Create(
      interfaces::ProxyResolverErrorObserverPtr()));
}

TEST_F(ProxyResolverErrorObserverMojoTest, ErrorReportedOnMainThread) {
  base::string16 error(base::ASCIIToUTF16("error message"));
  error_observer().OnPACScriptError(123, error);
  client().event_waiter().WaitForEvent(ErrorObserverClient::ERROR_RECEIVED);
  ASSERT_EQ(1u, client().errors().size());
  EXPECT_EQ(123, client().errors()[0].first);
  EXPECT_EQ(error, client().errors()[0].second);
}

TEST_F(ProxyResolverErrorObserverMojoTest, ErrorReportedOnAnotherThread) {
  base::Thread other_thread("error reporting thread");
  base::string16 error(base::ASCIIToUTF16("error message"));
  other_thread.Start();
  other_thread.message_loop()->PostTask(
      FROM_HERE, base::Bind(&ProxyResolverErrorObserver::OnPACScriptError,
                            base::Unretained(&error_observer()), 123, error));
  client().event_waiter().WaitForEvent(ErrorObserverClient::ERROR_RECEIVED);
  ASSERT_EQ(1u, client().errors().size());
  EXPECT_EQ(123, client().errors()[0].first);
  EXPECT_EQ(error, client().errors()[0].second);
}

}  // namespace net
