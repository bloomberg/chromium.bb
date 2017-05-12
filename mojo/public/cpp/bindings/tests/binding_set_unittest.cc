// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "mojo/public/interfaces/bindings/tests/ping_service.mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_associated_interfaces.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

class BindingSetTest : public testing::Test {
 public:
  BindingSetTest() {}
  ~BindingSetTest() override {}

  base::MessageLoop& loop() { return loop_; }

 private:
  base::MessageLoop loop_;

  DISALLOW_COPY_AND_ASSIGN(BindingSetTest);
};

template <typename BindingSetType, typename ContextType>
void ExpectContextHelper(BindingSetType* binding_set,
                         ContextType expected_context) {
  EXPECT_EQ(expected_context, binding_set->dispatch_context());
}

template <typename BindingSetType, typename ContextType>
base::Closure ExpectContext(BindingSetType* binding_set,
                            ContextType expected_context) {
  return base::Bind(
      &ExpectContextHelper<BindingSetType, ContextType>, binding_set,
      expected_context);
}

base::Closure Sequence(const base::Closure& first,
                       const base::Closure& second) {
  return base::Bind(
      [] (const base::Closure& first, const base::Closure& second) {
        first.Run();
        second.Run();
      }, first, second);
}

class PingImpl : public PingService {
 public:
  PingImpl() {}
  ~PingImpl() override {}

  void set_ping_handler(const base::Closure& handler) {
    ping_handler_ = handler;
  }

 private:
  // PingService:
  void Ping(const PingCallback& callback) override {
    if (!ping_handler_.is_null())
      ping_handler_.Run();
    callback.Run();
  }

  base::Closure ping_handler_;
};

TEST_F(BindingSetTest, BindingSetContext) {
  PingImpl impl;

  BindingSet<PingService, int> bindings;
  PingServicePtr ping_a, ping_b;
  bindings.AddBinding(&impl, MakeRequest(&ping_a), 1);
  bindings.AddBinding(&impl, MakeRequest(&ping_b), 2);

  {
    impl.set_ping_handler(ExpectContext(&bindings, 1));
    base::RunLoop loop;
    ping_a->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    impl.set_ping_handler(ExpectContext(&bindings, 2));
    base::RunLoop loop;
    ping_b->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    base::RunLoop loop;
    bindings.set_connection_error_handler(
        Sequence(ExpectContext(&bindings, 1), loop.QuitClosure()));
    ping_a.reset();
    loop.Run();
  }

  {
    base::RunLoop loop;
    bindings.set_connection_error_handler(
        Sequence(ExpectContext(&bindings, 2), loop.QuitClosure()));
    ping_b.reset();
    loop.Run();
  }

  EXPECT_TRUE(bindings.empty());
}

TEST_F(BindingSetTest, BindingSetConnectionErrorWithReason) {
  PingImpl impl;
  PingServicePtr ptr;
  BindingSet<PingService> bindings;
  bindings.AddBinding(&impl, MakeRequest(&ptr));

  base::RunLoop run_loop;
  bindings.set_connection_error_with_reason_handler(base::Bind(
      [](const base::Closure& quit_closure, uint32_t custom_reason,
         const std::string& description) {
        EXPECT_EQ(1024u, custom_reason);
        EXPECT_EQ("bye", description);
        quit_closure.Run();
      },
      run_loop.QuitClosure()));

  ptr.ResetWithReason(1024u, "bye");
}

class PingProviderImpl : public AssociatedPingProvider, public PingService {
 public:
  PingProviderImpl() {}
  ~PingProviderImpl() override {}

  void set_new_ping_context(int context) { new_ping_context_ = context; }

  void set_new_ping_handler(const base::Closure& handler) {
    new_ping_handler_ = handler;
  }

  void set_ping_handler(const base::Closure& handler) {
    ping_handler_ = handler;
  }

  AssociatedBindingSet<PingService, int>& ping_bindings() {
    return ping_bindings_;
  }

 private:
  // AssociatedPingProvider:
  void GetPing(PingServiceAssociatedRequest request) override {
    ping_bindings_.AddBinding(this, std::move(request), new_ping_context_);
    if (!new_ping_handler_.is_null())
      new_ping_handler_.Run();
  }

