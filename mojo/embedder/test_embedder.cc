// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/embedder/test_embedder.h"

#include "base/logging.h"
#include "base/macros.h"
#include "mojo/system/core_impl.h"
#include "mojo/system/handle_table.h"

namespace mojo {

namespace system {
namespace internal {

bool ShutdownCheckNoLeaks(CoreImpl* core_impl) {
  // No point in taking the lock.
  const HandleTable::HandleToEntryMap& handle_to_entry_map =
      core_impl->handle_table_.handle_to_entry_map_;

  if (handle_to_entry_map.empty())
    return true;

  for (HandleTable::HandleToEntryMap::const_iterator it =
           handle_to_entry_map.begin();
       it != handle_to_entry_map.end();
       ++it) {
    LOG(ERROR) << "Mojo embedder shutdown: Leaking handle " << (*it).first;
  }
  return false;
}

}  // namespace internal
}  // namespace system

namespace embedder {
namespace test {

bool Shutdown() {
  system::CoreImpl* core_impl = static_cast<system::CoreImpl*>(Core::Get());
  CHECK(core_impl);
  Core::Reset();

  bool rv = system::internal::ShutdownCheckNoLeaks(core_impl);
  delete core_impl;
  return rv;
}

}  // namespace test
}  // namespace embedder

}  // namespace mojo
