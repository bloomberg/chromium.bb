// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_SESSIONS_MOCK_DEBUG_INFO_GETTER_H_
#define COMPONENTS_SYNC_TEST_SESSIONS_MOCK_DEBUG_INFO_GETTER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sync/base/sync_export.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/sessions_impl/debug_info_getter.h"

namespace syncer {
namespace sessions {

// A mock implementation of DebugInfoGetter to be used in tests. Events added by
// AddDebugEvent are accessible via DebugInfoGetter methods.
class MockDebugInfoGetter : public sessions::DebugInfoGetter {
 public:
  MockDebugInfoGetter();
  ~MockDebugInfoGetter() override;

  // DebugInfoGetter implementation.
  void GetDebugInfo(sync_pb::DebugInfo* debug_info) override;
  void ClearDebugInfo() override;

  void AddDebugEvent();

 private:
  sync_pb::DebugInfo debug_info_;

  DISALLOW_COPY_AND_ASSIGN(MockDebugInfoGetter);
};

}  // namespace sessions
}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_SESSIONS_MOCK_DEBUG_INFO_GETTER_H_