  // PingService:
  void Ping(const PingCallback& callback) override {
    if (!ping_handler_.is_null())
      ping_handler_.Run();
    callback.Run();
  }

  AssociatedBindingSet<PingService, int> ping_bindings_;
  int new_ping_context_ = -1;
  base::Closure ping_handler_;
  base::Closure new_ping_handler_;
};

TEST_F(BindingSetTest, AssociatedBindingSetContext) {
  AssociatedPingProviderPtr provider;
  PingProviderImpl impl;
  Binding<AssociatedPingProvider> binding(&impl, MakeRequest(&provider));

  PingServiceAssociatedPtr ping_a;
  {
    base::RunLoop loop;
    impl.set_new_ping_context(1);
    impl.set_new_ping_handler(loop.QuitClosure());
    provider->GetPing(MakeRequest(&ping_a));
    loop.Run();
  }

  PingServiceAssociatedPtr ping_b;
  {
    base::RunLoop loop;
    impl.set_new_ping_context(2);
    impl.set_new_ping_handler(loop.QuitClosure());
    provider->GetPing(MakeRequest(&ping_b));
    loop.Run();
  }

  {
    impl.set_ping_handler(ExpectContext(&impl.ping_bindings(), 1));
    base::RunLoop loop;
    ping_a->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    impl.set_ping_handler(ExpectContext(&impl.ping_bindings(), 2));
    base::RunLoop loop;
    ping_b->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    base::RunLoop loop;
    impl.ping_bindings().set_connection_error_handler(
        Sequence(ExpectContext(&impl.ping_bindings(), 1), loop.QuitClosure()));
    ping_a.reset();
    loop.Run();
  }

  {
    base::RunLoop loop;
    impl.ping_bindings().set_connection_error_handler(
        Sequence(ExpectContext(&impl.ping_bindings(), 2), loop.QuitClosure()));
    ping_b.reset();
    loop.Run();
  }

  EXPECT_TRUE(impl.ping_bindings().empty());
}

TEST_F(BindingSetTest, MasterInterfaceBindingSetContext) {
  AssociatedPingProviderPtr provider_a, provider_b;
  PingProviderImpl impl;
  BindingSet<AssociatedPingProvider, int> bindings;

  bindings.AddBinding(&impl, MakeRequest(&provider_a), 1);
  bindings.AddBinding(&impl, MakeRequest(&provider_b), 2);

  {
    PingServiceAssociatedPtr ping;
    base::RunLoop loop;
    impl.set_new_ping_handler(
        Sequence(ExpectContext(&bindings, 1), loop.QuitClosure()));
    provider_a->GetPing(MakeRequest(&ping));
    loop.Run();
  }

  {
    PingServiceAssociatedPtr ping;
    base::RunLoop loop;
    impl.set_new_ping_handler(
        Sequence(ExpectContext(&bindings, 2), loop.QuitClosure()));
    provider_b->GetPing(MakeRequest(&ping));
    loop.Run();
  }

  {
    base::RunLoop loop;
    bindings.set_connection_error_handler(
        Sequence(ExpectContext(&bindings, 1), loop.QuitClosure()));
    provider_a.reset();
    loop.Run();
  }

  {
    base::RunLoop loop;
    bindings.set_connection_error_handler(
        Sequence(ExpectContext(&bindings, 2), loop.QuitClosure()));
    provider_b.reset();
    loop.Run();
  }

  EXPECT_TRUE(bindings.empty());
}

