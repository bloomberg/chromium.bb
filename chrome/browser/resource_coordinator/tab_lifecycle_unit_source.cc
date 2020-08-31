// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/discard_metrics_lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_observer.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/tracing_lifecycle_unit_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents_user_data.h"

namespace resource_coordinator {

// Allows storage of a TabLifecycleUnit on a WebContents.
class TabLifecycleUnitSource::TabLifecycleUnitHolder
    : public content::WebContentsUserData<
          TabLifecycleUnitSource::TabLifecycleUnitHolder> {
 public:
  ~TabLifecycleUnitHolder() override = default;

  TabLifecycleUnit* lifecycle_unit() const { return lifecycle_unit_.get(); }
  void set_lifecycle_unit(std::unique_ptr<TabLifecycleUnit> lifecycle_unit) {
    lifecycle_unit_ = std::move(lifecycle_unit);
  }
  std::unique_ptr<TabLifecycleUnit> TakeTabLifecycleUnit() {
    return std::move(lifecycle_unit_);
  }

 private:
  friend class content::WebContentsUserData<TabLifecycleUnitHolder>;

  explicit TabLifecycleUnitHolder(content::WebContents*) {}

  std::unique_ptr<TabLifecycleUnit> lifecycle_unit_;

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleUnitHolder);
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabLifecycleUnitSource::TabLifecycleUnitHolder)

// A very simple graph observer that forwards events over to the
// TabLifecycleUnitSource on the UI thread. This is created on the UI thread
// and ownership passed to the performance manager.
class TabLifecycleStateObserver
    : public performance_manager::PageNode::ObserverDefaultImpl,
      public performance_manager::GraphOwned {
 public:
  using Graph = performance_manager::Graph;
  using PageNode = performance_manager::PageNode;
  using WebContentsProxy = performance_manager::WebContentsProxy;

  TabLifecycleStateObserver() = default;
  ~TabLifecycleStateObserver() override = default;

 private:
  static void OnLifecycleStateChangedImpl(
      const WebContentsProxy& contents_proxy,
      performance_manager::mojom::LifecycleState state) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // If the web contents is still alive then dispatch to the actual
    // implementation in TabLifecycleUnitSource.
    if (auto* contents = contents_proxy.Get())
      TabLifecycleUnitSource::OnLifecycleStateChanged(contents, state);
  }

  static void OnOriginTrialFreezePolicyChangedImpl(
      const WebContentsProxy& contents_proxy,
      performance_manager::mojom::InterventionPolicy policy) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // If the web contents is still alive then dispatch to the actual
    // implementation in TabLifecycleUnitSource.
    if (auto* contents = contents_proxy.Get()) {
      TabLifecycleUnitSource::OnOriginTrialFreezePolicyChanged(contents,
                                                               policy);
    }
  }

  static void OnPageIsHoldingWebLockChangedImpl(
      const WebContentsProxy& contents_proxy,
      bool is_holding_weblock) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // If the web contents is still alive then dispatch to the actual
    // implementation in TabLifecycleUnitSource.
    if (auto* contents = contents_proxy.Get()) {
      TabLifecycleUnitSource::OnIsHoldingWebLockChanged(contents,
                                                        is_holding_weblock);
    }
  }

  static void OnPageIsHoldingIndexedDBLockChangedImpl(
      const WebContentsProxy& contents_proxy,
      bool is_holding_indexeddb_lock) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // If the web contents is still alive then dispatch to the actual
    // implementation in TabLifecycleUnitSource.
    if (auto* contents = contents_proxy.Get()) {
      TabLifecycleUnitSource::OnIsHoldingIndexedDBLockChanged(
          contents, is_holding_indexeddb_lock);
    }
  }

  // performance_manager::PageNode::ObserverDefaultImpl::
  void OnPageLifecycleStateChanged(const PageNode* page_node) override {
    // Forward the notification over to the UI thread.
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&TabLifecycleStateObserver::OnLifecycleStateChangedImpl,
                       page_node->GetContentsProxy(),
                       page_node->GetLifecycleState()));
  }

  void OnPageOriginTrialFreezePolicyChanged(
      const PageNode* page_node) override {
    // Forward the notification over to the UI thread.
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &TabLifecycleStateObserver::OnOriginTrialFreezePolicyChangedImpl,
            page_node->GetContentsProxy(),
            page_node->GetOriginTrialFreezePolicy()));
  }

  void OnPageIsHoldingWebLockChanged(const PageNode* page_node) override {
    // Forward the notification over to the UI thread.
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &TabLifecycleStateObserver::OnPageIsHoldingWebLockChangedImpl,
            page_node->GetContentsProxy(), page_node->IsHoldingWebLock()));
  }

  void OnPageIsHoldingIndexedDBLockChanged(const PageNode* page_node) override {
    // Forward the notification over to the UI thread.
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &TabLifecycleStateObserver::OnPageIsHoldingIndexedDBLockChangedImpl,
            page_node->GetContentsProxy(),
            page_node->IsHoldingIndexedDBLock()));
  }

  void OnPassedToGraph(Graph* graph) override {
    graph->AddPageNodeObserver(this);
  }

  void OnTakenFromGraph(Graph* graph) override {
    graph->RemovePageNodeObserver(this);
  }

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleStateObserver);
};

