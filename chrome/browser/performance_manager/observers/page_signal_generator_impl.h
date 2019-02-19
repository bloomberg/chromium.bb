// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_PAGE_SIGNAL_GENERATOR_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_PAGE_SIGNAL_GENERATOR_IMPL_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/resource_coordinator/public/mojom/page_signal.mojom.h"

namespace service_manager {
struct BindSourceInfo;
}  // namespace service_manager

namespace performance_manager {

// The PageSignalGenerator is a dedicated |GraphObserver| for
// calculating and emitting page-scoped signals. This observer observes
// PageCoordinationUnits, ProcessCoordinationUnits and FrameNodes,
// combining information from the graph to generate page level signals.
class PageSignalGeneratorImpl
    : public GraphObserver,
      public resource_coordinator::mojom::PageSignalGenerator {
 public:
  PageSignalGeneratorImpl();
  ~PageSignalGeneratorImpl() override;

  // resource_coordinator::mojom::PageSignalGenerator implementation.
  void AddReceiver(
      resource_coordinator::mojom::PageSignalReceiverPtr receiver) override;

  // GraphObserver implementation.
  bool ShouldObserve(const NodeBase* coordination_unit) override;
  void OnNodeCreated(NodeBase* cu) override;
  void OnBeforeNodeDestroyed(NodeBase* cu) override;
  void OnFramePropertyChanged(
      FrameNodeImpl* frame_cu,
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) override;
  void OnPagePropertyChanged(
      PageNodeImpl* page_cu,
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) override;
  void OnProcessPropertyChanged(
      ProcessNodeImpl* process_cu,
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) override;
  void OnFrameEventReceived(FrameNodeImpl* frame_cu,
                            resource_coordinator::mojom::Event event) override;
  void OnPageEventReceived(PageNodeImpl* page_cu,
                           resource_coordinator::mojom::Event event) override;
  void OnProcessEventReceived(
      ProcessNodeImpl* page_cu,
      resource_coordinator::mojom::Event event) override;
  void OnSystemEventReceived(SystemNodeImpl* system_cu,
                             resource_coordinator::mojom::Event event) override;

  void BindToInterface(
      resource_coordinator::mojom::PageSignalGeneratorRequest request,
      const service_manager::BindSourceInfo& source_info);

 private:
  // The amount of time a page has to be idle post-loading in order for it to be
  // considered loaded and idle. This is used in UpdateLoadIdleState
  // transitions.
  static const base::TimeDelta kLoadedAndIdlingTimeout;

  // The maximum amount of time post-DidStopLoading a page can be waiting for
  // an idle state to occur before the page is simply considered loaded anyways.
  // Since PageAlmostIdle is intended as an "initial loading complete" signal,
  // it needs to eventually terminate. This is strictly greater than the
  // kLoadedAndIdlingTimeout.
  static const base::TimeDelta kWaitingForIdleTimeout;

  friend class PageSignalGeneratorImplTest;
  FRIEND_TEST_ALL_PREFIXES(PageSignalGeneratorImplTest, IsLoading);
  FRIEND_TEST_ALL_PREFIXES(PageSignalGeneratorImplTest, IsIdling);
  FRIEND_TEST_ALL_PREFIXES(PageSignalGeneratorImplTest,
                           NonPersistentNotificationCreatedEvent);
  FRIEND_TEST_ALL_PREFIXES(PageSignalGeneratorImplTest,
                           PageDataCorrectlyManaged);
  FRIEND_TEST_ALL_PREFIXES(PageSignalGeneratorImplTest,
                           PageAlmostIdleTransitionsNoTimeout);
  FRIEND_TEST_ALL_PREFIXES(PageSignalGeneratorImplTest,
                           PageAlmostIdleTransitionsWithTimeout);
  FRIEND_TEST_ALL_PREFIXES(PageSignalGeneratorImplTest,
                           OnLoadTimePerformanceEstimate);

  // The state transitions for the PageAlmostIdle signal. In general a page
  // transitions through these states from top to bottom.
  enum LoadIdleState {
    // The initial state. Can only transition to kLoading from here.
    kLoadingNotStarted,
    // Loading has started. Almost idle signals are ignored in this state.
    // Can transition to kLoadedNotIdling and kLoadedAndIdling from here.
    kLoading,
    // Loading has completed, but the page has not started idling. Can only
    // transition to kLoadedAndIdling from here.
    kLoadedNotIdling,
    // Loading has completed, and the page is idling. Can transition to
    // kLoadedNotIdling or kLoadedAndIdle from here.
    kLoadedAndIdling,
    // Loading has completed and the page has been idling for sufficiently long.
    // This is the final state. Once this state has been reached a signal will
    // be emitted and no further state transitions will be tracked. Committing a
    // new non-same document navigation can start the cycle over again.
    kLoadedAndIdle
  };

  // Holds state per page CU. These are created via OnNodeCreated
  // and destroyed via OnBeforeNodeDestroyed.
  struct PageData {
    // Set the load idle state and the time of change. Also clears the
    // |performance_estimate_issued| flag.
    void SetLoadIdleState(LoadIdleState new_state, base::TimeTicks now);
    LoadIdleState GetLoadIdleState() const { return load_idle_state; }

    // Marks the point in time when the DidStopLoading signal was received,
    // transitioning to kLoadedAndNotIdling or kLoadedAndIdling. This is used as
    // the basis for the kWaitingForIdleTimeout.
    base::TimeTicks loading_stopped;
    // Marks the point in time when the last transition to kLoadedAndIdling
    // occurred. Used for gating the transition to kLoadedAndIdle.
    base::TimeTicks idling_started;
    // Notes the time of the last state change.
    base::TimeTicks last_state_change;
    // True iff a performance estimate has been issued for this page.
    bool performance_estimate_issued = false;
    // A one-shot timer used for transitioning between kLoadedAndIdling and
    // kLoadedAndIdle.
    base::OneShotTimer idling_timer;

   private:
    // Initially at kLoadingNotStarted. Transitions through the states via calls
    // to UpdateLoadIdleState. Is reset to kLoadingNotStarted when a non-same
    // document navigation is committed.
    LoadIdleState load_idle_state;
  };

  // These are called when properties/events affecting the load-idle state are
  // observed. Frame and Process variants will eventually all redirect to the
  // appropriate Page variant, where the real work is done.
  void UpdateLoadIdleStateFrame(const FrameNodeImpl* frame_cu);
  void UpdateLoadIdleStatePage(const PageNodeImpl* page_cu);
  void UpdateLoadIdleStateProcess(const ProcessNodeImpl* process_cu);

  // This method is called when a property affecting the lifecycle state is
  // observed.
  void UpdateLifecycleState(const PageNodeImpl* page_cu,
                            resource_coordinator::mojom::LifecycleState state);

  // Helper function for transitioning to the final state.
  void TransitionToLoadedAndIdle(const PageNodeImpl* page_cu,
                                 base::TimeTicks now);

  // Convenience accessors for state associated with a |page_cu|.
  PageData* GetPageData(const PageNodeImpl* page_cu);
  bool IsLoading(const PageNodeImpl* page_cu);
  bool IsIdling(const PageNodeImpl* page_cu);

  template <typename Method, typename... Params>
  void DispatchPageSignal(const PageNodeImpl* page_cu,
                          Method m,
                          Params... params);

  mojo::BindingSet<resource_coordinator::mojom::PageSignalGenerator> bindings_;
  mojo::InterfacePtrSet<resource_coordinator::mojom::PageSignalReceiver>
      receivers_;

  // Stores per Page CU data. This set is maintained by
  // OnNodeCreated and OnBeforeNodeDestroyed.
  std::map<const PageNodeImpl*, PageData> page_data_;

  DISALLOW_COPY_AND_ASSIGN(PageSignalGeneratorImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_PAGE_SIGNAL_GENERATOR_IMPL_H_
