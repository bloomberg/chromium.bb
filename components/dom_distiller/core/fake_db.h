// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_FAKE_DB_H_
#define COMPONENTS_DOM_DISTILLER_CORE_FAKE_DB_H_

#include "components/dom_distiller/core/dom_distiller_database.h"

namespace dom_distiller {
namespace test {

class FakeDB : public DomDistillerDatabaseInterface {
  typedef base::Callback<void(bool)> Callback;

 public:
  typedef base::hash_map<std::string, ArticleEntry> EntryMap;

  explicit FakeDB(EntryMap* db);
  virtual ~FakeDB();

  virtual void Init(const base::FilePath& database_dir,
                    DomDistillerDatabaseInterface::InitCallback callback)
      OVERRIDE;

  virtual void UpdateEntries(
      scoped_ptr<EntryVector> entries_to_save,
      scoped_ptr<EntryVector> entries_to_remove,
      DomDistillerDatabaseInterface::UpdateCallback callback) OVERRIDE;

  virtual void LoadEntries(DomDistillerDatabaseInterface::LoadCallback callback)
      OVERRIDE;
  base::FilePath& GetDirectory();

  void InitCallback(bool success);

  void LoadCallback(bool success);

  void UpdateCallback(bool success);

  static base::FilePath DirectoryForTestDB();

 private:
  static void RunLoadCallback(
      DomDistillerDatabaseInterface::LoadCallback callback,
      scoped_ptr<EntryVector> entries,
      bool success);

  base::FilePath dir_;
  EntryMap* db_;

  Callback init_callback_;
  Callback load_callback_;
  Callback update_callback_;
};

}  // namespace test
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_FAKE_DB_H_