TabLifecycleUnitSource::TabLifecycleUnitSource(
    InterventionPolicyDatabase* intervention_policy_database,
    UsageClock* usage_clock)
    : browser_tab_strip_tracker_(this, nullptr),
      intervention_policy_database_(intervention_policy_database),
      usage_clock_(usage_clock) {
  // In unit tests, tabs might already exist when TabLifecycleUnitSource is
  // instantiated. No TabLifecycleUnit is created for these tabs.

  DCHECK(intervention_policy_database_);

  BrowserList::AddObserver(this);
  browser_tab_strip_tracker_.Init();
}

TabLifecycleUnitSource::~TabLifecycleUnitSource() {
  BrowserList::RemoveObserver(this);
}

void TabLifecycleUnitSource::Start() {
  // TODO(sebmarchand): Remove the "IsAvailable" check, or merge the TM into the
  // PM. The TM and PM must always exist together.
  if (performance_manager::PerformanceManagerImpl::IsAvailable()) {
    performance_manager::PerformanceManagerImpl::PassToGraph(
        FROM_HERE, std::make_unique<TabLifecycleStateObserver>());
  }
}

// static
TabLifecycleUnitExternal* TabLifecycleUnitSource::GetTabLifecycleUnitExternal(
    content::WebContents* web_contents) {
  auto* lu = GetTabLifecycleUnit(web_contents);
  if (!lu)
    return nullptr;
  return lu->AsTabLifecycleUnitExternal();
}

void TabLifecycleUnitSource::AddTabLifecycleObserver(
    TabLifecycleObserver* observer) {
  tab_lifecycle_observers_.AddObserver(observer);
}

void TabLifecycleUnitSource::RemoveTabLifecycleObserver(
    TabLifecycleObserver* observer) {
  tab_lifecycle_observers_.RemoveObserver(observer);
}

void TabLifecycleUnitSource::SetFocusedTabStripModelForTesting(
    TabStripModel* tab_strip) {
  focused_tab_strip_model_for_testing_ = tab_strip;
  UpdateFocusedTab();
}

void TabLifecycleUnitSource::SetMemoryLimitEnterprisePolicyFlag(bool enabled) {
  memory_limit_enterprise_policy_ = enabled;
}

void TabLifecycleUnitSource::OnFirstLifecycleUnitCreated() {
  // In production builds monitor the policy override of the lifecycles feature.
  // This class owns the monitor so it is okay to use base::Unretained. Note
  // that tests often don't have a local_state pref service available.
  if (!g_browser_process->local_state())
    return;

  tab_freezing_enabled_enterprise_preference_monitor_ =
      std::make_unique<TabFreezingEnabledPreferenceMonitor>(
          g_browser_process->local_state(),
          base::BindRepeating(
              &TabLifecycleUnitSource::SetTabLifecyclesEnterprisePolicy,
              base::Unretained(this)));
}

