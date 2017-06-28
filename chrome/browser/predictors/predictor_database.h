// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PREDICTOR_DATABASE_H_
#define CHROME_BROWSER_PREDICTORS_PREDICTOR_DATABASE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace sql {
class Connection;
}

namespace predictors {

class AutocompleteActionPredictorTable;
class PredictorDatabaseInternal;
class ResourcePrefetchPredictorTables;

class PredictorDatabase : public KeyedService {
 public:
  explicit PredictorDatabase(Profile* profile);
  ~PredictorDatabase() override;

  scoped_refptr<AutocompleteActionPredictorTable> autocomplete_table();
  scoped_refptr<ResourcePrefetchPredictorTables> resource_prefetch_tables();

  // Used for testing.
  sql::Connection* GetDatabase();

 private:
  // KeyedService
  void Shutdown() override;

  scoped_refptr<PredictorDatabaseInternal> db_;

  DISALLOW_COPY_AND_ASSIGN(PredictorDatabase);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PREDICTOR_DATABASE_H_
