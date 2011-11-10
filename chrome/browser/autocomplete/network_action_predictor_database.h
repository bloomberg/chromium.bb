// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_NETWORK_ACTION_PREDICTOR_DATABASE_H_
#define CHROME_BROWSER_AUTOCOMPLETE_NETWORK_ACTION_PREDICTOR_DATABASE_H_
#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "base/synchronization/cancellation_flag.h"
#include "googleurl/src/gurl.h"
#include "sql/connection.h"

class Profile;

// This manages the network action predictor table within the SQLite database
// passed in to the constructor. It expects the following scheme:
//
// network_action_predictor
//   id                 A unique id.
//   user_text          What the user typed.
//   url                The URL of the entry.
//   number_of_hits     Number of times the entry was shown to the user and
//                      selected.
//   number_of_misses   Number of times the entry was shown to the user but not
//                      selected.
//
// Ref-counted as it is created and destroyed on a different thread to the DB
// thread that is required for all methods performing database access.
class NetworkActionPredictorDatabase
    : public base::RefCountedThreadSafe<NetworkActionPredictorDatabase> {
 public:
  struct Row {
    // TODO(dominich): Make this 64-bit integer as an optimization. This
    // requires some investigation into how to make sure the id is unique for
    // each user_text/url pair.
    // http://crbug.com/102020
    typedef std::string Id;

    Row();

    // Only used by unit tests.
    Row(const Id& id,
        const string16& user_text,
        const GURL& url,
        int number_of_hits,
        int number_of_misses);

    Id id;
    string16 user_text;
    GURL url;
    int number_of_hits;
    int number_of_misses;
  };

  explicit NetworkActionPredictorDatabase(Profile* profile);

  // Opens the database file from the profile path. Separated from the
  // constructor to ease construction/destruction of this object on one thread
  // but database access on the DB thread.
  void Initialize();

  void GetRow(const Row::Id& id, Row* row);
  void GetAllRows(std::vector<Row>* row_buffer);

  void AddRow(const Row& row);
  void UpdateRow(const Row& row);
  void DeleteRow(const Row::Id& id);
  void DeleteRows(const std::vector<Row::Id>& id_list);
  void DeleteAllRows();

  // For batching database operations.
  void BeginTransaction();
  void CommitTransaction();

  void OnPredictorDestroyed();

 private:
  friend class NetworkActionPredictorDatabaseTest;
  friend class base::RefCountedThreadSafe<NetworkActionPredictorDatabase>;
  virtual ~NetworkActionPredictorDatabase();

  void CreateTable();

  // TODO(dominich): Consider adding this table to one of the history databases.
  // In memory is currently used, but adding to the on-disk visits database
  // would allow DeleteOldEntries to be cheaper through use of a join.
  FilePath db_path_;
  sql::Connection db_;

  // Set when the NetworkActionPredictor is destroyed so we can cancel any
  // posted database requests.
  base::CancellationFlag canceled_;

  DISALLOW_COPY_AND_ASSIGN(NetworkActionPredictorDatabase);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_NETWORK_ACTION_PREDICTOR_DATABASE_H_