void TabLifecycleUnitSource::OnAllLifecycleUnitsDestroyed() {
  // This needs to be freed before shutdown as PrefChangeRegistrars can't exist
  // at shutdown. Tear it down when there are no more tabs being monitored.
  tab_freezing_enabled_enterprise_preference_monitor_.reset();
}

// static
TabLifecycleUnitSource::TabLifecycleUnit*
TabLifecycleUnitSource::GetTabLifecycleUnit(
    content::WebContents* web_contents) {
  auto* holder = TabLifecycleUnitHolder::FromWebContents(web_contents);
  if (holder)
    return holder->lifecycle_unit();
  return nullptr;
}

TabStripModel* TabLifecycleUnitSource::GetFocusedTabStripModel() const {
  if (focused_tab_strip_model_for_testing_)
    return focused_tab_strip_model_for_testing_;
  // Use FindLastActive() rather than FindBrowserWithActiveWindow() to avoid
  // flakiness when focus changes during browser tests.
  Browser* const focused_browser = chrome::FindLastActive();
  if (!focused_browser)
    return nullptr;
  return focused_browser->tab_strip_model();
}

void TabLifecycleUnitSource::UpdateFocusedTab() {
  TabStripModel* const focused_tab_strip_model = GetFocusedTabStripModel();
  content::WebContents* const focused_web_contents =
      focused_tab_strip_model ? focused_tab_strip_model->GetActiveWebContents()
                              : nullptr;
  TabLifecycleUnit* focused_lifecycle_unit =
      focused_web_contents ? GetTabLifecycleUnit(focused_web_contents)
                           : nullptr;

  // TODO(sangwoo.ko) We are refactoring TabStripModel API and this is
  // workaround to avoid DCHECK failure on Chromium os. This DCHECK is supposing
  // that OnTabInserted() is always called before OnBrowserSetLastActive is
  // called but it's not. After replacing old API use in BrowserView,
  // restore this to DCHECK(!focused_web_contents || focused_lifecycle_unit);
  // else case will be handled by following OnTabInserted().
  if (!focused_web_contents || focused_lifecycle_unit)
    UpdateFocusedTabTo(focused_lifecycle_unit);
}

void TabLifecycleUnitSource::UpdateFocusedTabTo(
    TabLifecycleUnit* new_focused_lifecycle_unit) {
  if (new_focused_lifecycle_unit == focused_lifecycle_unit_)
    return;
  if (focused_lifecycle_unit_)
    focused_lifecycle_unit_->SetFocused(false);
  if (new_focused_lifecycle_unit)
    new_focused_lifecycle_unit->SetFocused(true);
  focused_lifecycle_unit_ = new_focused_lifecycle_unit;
}

void TabLifecycleUnitSource::OnTabInserted(TabStripModel* tab_strip_model,
                                           content::WebContents* contents,
                                           bool foreground) {
  TabLifecycleUnit* lifecycle_unit = GetTabLifecycleUnit(contents);
  if (lifecycle_unit) {
    // An existing tab was moved to a new window.
    lifecycle_unit->SetTabStripModel(tab_strip_model);
    if (foreground)
      UpdateFocusedTab();
  } else {
    // A tab was created.
    TabLifecycleUnitHolder::CreateForWebContents(contents);
    auto* holder = TabLifecycleUnitHolder::FromWebContents(contents);
    holder->set_lifecycle_unit(std::make_unique<TabLifecycleUnit>(
        this, &tab_lifecycle_observers_, usage_clock_, contents,
        tab_strip_model));
    TabLifecycleUnit* lifecycle_unit = holder->lifecycle_unit();
    if (GetFocusedTabStripModel() == tab_strip_model && foreground)
      UpdateFocusedTabTo(lifecycle_unit);

    // Add a self-owned observers to record metrics and trace events.
    lifecycle_unit->AddObserver(new DiscardMetricsLifecycleUnitObserver());
    lifecycle_unit->AddObserver(new TracingLifecycleUnitObserver());

    auto page_node =
        performance_manager::PerformanceManager::GetPageNodeForWebContents(
            contents);

    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<performance_manager::PageNode> page_node,
               scoped_refptr<base::SingleThreadTaskRunner> runner) {
              if (!page_node)
                return;
              runner->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      &TabLifecycleUnitSource::SetInitialStateFromPageNodeData,
                      page_node->GetContentsProxy(),
                      page_node->GetOriginTrialFreezePolicy(),
                      page_node->IsHoldingWebLock(),
                      page_node->IsHoldingIndexedDBLock()));
            },
            std::move(page_node), base::ThreadTaskRunnerHandle::Get()));

    NotifyLifecycleUnitCreated(lifecycle_unit);
  }
}