TEST_F(BindingSetTest, PreDispatchHandler) {
  PingImpl impl;

  BindingSet<PingService, int> bindings;
  PingServicePtr ping_a, ping_b;
  bindings.AddBinding(&impl, MakeRequest(&ping_a), 1);
  bindings.AddBinding(&impl, MakeRequest(&ping_b), 2);

  {
    bindings.set_pre_dispatch_handler(base::Bind([] (const int& context) {
      EXPECT_EQ(1, context);
    }));
    base::RunLoop loop;
    ping_a->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    bindings.set_pre_dispatch_handler(base::Bind([] (const int& context) {
      EXPECT_EQ(2, context);
    }));
    base::RunLoop loop;
    ping_b->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    base::RunLoop loop;
    bindings.set_pre_dispatch_handler(
        base::Bind([](base::RunLoop* loop, const int& context) {
          EXPECT_EQ(1, context);
          loop->Quit();
        }, &loop));
    ping_a.reset();
    loop.Run();
  }

  {
    base::RunLoop loop;
    bindings.set_pre_dispatch_handler(
        base::Bind([](base::RunLoop* loop, const int& context) {
          EXPECT_EQ(2, context);
          loop->Quit();
        }, &loop));
    ping_b.reset();
    loop.Run();
  }

  EXPECT_TRUE(bindings.empty());
}

TEST_F(BindingSetTest, AssociatedBindingSetConnectionErrorWithReason) {
  AssociatedPingProviderPtr master_ptr;
  PingProviderImpl master_impl;
  Binding<AssociatedPingProvider> master_binding(&master_impl,
                                                 MakeRequest(&master_ptr));

  base::RunLoop run_loop;
  master_impl.ping_bindings().set_connection_error_with_reason_handler(
      base::Bind(
          [](const base::Closure& quit_closure, uint32_t custom_reason,
             const std::string& description) {
            EXPECT_EQ(2048u, custom_reason);
            EXPECT_EQ("bye", description);
            quit_closure.Run();
          },
          run_loop.QuitClosure()));

  PingServiceAssociatedPtr ptr;
  master_ptr->GetPing(MakeRequest(&ptr));

  ptr.ResetWithReason(2048u, "bye");

  run_loop.Run();
}

class PingInstanceCounter : public PingService {
 public:
  PingInstanceCounter() { ++instance_count; }
  ~PingInstanceCounter() override { --instance_count; }

  void Ping(const PingCallback& callback) override {}

  static int instance_count;
};
int PingInstanceCounter::instance_count = 0;

TEST_F(BindingSetTest, StrongBinding_Destructor) {
  PingServicePtr ping_a, ping_b;
  auto bindings = base::MakeUnique<StrongBindingSet<PingService>>();

  bindings->AddBinding(base::MakeUnique<PingInstanceCounter>(),
                       mojo::MakeRequest(&ping_a));
  EXPECT_EQ(1, PingInstanceCounter::instance_count);

  bindings->AddBinding(base::MakeUnique<PingInstanceCounter>(),
                       mojo::MakeRequest(&ping_b));
  EXPECT_EQ(2, PingInstanceCounter::instance_count);

  bindings.reset();
  EXPECT_EQ(0, PingInstanceCounter::instance_count);
}

TEST_F(BindingSetTest, StrongBinding_ConnectionError) {
  PingServicePtr ping_a, ping_b;
  StrongBindingSet<PingService> bindings;
  bindings.AddBinding(base::MakeUnique<PingInstanceCounter>(),
                      mojo::MakeRequest(&ping_a));
  bindings.AddBinding(base::MakeUnique<PingInstanceCounter>(),
                      mojo::MakeRequest(&ping_b));
  EXPECT_EQ(2, PingInstanceCounter::instance_count);

  ping_a.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, PingInstanceCounter::instance_count);

  ping_b.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, PingInstanceCounter::instance_count);
}

TEST_F(BindingSetTest, StrongBinding_RemoveBinding) {
  PingServicePtr ping_a, ping_b;
  StrongBindingSet<PingService> bindings;
  BindingId binding_id_a = bindings.AddBinding(
      base::MakeUnique<PingInstanceCounter>(), mojo::MakeRequest(&ping_a));
  BindingId binding_id_b = bindings.AddBinding(
      base::MakeUnique<PingInstanceCounter>(), mojo::MakeRequest(&ping_b));
  EXPECT_EQ(2, PingInstanceCounter::instance_count);

  EXPECT_TRUE(bindings.RemoveBinding(binding_id_a));
  EXPECT_EQ(1, PingInstanceCounter::instance_count);

  EXPECT_TRUE(bindings.RemoveBinding(binding_id_b));
  EXPECT_EQ(0, PingInstanceCounter::instance_count);
}

}  // namespace
}  // namespace test
}  // namespace mojo
