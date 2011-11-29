// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/cleanup_disabled_types_command.h"

#include <algorithm>

#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/sessions/sync_session_context.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace browser_sync {

CleanupDisabledTypesCommand::CleanupDisabledTypesCommand() {}
CleanupDisabledTypesCommand::~CleanupDisabledTypesCommand() {}

void CleanupDisabledTypesCommand::ExecuteImpl(sessions::SyncSession* session) {
  using syncable::ModelTypeSet;
  using syncable::ModelTypeSetToString;

  const ModelTypeSet& all_types = syncable::GetAllRealModelTypes();
  const ModelTypeSet& enabled_types =
      GetRoutingInfoTypes(session->routing_info());

  // Initially set |to_cleanup| to |all_types| \setminus |enabled_types|.
  ModelTypeSet to_cleanup;
  std::set_difference(all_types.begin(), all_types.end(),
                      enabled_types.begin(), enabled_types.end(),
                      std::inserter(to_cleanup, to_cleanup.end()));

  DVLOG(1) << "all_types = " << ModelTypeSetToString(all_types)
           << ", enabled_types = " << ModelTypeSetToString(enabled_types)
           << ", to_cleanup = " << ModelTypeSetToString(to_cleanup);

  // Because a full directory purge is slow, we avoid purging
  // undesired types unless we have reason to believe they were
  // previously enabled.  Because purging could theoretically fail on
  // the first sync session (when there's no previous routing info) we
  // pay the full directory scan price once and do a "deep clean" of
  // types that may potentially need cleanup so that we converge to
  // the correct state.
  //
  //                          in_previous  |   !in_previous
  //                                       |
  //   initial_sync_ended     should clean |  may have attempted cleanup
  //  !initial_sync_ended     should clean |  may have never been enabled, or
  //                                       |  could have been disabled before
  //                                       |  initial sync ended and cleanup
  //                                       |  may not have happened yet
  //                                       |  (failure, browser restart
  //                                       |  before another sync session,..)

  const ModelTypeSet& previous_enabled_types =
      GetRoutingInfoTypes(
          session->context()->previous_session_routing_info());
  // If |previous_enabled_types| is non-empty (i.e., not the first
  // sync session), set |to_cleanup| to its intersection with
  // |previous_enabled_types|.
  if (!previous_enabled_types.empty()) {
    using std::swap;
    ModelTypeSet temp;
    std::set_intersection(
        to_cleanup.begin(), to_cleanup.end(),
        previous_enabled_types.begin(), previous_enabled_types.end(),
        std::inserter(temp, temp.end()));
    swap(to_cleanup, temp);
  }

  DVLOG(1) << "previous_enabled_types = "
           << ModelTypeSetToString(previous_enabled_types)
           << ", to_cleanup = " << ModelTypeSetToString(to_cleanup);

  if (to_cleanup.empty())
    return;

  syncable::ScopedDirLookup dir(session->context()->directory_manager(),
                                session->context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }

  dir->PurgeEntriesWithTypeIn(to_cleanup);
}

}  // namespace browser_sync