void TabLifecycleUnitSource::OnTabDetached(content::WebContents* contents) {
  TabLifecycleUnit* lifecycle_unit = GetTabLifecycleUnit(contents);
  DCHECK(lifecycle_unit);
  if (focused_lifecycle_unit_ == lifecycle_unit)
    UpdateFocusedTabTo(nullptr);
  lifecycle_unit->SetTabStripModel(nullptr);
}

void TabLifecycleUnitSource::OnTabReplaced(content::WebContents* old_contents,
                                           content::WebContents* new_contents) {
  auto* old_contents_holder =
      TabLifecycleUnitHolder::FromWebContents(old_contents);
  DCHECK(old_contents_holder);
  DCHECK(old_contents_holder->lifecycle_unit());
  TabLifecycleUnitHolder::CreateForWebContents(new_contents);
  auto* new_contents_holder =
      TabLifecycleUnitHolder::FromWebContents(new_contents);
  DCHECK(new_contents_holder);
  DCHECK(!new_contents_holder->lifecycle_unit());
  new_contents_holder->set_lifecycle_unit(
      old_contents_holder->TakeTabLifecycleUnit());
  new_contents_holder->lifecycle_unit()->SetWebContents(new_contents);
}

void TabLifecycleUnitSource::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kInserted: {
      for (const auto& contents : change.GetInsert()->contents) {
        OnTabInserted(tab_strip_model, contents.contents,
                      selection.new_contents == contents.contents);
      }
      break;
    }
    case TabStripModelChange::kRemoved: {
      for (const auto& contents : change.GetRemove()->contents)
        OnTabDetached(contents.contents);
      break;
    }
    case TabStripModelChange::kReplaced: {
      auto* replace = change.GetReplace();
      OnTabReplaced(replace->old_contents, replace->new_contents);
      break;
    }
    case TabStripModelChange::kMoved:
    case TabStripModelChange::kSelectionOnly:
      break;
  }

  if (selection.active_tab_changed() && !tab_strip_model->empty())
    UpdateFocusedTab();
}

void TabLifecycleUnitSource::TabChangedAt(content::WebContents* contents,
                                          int index,
                                          TabChangeType change_type) {
  if (change_type != TabChangeType::kAll)
    return;
  TabLifecycleUnit* lifecycle_unit = GetTabLifecycleUnit(contents);
  // This can be called before OnTabStripModelChanged() and |lifecycle_unit|
  // will be null in that case. http://crbug.com/877940
  if (!lifecycle_unit)
    return;

  auto* audible_helper = RecentlyAudibleHelper::FromWebContents(contents);
  lifecycle_unit->SetRecentlyAudible(audible_helper->WasRecentlyAudible());
}

void TabLifecycleUnitSource::OnBrowserSetLastActive(Browser* browser) {
  UpdateFocusedTab();
}

void TabLifecycleUnitSource::OnBrowserNoLongerActive(Browser* browser) {
  UpdateFocusedTab();
}

