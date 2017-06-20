// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_DATABASE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_DATABASE_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/affiliation_utils.h"

namespace base {
class FilePath;
}  // namespace base

namespace sql {
class Connection;
class Statement;
}  // namespace sql

namespace password_manager {

// Stores equivalence classes of facets, i.e., facets that are affiliated with
// each other, in an SQLite database. See affiliation_utils.h for a more
// detailed definition of what this means.
//
// Under the assumption that there is most likely not much the caller can do in
// case of database errors, most methods silently ignore them. Nevertheless, the
// caller must plan ahead for this rare but non-negligible scenario, and expect
// that in odd cases basic database invariants will not hold.
class AffiliationDatabase {
 public:
  AffiliationDatabase();
  ~AffiliationDatabase();

  // Opens an existing database at |path|, or creates a new one if none exists,
  // and returns true on success.
  bool Init(const base::FilePath& path);

  // Looks up the equivalence class containing |facet_uri|, and returns true if
  // such a class is found, in which case it is also stored into |result|.
  bool GetAffiliationsForFacet(const FacetURI& facet_uri,
                               AffiliatedFacetsWithUpdateTime* result) const;

  // Retrieves all stored equivalence classes.
  void GetAllAffiliations(
      std::vector<AffiliatedFacetsWithUpdateTime>* results) const;

  // Removes the stored equivalence class, if any, containing |facet_uri|.
  void DeleteAffiliationsForFacet(const FacetURI& facet_uri);

  // Removes stored equivalence classes that were last updated before the
  // |cutoff_threshold|.
  void DeleteAffiliationsOlderThan(const base::Time& cutoff_threshold);

  // Removes all records from all tables of the database.
  void DeleteAllAffiliations();

  // Stores the equivalence class defined by |affiliated_facets| to the DB and
  // returns true unless it has a non-empty subset with a preexisting class, in
  // which case no changes are made and the function returns false.
  bool Store(const AffiliatedFacetsWithUpdateTime& affiliated_facets);

  // Stores the equivalence class defined by |affiliated_facets| to the DB,
  // database, and removes any other equivalence classes that are in conflict
  // with |affiliated_facets|, i.e. those that are neither equal nor disjoint to
  // it. Removed equivalence classes are stored into |removed_affiliations|.
  void StoreAndRemoveConflicting(
      const AffiliatedFacetsWithUpdateTime& affiliated_facets,
      std::vector<AffiliatedFacetsWithUpdateTime>* removed_affiliations);

  // Deletes the database file at |path| along with all its auxiliary files. The
  // database must be closed before calling this.
  static void Delete(const base::FilePath& path);

 private:
  // Creates any tables and indices that do not already exist in the database.
  bool CreateTablesAndIndicesIfNeeded();

  // Called when SQLite encounters an error.
  void SQLErrorCallback(int error_number, sql::Statement* statement);

  // The SQL connection to the database.
  std::unique_ptr<sql::Connection> sql_connection_;

  DISALLOW_COPY_AND_ASSIGN(AffiliationDatabase);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_DATABASE_H_
