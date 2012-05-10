// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_H_
#define CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_H_
#pragma once

#include <map>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_table.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"

struct AutocompleteLog;
struct AutocompleteMatch;
class AutocompleteResult;
class HistoryService;
class PredictorsHandler;
class Profile;

namespace history {
class URLDatabase;
}

namespace predictors {

// This class is responsible for determining the correct predictive network
// action to take given for a given AutocompleteMatch and entered text. it uses
// a AutocompleteActionPredictorTable accessed asynchronously on the DB thread
// to permanently store the data used to make predictions, and keeps local
// caches of that data to be able to make predictions synchronously on the UI
// thread where it lives. It can be accessed as a weak pointer so that it can
// safely use PostTaskAndReply without fear of crashes if it is destroyed before
// the reply triggers. This is necessary during initialization.
class AutocompleteActionPredictor
    : public ProfileKeyedService,
      public content::NotificationObserver,
      public base::SupportsWeakPtr<AutocompleteActionPredictor> {
 public:
  enum Action {
    ACTION_PRERENDER = 0,
    ACTION_PRECONNECT,
    ACTION_NONE,
    LAST_PREDICT_ACTION = ACTION_NONE
  };

  explicit AutocompleteActionPredictor(Profile* profile);
  virtual ~AutocompleteActionPredictor();

  static void set_hit_weight(double weight) { hit_weight_ = weight; }
  static double get_hit_weight() { return hit_weight_; }

  // Registers an AutocompleteResult for a given |user_text|. This will be used
  // when the user navigates from the Omnibox to determine early opportunities
  // to predict their actions.
  void RegisterTransitionalMatches(const string16& user_text,
                                   const AutocompleteResult& result);

  // Clears any transitional matches that have been registered. Called when, for
  // example, the AutocompleteEditModel is reverted.
  void ClearTransitionalMatches();

  // Return the recommended action given |user_text|, the text the user has
  // entered in the Omnibox, and |match|, the suggestion from Autocomplete.
  // This method uses information from the ShortcutsBackend including how much
  // of the matching entry the user typed, and how long it's been since the user
  // visited the matching URL, to calculate a score between 0 and 1. This score
  // is then mapped to an Action.
  Action RecommendAction(const string16& user_text,
                         const AutocompleteMatch& match) const;

  // Return true if the suggestion type warrants a TCP/IP preconnection.
  // i.e., it is now quite likely that the user will select the related domain.
  static bool IsPreconnectable(const AutocompleteMatch& match);

 private:
  friend class AutocompleteActionPredictorTest;
  friend class ::PredictorsHandler;

  struct TransitionalMatch {
    TransitionalMatch();
    ~TransitionalMatch();

    string16 user_text;
    std::vector<GURL> urls;

    bool operator==(const string16& other_user_text) const {
      return user_text == other_user_text;
    }
  };

  struct DBCacheKey {
    string16 user_text;
    GURL url;

    bool operator<(const DBCacheKey& rhs) const {
      return (user_text != rhs.user_text) ?
          (user_text < rhs.user_text) :  (url < rhs.url);
    }

    bool operator==(const DBCacheKey& rhs) const {
      return (user_text == rhs.user_text) && (url == rhs.url);
    }
  };

  struct DBCacheValue {
    int number_of_hits;
    int number_of_misses;
  };

  typedef std::map<DBCacheKey, DBCacheValue> DBCacheMap;
  typedef std::map<DBCacheKey, AutocompleteActionPredictorTable::Row::Id>
      DBIdCacheMap;

  static const int kMaximumDaysToKeepEntry;

  // Multiplying factor applied to the |number_of_hits| for a database entry
  // when calculating the confidence. It is currently set by a field trial so is
  // static. Once the field trial ends, this will be a constant value.
  static double hit_weight_;

  // NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Called when NOTIFICATION_OMNIBOX_OPENED_URL is observed.
  void OnOmniboxOpenedUrl(const AutocompleteLog& log);

  // Deletes any old or invalid entries from the local caches. |url_db| and
  // |id_list| must not be NULL. Every row id deleted will be added to id_list.
  void DeleteOldIdsFromCaches(
      history::URLDatabase* url_db,
      std::vector<AutocompleteActionPredictorTable::Row::Id>* id_list);

  // Called to delete any old or invalid entries from the database. Called after
  // the local caches are created once the history service is available.
  void DeleteOldEntries(history::URLDatabase* url_db);

  // Called to populate the local caches. This also calls DeleteOldEntries
  // if the history service is available, or registers for the notification of
  // it becoming available.
  void CreateCaches(
      std::vector<AutocompleteActionPredictorTable::Row>* row_buffer);

  // Attempts to call DeleteOldEntries if the in-memory database has been loaded
  // by |service|. Returns success as a boolean.
  bool TryDeleteOldEntries(HistoryService* service);

  // Uses local caches to calculate an exact percentage prediction that the user
  // will take a particular match given what they have typed. |is_in_db| is set
  // to differentiate trivial zero results resulting from a match not being
  // found from actual zero results where the calculation returns 0.0.
  double CalculateConfidence(const string16& user_text,
                             const AutocompleteMatch& match,
                             bool* is_in_db) const;

  // Calculates the confidence for an entry in the DBCacheMap.
  double CalculateConfidenceForDbEntry(DBCacheMap::const_iterator iter) const;

  // Adds and updates rows in the database and caches.
  void AddAndUpdateRows(
    const AutocompleteActionPredictorTable::Rows& rows_to_add,
    const AutocompleteActionPredictorTable::Rows& rows_to_update);

  // Removes all rows from the database and caches.
  void DeleteAllRows();

  // Removes rows from the database and caches that contain a URL in |rows|.
  void DeleteRowsWithURLs(const history::URLRows& rows);

  Profile* profile_;
  scoped_refptr<AutocompleteActionPredictorTable> table_;
  content::NotificationRegistrar notification_registrar_;

  // This is cleared after every Omnibox navigation.
  std::vector<TransitionalMatch> transitional_matches_;

  // This allows us to predict the effect of confidence threshold changes on
  // accuracy.
  mutable std::vector<std::pair<GURL, double> > tracked_urls_;

  DBCacheMap db_cache_;
  DBIdCacheMap db_id_cache_;

  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteActionPredictor);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_H_
