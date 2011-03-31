// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_AUTOFILL_MIGRATION_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_AUTOFILL_MIGRATION_H_
#pragma once

namespace syncable {
enum AutofillMigrationState {

  // Indicates the default state. After first run the state would change to
  // one of the following.
  NOT_DETERMINED,

  // The autofill profile is not migrated. Current sync should migrate the data
  // by syncing down the old autofill and syncing profiles back up to the server
  // as new autofill.
  NOT_MIGRATED,

  // We have migrated the autofill profile data. From now on autofill and
  // autofill profiles are 2 seperate data types.
  MIGRATED,

  // The autofill datatype is being synced new.(either because this is a new
  // client or the user just enabled them for syncing). In which case if
  // someother client had migrated the data already then our new state after
  // first sync would be MIGRATED. Else we would be responsible for migrating
  // the data.
  INSUFFICIENT_INFO_TO_DETERMINE
};

struct AutofillMigrationDebugInfo {
  enum PropertyToSet {
    MIGRATION_TIME,
    ENTRIES_ADDED,
    PROFILES_ADDED
  };
  int64 autofill_migration_time;
  // NOTE(akalin): We don't increment
  // |bookmarks_added_during_migration| anymore, although it's not
  // worth the effort to remove it from the code.  Eventually, this
  // will go away once we remove all the autofill migration code.
  int bookmarks_added_during_migration;
  int autofill_entries_added_during_migration;
  int autofill_profile_added_during_migration;
};

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_AUTOFILL_MIGRATION_H_

