// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_IMPL_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_IMPL_H_

#include <stdint.h>

#include <queue>
#include <set>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/common/features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ppapi/features/features.h"
#include "storage/common/quota/quota_types.h"
#include "url/gurl.h"

class BrowsingDataRemoverFactory;

namespace content {
class BrowserContext;
class BrowsingDataFilterBuilder;
class StoragePartition;
}

class BrowsingDataRemoverImpl :
    public BrowsingDataRemover,
    public KeyedService {
 public:
  // The completion inhibitor can artificially delay completion of the browsing
  // data removal process. It is used during testing to simulate scenarios in
  // which the deletion stalls or takes a very long time.
  class CompletionInhibitor {
   public:
    // Invoked when a |remover| is just about to complete clearing browser data,
    // and will be prevented from completing until after the callback
    // |continue_to_completion| is run.
    virtual void OnBrowsingDataRemoverWouldComplete(
        BrowsingDataRemoverImpl* remover,
        const base::Closure& continue_to_completion) = 0;

   protected:
    virtual ~CompletionInhibitor() {}
  };

  // Used to track the deletion of a single data storage backend.
  class SubTask {
   public:
    // Creates a SubTask that calls |forward_callback| when completed.
    // |forward_callback| is only kept as a reference and must outlive SubTask.
    explicit SubTask(const base::Closure& forward_callback);
    ~SubTask();

    // Indicate that the task is in progress and we're waiting.
    void Start();

    // Returns a callback that should be called to indicate that the task
    // has been finished.
    base::Closure GetCompletionCallback();

    // Whether the task is still in progress.
    bool is_pending() const { return is_pending_; }

   private:
    void CompletionCallback();

    bool is_pending_;
    const base::Closure& forward_callback_;
    base::WeakPtrFactory<SubTask> weak_ptr_factory_;
  };

  // Is the BrowsingDataRemoverImpl currently in the process of removing data?
  bool is_removing() { return is_removing_; }

  // Sets a CompletionInhibitor, which will be notified each time an instance is
  // about to complete a browsing data removal process, and will be able to
  // artificially delay the completion.
  // TODO(crbug.com/483528): Make this non-static.
  static void set_completion_inhibitor_for_testing(
      CompletionInhibitor* inhibitor) {
    completion_inhibitor_ = inhibitor;
  }

  // BrowsingDataRemover implementation:
  void SetEmbedderDelegate(
      std::unique_ptr<BrowsingDataRemoverDelegate> embedder_delegate) override;
  BrowsingDataRemoverDelegate* GetEmbedderDelegate() const override;

  void Remove(const base::Time& delete_begin,
              const base::Time& delete_end,
              int remove_mask,
              int origin_type_mask) override;
  void RemoveAndReply(const base::Time& delete_begin,
                      const base::Time& delete_end,
                      int remove_mask,
                      int origin_type_mask,
                      Observer* observer) override;
  void RemoveWithFilter(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder)
      override;
  void RemoveWithFilterAndReply(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
      Observer* observer) override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  const base::Time& GetLastUsedBeginTime() override;
  const base::Time& GetLastUsedEndTime() override;
  int GetLastUsedRemovalMask() override;
  int GetLastUsedOriginTypeMask() override;

  // Used for testing.
  void OverrideStoragePartitionForTesting(
      content::StoragePartition* storage_partition);

 protected:
  // Use BrowsingDataRemoverFactory::GetForBrowserContext to get an instance of
  // this class. The constructor is protected so that the class is mockable.
  explicit BrowsingDataRemoverImpl(content::BrowserContext* browser_context);
  ~BrowsingDataRemoverImpl() override;

  // A common reduction of all public Remove[WithFilter][AndReply] methods.
  virtual void RemoveInternal(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
      Observer* observer);

 private:
  // Testing the private RemovalTask.
  FRIEND_TEST_ALL_PREFIXES(BrowsingDataRemoverImplTest, MultipleTasks);

  friend class BrowsingDataRemoverFactory;

  // Represents a single removal task. Contains all parameters needed to execute
  // it and a pointer to the observer that added it.
  struct RemovalTask {
    RemovalTask(
        const base::Time& delete_begin,
        const base::Time& delete_end,
        int remove_mask,
        int origin_type_mask,
        std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
        Observer* observer);
    ~RemovalTask();

    base::Time delete_begin;
    base::Time delete_end;
    int remove_mask;
    int origin_type_mask;
    std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder;
    Observer* observer;
  };

  void Shutdown() override;

  // Setter for |is_removing_|; DCHECKs that we can only start removing if we're
  // not already removing, and vice-versa.
  void SetRemoving(bool is_removing);

  // Executes the next removal task. Called after the previous task was finished
  // or directly from Remove() if the task queue was empty.
  void RunNextTask();

  // Removes the specified items related to browsing for a specific host. If the
  // provided |remove_url| is empty, data is removed for all origins; otherwise,
  // it is restricted by the origin filter origin (where implemented yet). The
  // |origin_type_mask| parameter defines the set of origins from which data
  // should be removed (protected, unprotected, or both).
  // TODO(ttr314): Remove "(where implemented yet)" constraint above once
  // crbug.com/113621 is done.
  // TODO(crbug.com/589586): Support all backends w/ origin filter.
  void RemoveImpl(const base::Time& delete_begin,
                  const base::Time& delete_end,
                  int remove_mask,
                  const content::BrowsingDataFilterBuilder& filter_builder,
                  int origin_type_mask);

  // Notifies observers and transitions to the idle state.
  void Notify();

  // Checks if we are all done, and if so, calls Notify().
  void NotifyIfDone();

  // Returns true if we're all done.
  bool AllDone();

  // The browser context we're to remove from.
  content::BrowserContext* browser_context_;

  // A delegate to delete the embedder-specific data.
  std::unique_ptr<BrowsingDataRemoverDelegate> embedder_delegate_;

  // Start time to delete from.
  base::Time delete_begin_;

  // End time to delete to.
  base::Time delete_end_;

  // The removal mask for the current removal operation.
  int remove_mask_ = 0;

  // From which types of origins should we remove data?
  int origin_type_mask_ = 0;

  // True if Remove has been invoked.
  bool is_removing_;

  // Removal tasks to be processed.
  std::queue<RemovalTask> task_queue_;

  // If non-NULL, the |completion_inhibitor_| is notified each time an instance
  // is about to complete a browsing data removal process, and has the ability
  // to artificially delay completion. Used for testing.
  static CompletionInhibitor* completion_inhibitor_;

  // A callback to NotifyIfDone() used by SubTasks instances.
  const base::Closure sub_task_forward_callback_;

  // Keeping track of various subtasks to be completed.
  // These may only be accessed from UI thread in order to avoid races!
  SubTask synchronous_clear_operations_;
  SubTask clear_embedder_data_;
  SubTask clear_cache_;
  SubTask clear_channel_ids_;
  SubTask clear_http_auth_cache_;
  SubTask clear_storage_partition_data_;

  // Observers of the global state and individual tasks.
  base::ObserverList<Observer, true> observer_list_;

  // We do not own this.
  content::StoragePartition* storage_partition_for_testing_ = nullptr;

  base::WeakPtrFactory<BrowsingDataRemoverImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverImpl);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_IMPL_H_
