// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_IO_DATA_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_IO_DATA_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/previews/content/previews_optimization_guide.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_black_list_delegate.h"
#include "components/previews/core/previews_decider.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_logger.h"
#include "net/nqe/effective_connection_type.h"

class GURL;

namespace net {
class URLRequest;
}

namespace previews {
class PreviewsOptOutStore;
class PreviewsUIService;

typedef base::Callback<bool(PreviewsType)> PreviewsIsEnabledCallback;

// A class to manage the IO portion of inter-thread communication between
// previews/ objects. Created on the UI thread, but used only on the IO thread
// after initialization.
class PreviewsIOData : public PreviewsDecider,
                       public PreviewsBlacklistDelegate {
 public:
  PreviewsIOData(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~PreviewsIOData() override;

  // PreviewsBlacklistDelegate:
  void OnNewBlacklistedHost(const std::string& host, base::Time time) override;
  void OnUserBlacklistedStatusChange(bool blacklisted) override;
  void OnBlacklistCleared(base::Time time) override;

  // Stores |previews_ui_service| as |previews_ui_service_| and posts a task to
  // InitializeOnIOThread on the IO thread.
  virtual void Initialize(
      base::WeakPtr<PreviewsUIService> previews_ui_service,
      std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store,
      const PreviewsIsEnabledCallback& is_enabled_callback);

  // Adds log message of the navigation asynchronously.
  void LogPreviewNavigation(const GURL& url,
                            bool opt_out,
                            PreviewsType type,
                            base::Time time) const;

  // Adds log message of preview decision made asynchronously.
  void LogPreviewDecisionMade(PreviewsEligibilityReason reason,
                              const GURL& url,
                              base::Time time,
                              PreviewsType type) const;

  // Adds a navigation to |url| to the black list with result |opt_out|.
  void AddPreviewNavigation(const GURL& url, bool opt_out, PreviewsType type);

  // Clears the history of the black list between |begin_time| and |end_time|,
  // both inclusive.
  void ClearBlackList(base::Time begin_time, base::Time end_time);

  // Change the status of whether to ignore the decisions made by
  // PreviewsBlackList to |ignored|. Virtualized in testing.
  virtual void SetIgnorePreviewsBlacklistDecision(bool ignored);

  // The previews black list that decides whether a navigation can use previews.
  PreviewsBlackList* black_list() const { return previews_black_list_.get(); }

  // PreviewsDecider implementation:
  bool ShouldAllowPreview(const net::URLRequest& request,
                          PreviewsType type) const override;
  bool ShouldAllowPreviewAtECT(
      const net::URLRequest& request,
      PreviewsType type,
      net::EffectiveConnectionType effective_connection_type_threshold,
      const std::vector<std::string>& host_blacklist_from_server)
      const override;

 protected:
  // Posts a task to SetIOData for |previews_ui_service_| on the UI thread with
  // a weak pointer to |this|. Virtualized for testing.
  virtual void InitializeOnIOThread(
      std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store);

  // Sets a blacklist for testing.
  void SetPreviewsBlacklistForTesting(
      std::unique_ptr<PreviewsBlackList> previews_back_list);

  // Sets an optimization guide for testing.
  void SetPreviewsOptimizationGuideForTesting(
      std::unique_ptr<PreviewsOptimizationGuide> previews_opt_guide);

 private:
  // The UI thread portion of the inter-thread communication for previews.
  base::WeakPtr<PreviewsUIService> previews_ui_service_;

  std::unique_ptr<PreviewsBlackList> previews_black_list_;

  // Holds optimization guidance from the server.
  std::unique_ptr<PreviewsOptimizationGuide> previews_opt_guide_;

  // Whether the decisions made by PreviewsBlackList should be ignored or not.
  // This can be changed by chrome://interventions-internals to test/debug the
  // behavior of Previews decisions.
  bool blacklist_ignored_;

  // The UI and IO thread task runners. |ui_task_runner_| is used to post
  // tasks to |previews_ui_service_|, and |io_task_runner_| is used to post from
  // Initialize to InitializeOnIOThread as well as verify that execution is
  // happening on the IO thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Whether the preview is enabled. Valid after Initialize() is called.
  PreviewsIsEnabledCallback is_enabled_callback_;

  base::WeakPtrFactory<PreviewsIOData> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsIOData);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_IO_DATA_H_
