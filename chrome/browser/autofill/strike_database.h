// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_STRIKE_DATABASE_H_
#define CHROME_BROWSER_AUTOFILL_STRIKE_DATABASE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/leveldb_proto/proto_database.h"

namespace autofill {
class StrikeData;

// Manages data on whether different Autofill opportunities should be offered to
// the user. Projects can earn strikes in a number of ways; for instance, if a
// user ignores or declines a prompt, or if a user accepts a prompt but the task
// fails.
class StrikeDatabase {
 public:
  StrikeDatabase(const base::FilePath& database_dir);
  ~StrikeDatabase();

 private:
  void OnDatabaseInit(bool success);

  std::unique_ptr<leveldb_proto::ProtoDatabase<StrikeData>> db_;

  base::WeakPtrFactory<StrikeDatabase> weak_ptr_factory_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_STRIKE_DATABASE_H_
