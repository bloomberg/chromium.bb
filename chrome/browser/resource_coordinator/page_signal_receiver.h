// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_PAGE_SIGNAL_RECEIVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_PAGE_SIGNAL_RECEIVER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/mojom/page_signal.mojom.h"

namespace content {
class WebContents;
}

namespace resource_coordinator {

// A PageSignalObserver is implemented to receive notifications from
// PageSignalReceiver by adding itself to PageSignalReceiver.
class PageSignalObserver {
 public:
  virtual ~PageSignalObserver() = default;
  // PageSignalReceiver will deliver signals with a |web_contents| even it's not
  // managed by the client. Thus the clients are responsible for checking the
  // passed |web_contents| by themselves.
  // Also note that because these event notifications are asynchronous to
  // navigation, it's possible that |web_contents| has navigated to another site
  // by the time the notification arrives.
  // The |page_navigation_id| parameter allows comparing the navigation_id
  // against the current navigation handle of |web_contents|, and the url
  // in the struct can be used post-navigation to rendezvous to the origin
  // the event relates to.
  virtual void OnPageAlmostIdle(
      content::WebContents* web_contents,
      const PageNavigationIdentity& page_navigation_id) {}
  virtual void OnRendererIsBloated(
      content::WebContents* web_contents,
      const PageNavigationIdentity& page_navigation_id) {}
  virtual void OnExpectedTaskQueueingDurationSet(
      content::WebContents* web_contents,
      const PageNavigationIdentity& page_navigation_id,
      base::TimeDelta duration) {}
  virtual void OnLifecycleStateChanged(
      content::WebContents* web_contents,
      const PageNavigationIdentity& page_navigation_id,
      mojom::LifecycleState state) {}
  virtual void OnNonPersistentNotificationCreated(
      content::WebContents* web_contents,
      const PageNavigationIdentity& page_navigation_id) {}
  virtual void OnLoadTimePerformanceEstimate(
      content::WebContents* web_contents,
      const PageNavigationIdentity& page_navigation_id,
      base::TimeDelta load_duration,
      base::TimeDelta cpu_usage_estimate,
      uint64_t private_footprint_kb_estimate) {}
};

// Implementation of resource_coordinator::mojom::PageSignalReceiver.
// PageSignalReceiver constructs a mojo channel to PageSignalGenerator in
// resource coordinator, passes an interface pointer to PageSignalGenerator,
// receives page scoped signals from PageSignalGenerator, and dispatches them
// with WebContents to PageSignalObservers.
// The mojo channel won't be constructed until PageSignalReceiver has the first
// observer.
class PageSignalReceiver : public mojom::PageSignalReceiver {
 public:
  PageSignalReceiver();
  ~PageSignalReceiver() override;

  static bool IsEnabled();
  // Callers do not take ownership.
  static PageSignalReceiver* GetInstance();

  // mojom::PageSignalReceiver implementation.
  void NotifyPageAlmostIdle(
      const PageNavigationIdentity& page_navigation_id) override;
  void NotifyRendererIsBloated(
      const PageNavigationIdentity& page_navigation_id) override;
  void SetExpectedTaskQueueingDuration(
      const PageNavigationIdentity& page_navigation_id,
      base::TimeDelta duration) override;
  void SetLifecycleState(const PageNavigationIdentity& page_navigation_id,
                         mojom::LifecycleState) override;
  void NotifyNonPersistentNotificationCreated(
      const PageNavigationIdentity& page_navigation_id) override;
  void OnLoadTimePerformanceEstimate(
      const PageNavigationIdentity& page_navigation_id,
      base::TimeDelta load_duration,
      base::TimeDelta cpu_usage_estimate,
      uint64_t private_footprint_kb_estimate) override;

  void AddObserver(PageSignalObserver* observer);
  void RemoveObserver(PageSignalObserver* observer);
  void AssociateCoordinationUnitIDWithWebContents(
      const CoordinationUnitID& cu_id,
      content::WebContents* web_contents);
  void SetNavigationID(content::WebContents* web_contents,
                       int64_t navigation_id);
  void RemoveCoordinationUnitID(const CoordinationUnitID& cu_id);

  // Retrieves the unique ID of the last navigation observed for |web_contents|
  // or 0 if no navigation has been observed yet.
  int64_t GetNavigationIDForWebContents(content::WebContents* web_contents);

 private:
  FRIEND_TEST_ALL_PREFIXES(PageSignalReceiverUnitTest,
                           NotifyObserversThatCanRemoveCoordinationUnitID);
  template <typename Method, typename... Params>
  void NotifyObserversIfKnownCu(
      const PageNavigationIdentity& page_navigation_id,
      Method m,
      Params... params);

  mojo::Binding<mojom::PageSignalReceiver> binding_;
  std::map<CoordinationUnitID, content::WebContents*> cu_id_web_contents_map_;
  std::map<content::WebContents*, int64_t> web_contents_navigation_id_map_;
  base::ObserverList<PageSignalObserver>::Unchecked observers_;
  DISALLOW_COPY_AND_ASSIGN(PageSignalReceiver);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_PAGE_SIGNAL_RECEIVER_H_
