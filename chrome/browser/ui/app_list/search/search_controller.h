// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/speech_ui_model_observer.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {

class History;
class SearchBoxModel;
class SearchProvider;
class SearchResult;
class SpeechUIModel;

// Controller that collects query from given SearchBoxModel, dispatches it
// to all search providers, then invokes the mixer to mix and to publish the
// results to the given SearchResults UI model.
class SearchController : public SpeechUIModelObserver {
 public:
  SearchController(Profile* profile,
                   SearchBoxModel* search_box,
                   AppListModel::SearchResults* results,
                   SpeechUIModel* speech_ui,
                   AppListControllerDelegate* list_controller);
  virtual ~SearchController();

  void Init();

  void Start();
  void Stop();

  void OpenResult(SearchResult* result, int event_flags);
  void InvokeResultAction(SearchResult* result,
                          int action_index,
                          int event_flags);

 private:
  typedef ScopedVector<SearchProvider> Providers;

  // Takes ownership of |provider| and associates it with given mixer group.
  void AddProvider(Mixer::GroupId group,
                   scoped_ptr<SearchProvider> provider);

  // Invoked when the search results are changed.
  void OnResultsChanged();

  // SpeechUIModelObserver overrides:
  virtual void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) OVERRIDE;

  Profile* profile_;
  SearchBoxModel* search_box_;
  SpeechUIModel* speech_ui_;
  AppListControllerDelegate* list_controller_;

  bool dispatching_query_;
  Providers providers_;
  scoped_ptr<Mixer> mixer_;
  History* history_;  // KeyedService, not owned.

  base::OneShotTimer<SearchController> stop_timer_;

  DISALLOW_COPY_AND_ASSIGN(SearchController);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_