// static
void TabLifecycleUnitSource::OnLifecycleStateChanged(
    content::WebContents* web_contents,
    performance_manager::mojom::LifecycleState state) {
  TabLifecycleUnit* lifecycle_unit = GetTabLifecycleUnit(web_contents);

  // Lifecycle state is updated independently from navigations. Therefore, there
  // is no need to filter out the event if it was generated before the last
  // navigation.
  if (lifecycle_unit)
    lifecycle_unit->UpdateLifecycleState(state);
}

// static
void TabLifecycleUnitSource::SetInitialStateFromPageNodeData(
    const performance_manager::WebContentsProxy& contents_proxy,
    performance_manager::mojom::InterventionPolicy origin_trial_policy,
    bool is_holding_weblock,
    bool is_holding_indexeddb_lock) {
  if (!contents_proxy.Get())
    return;
  TabLifecycleUnit* lifecycle_unit = GetTabLifecycleUnit(contents_proxy.Get());
  DCHECK(lifecycle_unit);
  lifecycle_unit->SetInitialStateFromPageNodeData(
      origin_trial_policy, is_holding_weblock, is_holding_indexeddb_lock);
}

// static
void TabLifecycleUnitSource::OnOriginTrialFreezePolicyChanged(
    content::WebContents* web_contents,
    performance_manager::mojom::InterventionPolicy policy) {
  TabLifecycleUnit* lifecycle_unit = GetTabLifecycleUnit(web_contents);

  // Some WebContents aren't attached to a tab, so there is no corresponding
  // TabLifecycleUnit.
  // TODO(fdoray): This may want to filter for the navigation_id.
  if (lifecycle_unit)
    lifecycle_unit->UpdateOriginTrialFreezePolicy(policy);
}

// static
void TabLifecycleUnitSource::OnIsHoldingWebLockChanged(
    content::WebContents* web_contents,
    bool is_holding_weblock) {
  TabLifecycleUnit* lifecycle_unit = GetTabLifecycleUnit(web_contents);

  // Some WebContents aren't attached to a tab, so there is no corresponding
  // TabLifecycleUnit.
  if (lifecycle_unit)
    lifecycle_unit->SetIsHoldingWebLock(is_holding_weblock);
}

// static
void TabLifecycleUnitSource::OnIsHoldingIndexedDBLockChanged(
    content::WebContents* web_contents,
    bool is_holding_indexeddb_lock) {
  TabLifecycleUnit* lifecycle_unit = GetTabLifecycleUnit(web_contents);

  // Some WebContents aren't attached to a tab, so there is no corresponding
  // TabLifecycleUnit.
  if (lifecycle_unit)
    lifecycle_unit->SetIsHoldingIndexedDBLock(is_holding_indexeddb_lock);
}

void TabLifecycleUnitSource::SetTabLifecyclesEnterprisePolicy(bool enabled) {
  tab_freezing_enabled_enterprise_policy_ = enabled;
}

TabFreezingEnabledPreferenceMonitor::TabFreezingEnabledPreferenceMonitor(
    PrefService* pref_service,
    OnPreferenceChangedCallback callback)
    : pref_service_(pref_service), callback_(callback) {
  // Create a registrar to track changes to the setting.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service_);
  pref_change_registrar_->Add(
      prefs::kTabFreezingEnabled,
      base::BindRepeating(&TabFreezingEnabledPreferenceMonitor::GetPref,
                          base::Unretained(this)));

  // Do an initial check of the value.
  GetPref();
}

TabFreezingEnabledPreferenceMonitor::~TabFreezingEnabledPreferenceMonitor() =
    default;

void TabFreezingEnabledPreferenceMonitor::GetPref() {
  bool enabled = true;

  // If the preference is set to false by enterprise policy then disable the
  // lifecycles feature.
  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kTabFreezingEnabled);
  if (pref->IsManaged() && !pref->GetValue()->GetBool())
    enabled = false;

  callback_.Run(enabled);
}

}  // namespace resource_coordinator
