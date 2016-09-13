// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/binding_set.h"
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

template <typename BindingSetType>
void ExpectContextHelper(BindingSetType* binding_set, void* expected_context) {
  EXPECT_EQ(expected_context, binding_set->dispatch_context());
}

template <typename BindingSetType>
base::Closure ExpectContext(BindingSetType* binding_set,
                            void* expected_context) {
  return base::Bind(
      &ExpectContextHelper<BindingSetType>, binding_set, expected_context);
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

  void* context_a = reinterpret_cast<void*>(1);
  void* context_b = reinterpret_cast<void*>(2);

  BindingSet<PingService> bindings_(BindingSetDispatchMode::WITH_CONTEXT);
  PingServicePtr ping_a, ping_b;
  bindings_.AddBinding(&impl, GetProxy(&ping_a), context_a);
  bindings_.AddBinding(&impl, GetProxy(&ping_b), context_b);

  {
    impl.set_ping_handler(ExpectContext(&bindings_, context_a));
    base::RunLoop loop;
    ping_a->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    impl.set_ping_handler(ExpectContext(&bindings_, context_b));
    base::RunLoop loop;
    ping_b->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    base::RunLoop loop;
    bindings_.set_connection_error_handler(
        Sequence(ExpectContext(&bindings_, context_a), loop.QuitClosure()));
    ping_a.reset();
    loop.Run();
  }

  {
    base::RunLoop loop;
    bindings_.set_connection_error_handler(
        Sequence(ExpectContext(&bindings_, context_b), loop.QuitClosure()));
    ping_b.reset();
    loop.Run();
  }

  EXPECT_TRUE(bindings_.empty());
}

TEST_F(BindingSetTest, BindingSetConnectionErrorWithReason) {
  PingImpl impl;
  PingServicePtr ptr;
  BindingSet<PingService> bindings;
  bindings.AddBinding(&impl, GetProxy(&ptr));

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
  PingProviderImpl() : ping_bindings_(BindingSetDispatchMode::WITH_CONTEXT) {}
  ~PingProviderImpl() override {}

  void set_new_ping_context(void* context) { new_ping_context_ = context; }

  void set_new_ping_handler(const base::Closure& handler) {
    new_ping_handler_ = handler;
  }

  void set_ping_handler(const base::Closure& handler) {
    ping_handler_ = handler;
  }

  AssociatedBindingSet<PingService>& ping_bindings() { return ping_bindings_; }

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

  AssociatedBindingSet<PingService> ping_bindings_;
  void* new_ping_context_ = nullptr;
  base::Closure ping_handler_;
  base::Closure new_ping_handler_;
};

TEST_F(BindingSetTest, AssociatedBindingSetContext) {
  AssociatedPingProviderPtr provider;
  PingProviderImpl impl;
  Binding<AssociatedPingProvider> binding(&impl, GetProxy(&provider));

  void* context_a = reinterpret_cast<void*>(1);
  void* context_b = reinterpret_cast<void*>(2);

  PingServiceAssociatedPtr ping_a;
  {
    base::RunLoop loop;
    impl.set_new_ping_context(context_a);
    impl.set_new_ping_handler(loop.QuitClosure());
    provider->GetPing(GetProxy(&ping_a, provider.associated_group()));
    loop.Run();
  }

  PingServiceAssociatedPtr ping_b;
  {
    base::RunLoop loop;
    impl.set_new_ping_context(context_b);
    impl.set_new_ping_handler(loop.QuitClosure());
    provider->GetPing(GetProxy(&ping_b, provider.associated_group()));
    loop.Run();
  }

  {
    impl.set_ping_handler(ExpectContext(&impl.ping_bindings(), context_a));
    base::RunLoop loop;
    ping_a->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    impl.set_ping_handler(ExpectContext(&impl.ping_bindings(), context_b));
    base::RunLoop loop;
    ping_b->Ping(loop.QuitClosure());
    loop.Run();
  }

  {
    base::RunLoop loop;
    impl.ping_bindings().set_connection_error_handler(
        Sequence(ExpectContext(&impl.ping_bindings(), context_a),
                 loop.QuitClosure()));
    ping_a.reset();
    loop.Run();
  }

  {
    base::RunLoop loop;
    impl.ping_bindings().set_connection_error_handler(
        Sequence(ExpectContext(&impl.ping_bindings(), context_b),
                 loop.QuitClosure()));
    ping_b.reset();
    loop.Run();
  }

  EXPECT_TRUE(impl.ping_bindings().empty());
}

TEST_F(BindingSetTest, MasterInterfaceBindingSetContext) {
  AssociatedPingProviderPtr provider_a, provider_b;
  PingProviderImpl impl;
  BindingSet<AssociatedPingProvider> bindings(
      BindingSetDispatchMode::WITH_CONTEXT);

  void* context_a = reinterpret_cast<void*>(1);
  void* context_b = reinterpret_cast<void*>(2);

  bindings.AddBinding(&impl, GetProxy(&provider_a), context_a);
  bindings.AddBinding(&impl, GetProxy(&provider_b), context_b);

  {
    PingServiceAssociatedPtr ping;
    base::RunLoop loop;
    impl.set_new_ping_handler(
        Sequence(ExpectContext(&bindings, context_a), loop.QuitClosure()));
    provider_a->GetPing(GetProxy(&ping, provider_a.associated_group()));
    loop.Run();
  }

  {
    PingServiceAssociatedPtr ping;
    base::RunLoop loop;
    impl.set_new_ping_handler(
        Sequence(ExpectContext(&bindings, context_b), loop.QuitClosure()));
    provider_b->GetPing(GetProxy(&ping, provider_b.associated_group()));
    loop.Run();
  }

  {
    base::RunLoop loop;
    bindings.set_connection_error_handler(
        Sequence(ExpectContext(&bindings, context_a), loop.QuitClosure()));
    provider_a.reset();
    loop.Run();
  }

  {
    base::RunLoop loop;
    bindings.set_connection_error_handler(
        Sequence(ExpectContext(&bindings, context_b), loop.QuitClosure()));
    provider_b.reset();
    loop.Run();
  }

  EXPECT_TRUE(bindings.empty());
}

TEST_F(BindingSetTest, AssociatedBindingSetConnectionErrorWithReason) {
  AssociatedPingProviderPtr master_ptr;
  PingProviderImpl master_impl;
  Binding<AssociatedPingProvider> master_binding(&master_impl, &master_ptr);

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
  master_ptr->GetPing(GetProxy(&ptr, master_ptr.associated_group()));

  ptr.ResetWithReason(2048u, "bye");

  run_loop.Run();
}

}  // namespace
}  // namespace test
}  // namespace mojo
