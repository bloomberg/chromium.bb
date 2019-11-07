// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_REGISTRY_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "chrome/services/app_service/public/cpp/instance.h"
#include "chrome/services/app_service/public/cpp/instance_update.h"

namespace aura {
class Window;
}

namespace apps {

// InstanceRegistry keeps all of the Instances seen by AppServiceProxy.
// It also keeps the "sum" of those previous deltas, so that observers of this
// object can be updated with the InstanceUpdate structure. It can also be
// queried synchronously.
//
// This class is not thread-safe.
class InstanceRegistry {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // The InstanceUpdate argument shouldn't be accessed after OnInstanceUpdate
    // returns.
    virtual void OnInstanceUpdate(const InstanceUpdate& update) = 0;

    // Called when the InstanceRegistry object (the thing that this observer
    // observes) will be destroyed. In response, the observer, |this|, should
    // call "instance_registry->RemoveObserver(this)", whether directly or
    // indirectly (e.g. via ScopedObserver::Remove or via Observe(nullptr)).
    virtual void OnInstanceRegistryWillBeDestroyed(InstanceRegistry* cache) = 0;

   protected:
    // Use this constructor when the observer |this| is tied to a single
    // InstanceRegistry for its entire lifetime, or until the observee (the
    // InstanceRegistry) is destroyed, whichever comes first.
    explicit Observer(InstanceRegistry* cache);

    // Use this constructor when the observer |this| wants to observe a
    // InstanceRegistry for part of its lifetime. It can then call Observe() to
    // start and stop observing.
    Observer();

    ~Observer() override;

    // Start observing a different InstanceRegistry. |instance_registry| may be
    // nullptr, meaning to stop observing.
    void Observe(InstanceRegistry* instance_registry);

   private:
    InstanceRegistry* instance_registry_ = nullptr;
  };

  InstanceRegistry();
  ~InstanceRegistry();

  InstanceRegistry(const InstanceRegistry&) = delete;
  InstanceRegistry& operator=(const InstanceRegistry&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  using InstancePtr = std::unique_ptr<Instance>;
  using Instances = std::vector<InstancePtr>;

  // Notification and merging might be delayed until after OnInstances returns.
  // For example, suppose that the initial set of states is (a0, b0, c0) for
  // three app_id's ("a", "b", "c"). Now suppose OnInstances is called with two
  // updates (b1, c1), and when notified of b1, an observer calls OnInstances
  // again with (c2, d2). The c1 delta should be processed before the c2 delta,
  // as it was sent first: c2 should be merged (with "newest wins" semantics)
  // onto c1 and not vice versa. This means that processing c2 (scheduled by the
  // second OnInstances call) should wait until the first OnInstances call has
  // finished processing b1 (and then c1), which means that processing c2 is
  // delayed until after the second OnInstances call returns.
  //
  // The caller presumably calls OnInstances(std::move(deltas)).
  void OnInstances(const Instances& deltas);

  // Calls f, a void-returning function whose arguments are (const
  // apps::InstanceUpdate&), on each window in the instance_registry.
  //
  // f's argument is an apps::InstanceUpdate instead of an Instance* so that
  // callers can more easily share code with Observer::OnInstanceUpdate (which
  // also takes an apps::InstanceUpdate), and an apps::InstanceUpdate also has a
  // StateIsNull method.
  //
  // The apps::InstanceUpdate argument to f shouldn't be accessed after f
  // returns.
  //
  // f must be synchronous, and if it asynchronously calls ForEachInstance
  // again, it's not guaranteed to see a consistent state.
  template <typename FunctionType>
  void ForEachInstance(FunctionType f) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

    for (const auto& s_iter : states_) {
      apps::Instance* state = s_iter.second.get();

      auto d_iter = deltas_in_progress_.find(s_iter.first);
      apps::Instance* delta =
          (d_iter != deltas_in_progress_.end()) ? d_iter->second : nullptr;

      f(apps::InstanceUpdate(state, delta));
    }

    for (const auto& d_iter : deltas_in_progress_) {
      apps::Instance* delta = d_iter.second;

      auto s_iter = states_.find(d_iter.first);
      if (s_iter != states_.end()) {
        continue;
      }

      f(apps::InstanceUpdate(nullptr, delta));
    }
  }

  // Calls f, a void-returning function whose arguments are (const
  // apps::InstanceUpdate&), on the instance in the instance_registry with the
  // given window. It will return true (and call f) if there is such an
  // instance, otherwise it will return false (and not call f). The
  // InstanceUpdate argument to f has the same semantics as for ForEachInstance,
  // above.
  //
  // f must be synchronous, and if it asynchronously calls ForOneInstance again,
  // it's not guaranteed to see a consistent state.
  template <typename FunctionType>
  bool ForOneInstance(const aura::Window* window, FunctionType f) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

    auto s_iter = states_.find(window);
    apps::Instance* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;

    auto d_iter = deltas_in_progress_.find(window);
    apps::Instance* delta =
        (d_iter != deltas_in_progress_.end()) ? d_iter->second : nullptr;

    if (state || delta) {
      f(apps::InstanceUpdate(state, delta));
      return true;
    }
    return false;
  }

 private:
  void DoOnInstances(const Instances& deltas);

  base::ObserverList<Observer> observers_;

  // Maps from window to the latest state: the "sum" of all previous deltas.
  std::map<const aura::Window*, InstancePtr> states_;

  // Track the deltas being processed or are about to be processed by
  // OnInstances. They are separate to manage the "notification and merging
  // might be delayed until after OnInstances returns" concern described above.
  //
  // OnInstances calls DoOnInstances zero or more times. If we're nested, so
  // that there's multiple OnInstances call to this InstanceRegistry in the call
  // stack, the deeper OnInstances call simply adds work to deltas_pending_ and
  // returns without calling DoOnInstances. If we're not nested, OnInstances
  // calls DoOnInstances one or more times; "more times" happens if
  // DoOnInstances notifying observers leads to more OnInstances calls that
  // enqueue deltas_pending_ work. The deltas_in_progress_ map (keyed by window)
  // contains those deltas being considered by DoOnInstances.
  //
  // Nested OnInstances calls are expected to be rare (but still dealt with
  // sensibly). In the typical case, OnInstances should call DoOnInstances
  // exactly once, and deltas_pending_ will stay empty.
  std::map<const aura::Window*, Instance*> deltas_in_progress_;
  Instances deltas_pending_;

  SEQUENCE_CHECKER(my_sequence_checker_);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_REGISTRY_H_
