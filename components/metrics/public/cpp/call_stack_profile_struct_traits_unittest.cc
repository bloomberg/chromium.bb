// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "components/metrics/public/interfaces/call_stack_profile_collector_test.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

base::StackSamplingProfiler::CallStackProfile CreateProfile(
    const std::vector<base::StackSamplingProfiler::Module>& modules,
    const std::vector<base::StackSamplingProfiler::Sample>& samples,
    base::TimeDelta profile_duration,
    base::TimeDelta sampling_period) {
  base::StackSamplingProfiler::CallStackProfile profile;
  profile.modules = modules;
  profile.samples = samples;
  profile.profile_duration = profile_duration;
  profile.sampling_period = sampling_period;
  return profile;
}

}

class CallStackProfileCollectorTestImpl
    : public mojom::CallStackProfileCollectorTest {
 public:
  explicit CallStackProfileCollectorTestImpl(
      mojo::InterfaceRequest<mojom::CallStackProfileCollectorTest> request)
      : binding_(this, std::move(request)) {
  }

  // CallStackProfileCollectorTest:
  void BounceFrame(const base::StackSamplingProfiler::Frame& in,
                   const BounceFrameCallback& callback) override {
    callback.Run(in);
  }

  void BounceModule(const base::StackSamplingProfiler::Module& in,
                    const BounceModuleCallback& callback) override {
    callback.Run(in);
  }

  void BounceProfile(const base::StackSamplingProfiler::CallStackProfile& in,
                     const BounceProfileCallback& callback) override {
    callback.Run(in);
  }

 private:
  mojo::Binding<CallStackProfileCollectorTest> binding_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileCollectorTestImpl);
};

class CallStackProfileStructTraitsTest : public testing::Test {
 public:
  CallStackProfileStructTraitsTest() : impl_(GetProxy(&proxy_)) {}

 protected:
  base::MessageLoop message_loop_;
  mojom::CallStackProfileCollectorTestPtr proxy_;
  CallStackProfileCollectorTestImpl impl_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileStructTraitsTest);
};

// Checks serialization/deserialization of Module fields.
TEST_F(CallStackProfileStructTraitsTest, Module) {
  using Module = base::StackSamplingProfiler::Module;

  struct SerializeCase {
    Module module;
    bool expect_success;
  };

  const SerializeCase serialize_cases[] = {
    // Null base address.
    {
      Module(0x0, "abcd", base::FilePath(base::FilePath::kCurrentDirectory)),
      true
    },
    // Non-null base address.
    {
      Module(0x10, "abcd", base::FilePath(base::FilePath::kCurrentDirectory)),
      true
    },
    // Base address with a bit set beyond 32 bits, when built for x64.
    {
      Module(1ULL << (sizeof(uintptr_t) * 8) * 3 / 4, "abcd",
             base::FilePath(base::FilePath::kCurrentDirectory)),
      true
    },
    // Empty module id.
    {
      Module(0x10, "", base::FilePath(base::FilePath::kCurrentDirectory)),
      true
    },
    // Module id at the length limit.
    {
      Module(0x10, std::string(40, ' '),
             base::FilePath(base::FilePath::kCurrentDirectory)),
      true
    },
    // Module id beyond the length limit.
    {
      Module(0x10, std::string(41, ' '),
             base::FilePath(base::FilePath::kCurrentDirectory)),
      false
    },
  };

  for (const SerializeCase& input : serialize_cases) {
    Module output;
    EXPECT_EQ(input.expect_success,
              proxy_->BounceModule(input.module, &output));

    if (!input.expect_success)
      continue;

    EXPECT_EQ(input.module.base_address, output.base_address);
    EXPECT_EQ(input.module.id, output.id);
    EXPECT_EQ(input.module.filename, output.filename);
  }
}

// Checks serialization/deserialization of Frame fields.
TEST_F(CallStackProfileStructTraitsTest, Frame) {
  using Frame = base::StackSamplingProfiler::Frame;

  const Frame serialize_cases[] = {
    // Null instruction pointer.
    Frame(0x0, 10),
    // Non-null instruction pointer.
    Frame(0x10, 10),
    // Instruction pointer with a bit set beyond 32 bits, when built for x64.
    Frame(1ULL << (sizeof(uintptr_t) * 8) * 3 / 4, 10),
    // Zero module index.
    Frame(0xabcd, 0),
    // Non-zero module index.
    Frame(0xabcd, 1),
    // Non-zero module index.
    Frame(0xabcd, 10),
    // Unknown module index.
    Frame(0xabcd, Frame::kUnknownModuleIndex),
  };

  for (const Frame& input : serialize_cases) {
    Frame output;
    EXPECT_TRUE(proxy_->BounceFrame(input, &output));

    EXPECT_EQ(input.instruction_pointer, output.instruction_pointer);
    EXPECT_EQ(input.module_index, output.module_index);
  }
}

// Checks serialization/deserialization of Profile fields, including validation
// of the Frame module_index field.
TEST_F(CallStackProfileStructTraitsTest, Profile) {
  using Module = base::StackSamplingProfiler::Module;
  using Frame = base::StackSamplingProfiler::Frame;
  using Sample = base::StackSamplingProfiler::Sample;
  using Profile = base::StackSamplingProfiler::CallStackProfile;

  struct SerializeCase {
    Profile profile;
    bool expect_success;
  };

  const SerializeCase serialize_cases[] = {
    // Empty modules and samples.
    {
      CreateProfile(std::vector<Module>(), std::vector<Sample>(),
                    base::TimeDelta::FromSeconds(1),
                    base::TimeDelta::FromSeconds(2)),
      true
    },
    // Non-empty modules and empty samples.
    {
      CreateProfile({ Module(0x4000, "a", base::FilePath()) },
                    std::vector<Sample>(),
                    base::TimeDelta::FromSeconds(1),
                    base::TimeDelta::FromSeconds(2)),
      true
    },
    // Valid values for modules and samples.
    {
      CreateProfile({
                      Module(0x4000, "a", base::FilePath()),
                      Module(0x4100, "b", base::FilePath()),
                    },
                    {
                      {
                        Frame(0x4010, 0),
                        Frame(0x4110, 1),
                        Frame(0x4110, Frame::kUnknownModuleIndex),
                      }
                    },
                    base::TimeDelta::FromSeconds(1),
                    base::TimeDelta::FromSeconds(2)),
      true
    },
    // Valid values for modules, but an out of range module index in the second
    // sample.
    {
      CreateProfile({
                      Module(0x4000, "a", base::FilePath()),
                      Module(0x4100, "b", base::FilePath()),
                    },
                    {
                      {
                        Frame(0x4010, 0),
                        Frame(0x4110, 1),
                        Frame(0x4110, Frame::kUnknownModuleIndex),
                      },
                      {
                        Frame(0x4010, 0),
                        Frame(0x4110, 2),
                      },
                    },
                    base::TimeDelta::FromSeconds(1),
                    base::TimeDelta::FromSeconds(2)),
      false
    },
  };

  for (const SerializeCase& input : serialize_cases) {
    SCOPED_TRACE(&input - &serialize_cases[0]);

    Profile output;
    EXPECT_EQ(input.expect_success,
              proxy_->BounceProfile(input.profile, &output));

    if (!input.expect_success)
      continue;

    EXPECT_EQ(input.profile.modules, output.modules);
    EXPECT_EQ(input.profile.samples, output.samples);
    EXPECT_EQ(input.profile.profile_duration, output.profile_duration);
    EXPECT_EQ(input.profile.sampling_period, output.sampling_period);
  }
}

}  // namespace metrics
