// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_QUERY_RESULT_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_QUERY_RESULT_MANAGER_H_

#include <map>
#include <set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/ui/webui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/webui/media_router/media_sink_with_cast_modes.h"

namespace media_router {

class MediaRouter;
class MediaSinksObserver;
struct RoutesQueryResult;
struct SinksQueryResult;

// The Media Router dialog allows the user to initiate casting using one of
// several actions (each represented by a cast mode).  Each cast mode is
// associated with a media source.  This class allows the dialog to receive
// lists of MediaSinks compatible with the cast modes available through the
// dialog.
//
// Typical use:
//
//   QueryResultManager::Observer* observer = ...;
//   QueryResultManager result_manager(router);
//   result_manager.AddObserver(observer);
//   result_manager.StartSinksQuery(MediaCastMode::DEFAULT,
//       MediaSourceForPresentationUrl("http://google.com"));
//   result_manager.StartSinksQuery(MediaCastMode::TAB_MIRROR,
//       MediaSourceForTab(123));
//   ...
//   [Updates will be received by observer via OnResultsUpdated()]
//   ...
//   [When info on MediaSource is needed, i.e. when requesting route for a mode]
//   CastModeSet cast_modes;
//   result_manager.GetSupportedCastModes(&cast_modes);
//   [Logic to select a MediaCastMode from the set]
//   MediaSource source = result_manager.GetSourceForCastMode(
//       MediaCastMode::TAB_MIRROR);
//   if (!source.Empty()) {
//     ...
//   }
//
// Not thread-safe.  Must be used on a single thread.
class QueryResultManager {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Updated results have been received.
    // |sinks|: List of sinks and the cast modes they are compatible with.
    virtual void OnResultsUpdated(
        const std::vector<MediaSinkWithCastModes>& sinks) = 0;
  };

  explicit QueryResultManager(MediaRouter* media_router);
  ~QueryResultManager();

  // Adds/removes an observer that is notified with query results.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Requests a list of MediaSinks compatible with |source| for |cast_mode|.
  // Results are sent to all observers registered with AddObserver().
  //
  // May start a new query in the Media Router for the registered source if
  // there is no existing query for it.  If there is an existing query for
  // |cast_mode|, it is stopped.
  //
  // If |source| is empty, no new queries are begun.
  void StartSinksQuery(MediaCastMode cast_mode, const MediaSource& source);

  // Stops notifying observers for |cast_mode|.
  void StopSinksQuery(MediaCastMode cast_mode);

  // Gets the set of cast modes that are being actively queried. |cast_mode_set|
  // should be empty.
  void GetSupportedCastModes(CastModeSet* cast_modes) const;

  // Returns the MediaSource registered for |cast_mode|.  Returns an empty
  // MediaSource if there is none.
  MediaSource GetSourceForCastMode(MediaCastMode cast_mode) const;

 private:
  class CastModeMediaSinksObserver;

  FRIEND_TEST_ALL_PREFIXES(QueryResultManagerTest, Observers);
  FRIEND_TEST_ALL_PREFIXES(QueryResultManagerTest, StartRoutesDiscovery);
  FRIEND_TEST_ALL_PREFIXES(QueryResultManagerTest, MultipleQueries);

  // Sets the media source for |cast_mode|.
  void SetSourceForCastMode(MediaCastMode cast_mode, const MediaSource& source);

  // Stops and destroys the MediaSinksObserver for |cast_mode|.
  void RemoveObserverForCastMode(MediaCastMode cast_mode);

  // Returns true if the |entry|'s sink is compatible with at least one cast
  // mode.
  bool IsValid(const MediaSinkWithCastModes& entry) const;

  // Modifies the current set of results with |result| associated with
  // |cast_mode|.
  void UpdateWithSinksQueryResult(MediaCastMode cast_mode,
                                  const std::vector<MediaSink>& result);

  // Notifies observers that results have been updated.
  void NotifyOnResultsUpdated();

  // MediaSinksObservers that listens for compatible MediaSink updates.
  // Each observer is associated with a MediaCastMode. Results received by
  // observers are propagated back to this class.
  // TODO(mfoltz): Remove linked_ptr when there is a ScopedPtrMap available.
  std::map<MediaCastMode, linked_ptr<MediaSinksObserver>> sinks_observers_;

  // Holds registrations of MediaSources for cast modes.
  std::map<MediaCastMode, MediaSource> cast_mode_sources_;

  // Holds all known sinks and their associated cast modes.
  std::map<MediaSink::Id, MediaSinkWithCastModes> all_sinks_;

  // Registered observers.
  base::ObserverList<Observer> observers_;

  // Not owned by this object.
  MediaRouter* router_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(QueryResultManager);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_QUERY_RESULT_MANAGER_H_
