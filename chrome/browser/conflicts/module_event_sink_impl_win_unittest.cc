// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_event_sink_impl_win.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/common/conflicts/module_watcher_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The address of this module in memory. The linker will take care of defining
// this symbol.
extern "C" IMAGE_DOS_HEADER __ImageBase;

// An invalid load address.
const uint64_t kInvalidLoadAddress = 0xDEADBEEF;

}  // namespace

class ModuleEventSinkImplTest : public testing::Test {
 protected:
  ModuleEventSinkImplTest()
      : message_loop_(base::MakeUnique<base::MessageLoop>()),
        module_database_(
            base::MakeUnique<ModuleDatabase>(message_loop_->task_runner())) {}

  void CreateModuleSinkImpl() {
    module_event_sink_impl_ = base::MakeUnique<ModuleEventSinkImpl>(
        ::GetCurrentProcess(), content::PROCESS_TYPE_BROWSER,
        module_database_.get());
  }

  ModuleDatabase* module_database() {
    return module_event_sink_impl_->module_database_;
  }

  const ModuleDatabase::ModuleMap& modules() {
    return module_database_->modules_;
  }

  const ModuleDatabase::ProcessMap& processes() {
    return module_database_->processes_;
  }

  uint32_t process_id() { return module_event_sink_impl_->process_id_; }

  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<ModuleDatabase> module_database_;
  std::unique_ptr<ModuleEventSinkImpl> module_event_sink_impl_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ModuleEventSinkImplTest);
};

TEST_F(ModuleEventSinkImplTest, CallsForwardedAsExpected) {
  const uintptr_t kValidLoadAddress = reinterpret_cast<uintptr_t>(&__ImageBase);

  EXPECT_EQ(0u, modules().size());
  EXPECT_EQ(0u, processes().size());

  // Construction should immediately fire off a call to OnProcessStarted and
  // create a process entry in the module database.
  CreateModuleSinkImpl();
  EXPECT_EQ(module_database_.get(), module_database());
  EXPECT_EQ(::GetCurrentProcessId(), process_id());
  EXPECT_EQ(0u, modules().size());
  EXPECT_EQ(1u, processes().size());

  // An invalid load event should not cause a module entry.
  module_event_sink_impl_->OnModuleEvent(
      mojom::ModuleEventType::MODULE_ALREADY_LOADED, kInvalidLoadAddress);
  EXPECT_EQ(0u, modules().size());
  EXPECT_EQ(1u, processes().size());

  // A valid load event should cause a module entry.
  module_event_sink_impl_->OnModuleEvent(mojom::ModuleEventType::MODULE_LOADED,
                                         kValidLoadAddress);
  EXPECT_EQ(1u, modules().size());
  EXPECT_EQ(1u, processes().size());
}
