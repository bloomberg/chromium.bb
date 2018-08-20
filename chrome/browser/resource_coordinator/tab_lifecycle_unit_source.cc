// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/discard_metrics_lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents_user_data.h"

namespace resource_coordinator {

namespace {
TabLifecycleUnitSource* instance_ = nullptr;
}  // namespace

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
};

TabLifecycleUnitSource::TabLifecycleUnitSource(
    InterventionPolicyDatabase* intervention_policy_database,
    UsageClock* usage_clock)
    : browser_tab_strip_tracker_(this, nullptr, this),
      page_signal_receiver_observer_(this),
      intervention_policy_database_(intervention_policy_database),
      usage_clock_(usage_clock) {
  DCHECK(!instance_);

  // In unit tests, tabs might already exist when TabLifecycleUnitSource is
  // instantiated. No TabLifecycleUnit is created for these tabs.

  DCHECK(intervention_policy_database_);
  browser_tab_strip_tracker_.Init();
  instance_ = this;
  if (auto* page_signal_receiver = PageSignalReceiver::GetInstance())
    page_signal_receiver_observer_.Add(page_signal_receiver);
}

TabLifecycleUnitSource::~TabLifecycleUnitSource() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

// static
TabLifecycleUnitSource* TabLifecycleUnitSource::GetInstance() {
  return instance_;
}

TabLifecycleUnitExternal* TabLifecycleUnitSource::GetTabLifecycleUnitExternal(
    content::WebContents* web_contents) const {
  return GetTabLifecycleUnit(web_contents);
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

void TabLifecycleUnitSource::OnFirstLifecycleUnitCreated() {
  // In production builds monitor the policy override of the lifecycles feature.
  // This class owns the monitor so it is okay to use base::Unretained. Note
  // that tests often don't have a local_state pref service available.
  if (!g_browser_process->local_state())
    return;

  tab_lifecycles_enterprise_preference_monitor_ =
      std::make_unique<TabLifecylesEnterprisePreferenceMonitor>(
          g_browser_process->local_state(),
          base::BindRepeating(
              &TabLifecycleUnitSource::SetTabLifecyclesEnterprisePolicy,
              base::Unretained(this)));
}

void TabLifecycleUnitSource::OnAllLifecycleUnitsDestroyed() {
  // This needs to be freed before shutdown as PrefChangeRegistrars can't exist
  // at shutdown. Tear it down when there are no more tabs being monitored.
  tab_lifecycles_enterprise_preference_monitor_.reset();
}

TabLifecycleUnitSource::TabLifecycleUnit*
TabLifecycleUnitSource::GetTabLifecycleUnit(
    content::WebContents* web_contents) const {
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
  DCHECK(!focused_web_contents || focused_lifecycle_unit);
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

void TabLifecycleUnitSource::TabInsertedAt(TabStripModel* tab_strip_model,
                                           content::WebContents* contents,
                                           int index,
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

    // Add a self-owned observer to the LifecycleUnit to record metrics.
    lifecycle_unit->AddObserver(new DiscardMetricsLifecycleUnitObserver());

    NotifyLifecycleUnitCreated(lifecycle_unit);
  }
}

void TabLifecycleUnitSource::TabDetachedAt(content::WebContents* contents,
                                           int index,
                                           bool was_active) {
  TabLifecycleUnit* lifecycle_unit = GetTabLifecycleUnit(contents);
  DCHECK(lifecycle_unit);
  if (focused_lifecycle_unit_ == lifecycle_unit)
    UpdateFocusedTabTo(nullptr);
  lifecycle_unit->SetTabStripModel(nullptr);
}

void TabLifecycleUnitSource::ActiveTabChanged(
    content::WebContents* old_contents,
    content::WebContents* new_contents,
    int index,
    int reason) {
  UpdateFocusedTab();
}

void TabLifecycleUnitSource::TabReplacedAt(TabStripModel* tab_strip_model,
                                           content::WebContents* old_contents,
                                           content::WebContents* new_contents,
                                           int index) {
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

void TabLifecycleUnitSource::TabChangedAt(content::WebContents* contents,
                                          int index,
                                          TabChangeType change_type) {
  if (change_type != TabChangeType::kAll)
    return;
  TabLifecycleUnit* lifecycle_unit = GetTabLifecycleUnit(contents);
  DCHECK(lifecycle_unit);
  auto* audible_helper = RecentlyAudibleHelper::FromWebContents(contents);
  lifecycle_unit->SetRecentlyAudible(audible_helper->WasRecentlyAudible());
}

void TabLifecycleUnitSource::OnBrowserSetLastActive(Browser* browser) {
  UpdateFocusedTab();
}

void TabLifecycleUnitSource::OnBrowserNoLongerActive(Browser* browser) {
  UpdateFocusedTab();
}

void TabLifecycleUnitSource::OnLifecycleStateChanged(
    content::WebContents* web_contents,
    const PageNavigationIdentity& page_navigation_id,
    mojom::LifecycleState state) {
  TabLifecycleUnit* lifecycle_unit = GetTabLifecycleUnit(web_contents);

  // Some WebContents aren't attached to a tab, so there is no corresponding
  // TabLifecycleUnit.
  // TODO(fdoray): This may want to filter for the navigation_id.
  if (lifecycle_unit)
    lifecycle_unit->UpdateLifecycleState(state);
}

void TabLifecycleUnitSource::SetTabLifecyclesEnterprisePolicy(bool enabled) {
  tab_lifecycles_enterprise_policy_ = enabled;
}

TabLifecylesEnterprisePreferenceMonitor::
    TabLifecylesEnterprisePreferenceMonitor(
        PrefService* pref_service,
        OnPreferenceChangedCallback callback)
    : pref_service_(pref_service), callback_(callback) {
  // Create a registrar to track changes to the setting.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service_);
  pref_change_registrar_->Add(
      prefs::kTabLifecyclesEnabled,
      base::BindRepeating(&TabLifecylesEnterprisePreferenceMonitor::GetPref,
                          base::Unretained(this)));

  // Do an initial check of the value.
  GetPref();
}

TabLifecylesEnterprisePreferenceMonitor::
    ~TabLifecylesEnterprisePreferenceMonitor() = default;

void TabLifecylesEnterprisePreferenceMonitor::GetPref() {
  bool enabled = true;

  // If the preference is set to false by enterprise policy then disable the
  // lifecycles feature.
  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kTabLifecyclesEnabled);
  if (pref->IsManaged() && !pref->GetValue()->GetBool())
    enabled = false;

  callback_.Run(enabled);
}

}  // namespace resource_coordinator
