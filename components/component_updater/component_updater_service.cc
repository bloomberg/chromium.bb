// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_service.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "components/component_updater/component_patcher_operation.h"
#include "components/component_updater/component_unpacker.h"
#include "components/component_updater/component_updater_configurator.h"
#include "components/component_updater/component_updater_ping_manager.h"
#include "components/component_updater/component_updater_utils.h"
#include "components/component_updater/crx_downloader.h"
#include "components/component_updater/crx_update_item.h"
#include "components/component_updater/update_checker.h"
#include "components/component_updater/update_response.h"
#include "url/gurl.h"

namespace component_updater {

// The component updater is designed to live until process shutdown, so
// base::Bind() calls are not refcounted.

namespace {

// Returns true if the |proposed| version is newer than |current| version.
bool IsVersionNewer(const Version& current, const std::string& proposed) {
  Version proposed_ver(proposed);
  return proposed_ver.IsValid() && current.CompareTo(proposed_ver) < 0;
}

// Returns true if a differential update is available, it has not failed yet,
// and the configuration allows it.
bool CanTryDiffUpdate(const CrxUpdateItem* update_item,
                      const Configurator& config) {
  return HasDiffUpdate(update_item) && !update_item->diff_update_failed &&
         config.DeltasEnabled();
}

void AppendDownloadMetrics(
    const std::vector<CrxDownloader::DownloadMetrics>& source,
    std::vector<CrxDownloader::DownloadMetrics>* destination) {
  destination->insert(destination->end(), source.begin(), source.end());
}

}  // namespace

CrxUpdateItem::CrxUpdateItem()
    : status(kNew),
      on_demand(false),
      diff_update_failed(false),
      error_category(0),
      error_code(0),
      extra_code1(0),
      diff_error_category(0),
      diff_error_code(0),
      diff_extra_code1(0) {
}

CrxUpdateItem::~CrxUpdateItem() {
}

CrxComponent::CrxComponent()
    : installer(NULL), allow_background_download(true) {
}

CrxComponent::~CrxComponent() {
}

//////////////////////////////////////////////////////////////////////////////
// The one and only implementation of the ComponentUpdateService interface. In
// charge of running the show. The main method is ProcessPendingItems() which
// is called periodically to do the upgrades/installs or the update checks.
// An important consideration here is to be as "low impact" as we can to the
// rest of the browser, so even if we have many components registered and
// eligible for update, we only do one thing at a time with pauses in between
// the tasks. Also when we do network requests there is only one |url_fetcher_|
// in flight at a time.
// There are no locks in this code, the main structure |work_items_| is mutated
// only from the main thread. The unpack and installation is done in a blocking
// pool thread. The network requests are done in the IO thread or in the file
// thread.
class CrxUpdateService : public ComponentUpdateService, public OnDemandUpdater {
 public:
  explicit CrxUpdateService(Configurator* config);
  virtual ~CrxUpdateService();

  // Overrides for ComponentUpdateService.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual Status Start() OVERRIDE;
  virtual Status Stop() OVERRIDE;
  virtual Status RegisterComponent(const CrxComponent& component) OVERRIDE;
  virtual std::vector<std::string> GetComponentIDs() const OVERRIDE;
  virtual OnDemandUpdater& GetOnDemandUpdater() OVERRIDE;
  virtual void MaybeThrottle(const std::string& crx_id,
                             const base::Closure& callback) OVERRIDE;
  virtual scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner()
      OVERRIDE;

  // Context for a crx download url request.
  struct CRXContext {
    ComponentInstaller* installer;
    std::vector<uint8> pk_hash;
    std::string id;
    std::string fingerprint;
    CRXContext() : installer(NULL) {}
  };

 private:
  enum ErrorCategory {
    kErrorNone = 0,
    kNetworkError,
    kUnpackError,
    kInstallError,
  };

  enum StepDelayInterval {
    kStepDelayShort = 0,
    kStepDelayMedium,
    kStepDelayLong,
  };

  // Overrides for ComponentUpdateService.
  virtual bool GetComponentDetails(const std::string& component_id,
                                   CrxUpdateItem* item) const OVERRIDE;

  // Overrides for OnDemandUpdater.
  virtual Status OnDemandUpdate(const std::string& component_id) OVERRIDE;

  void UpdateCheckComplete(int error,
                           const std::string& error_message,
                           const UpdateResponse::Results& results);
  void OnUpdateCheckSucceeded(const UpdateResponse::Results& results);
  void OnUpdateCheckFailed(int error, const std::string& error_message);

  void DownloadProgress(const std::string& component_id,
                        const CrxDownloader::Result& download_result);

  void DownloadComplete(scoped_ptr<CRXContext> crx_context,
                        const CrxDownloader::Result& download_result);

  Status OnDemandUpdateInternal(CrxUpdateItem* item);
  Status OnDemandUpdateWithCooldown(CrxUpdateItem* item);

  void ProcessPendingItems();

  // Find a component that is ready to update.
  CrxUpdateItem* FindReadyComponent() const;

  // Prepares the components for an update check and initiates the request.
  // Returns true if an update check request has been made. Returns false if
  // no update check was needed or an error occured.
  bool CheckForUpdates();

  void UpdateComponent(CrxUpdateItem* workitem);

  void ScheduleNextRun(StepDelayInterval step_delay);

  void ParseResponse(const std::string& xml);

  void Install(scoped_ptr<CRXContext> context, const base::FilePath& crx_path);

  void EndUnpacking(const std::string& component_id,
                    const base::FilePath& crx_path,
                    ComponentUnpacker::Error error,
                    int extended_error);

  void DoneInstalling(const std::string& component_id,
                      ComponentUnpacker::Error error,
                      int extended_error);

  void ChangeItemState(CrxUpdateItem* item, CrxUpdateItem::Status to);

  size_t ChangeItemStatus(CrxUpdateItem::Status from, CrxUpdateItem::Status to);

  CrxUpdateItem* FindUpdateItemById(const std::string& id) const;

  void NotifyObservers(Observer::Events event, const std::string& id);

  bool HasOnDemandItems() const;

  Status GetServiceStatus(const CrxUpdateItem::Status status);

  scoped_ptr<Configurator> config_;

  scoped_ptr<UpdateChecker> update_checker_;

  scoped_ptr<PingManager> ping_manager_;

  scoped_refptr<ComponentUnpacker> unpacker_;

  scoped_ptr<CrxDownloader> crx_downloader_;

  // A collection of every work item.
  typedef std::vector<CrxUpdateItem*> UpdateItems;
  UpdateItems work_items_;

  base::OneShotTimer<CrxUpdateService> timer_;

  base::ThreadChecker thread_checker_;

  // Used to post responses back to the main thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  bool running_;

  ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(CrxUpdateService);
};

//////////////////////////////////////////////////////////////////////////////

CrxUpdateService::CrxUpdateService(Configurator* config)
    : config_(config),
      ping_manager_(new PingManager(*config)),
      main_task_runner_(base::MessageLoopProxy::current()),
      blocking_task_runner_(config->GetSequencedTaskRunner()),
      running_(false) {
}

CrxUpdateService::~CrxUpdateService() {
  // Because we are a singleton, at this point only the main thread should be
  // alive, this simplifies the management of the work that could be in
  // flight in other threads.
  Stop();
  STLDeleteElements(&work_items_);
}

void CrxUpdateService::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void CrxUpdateService::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

ComponentUpdateService::Status CrxUpdateService::Start() {
  // Note that RegisterComponent will call Start() when the first
  // component is registered, so it can be called twice. This way
  // we avoid scheduling the timer if there is no work to do.
  VLOG(1) << "CrxUpdateService starting up";
  running_ = true;
  if (work_items_.empty())
    return kOk;

  NotifyObservers(Observer::COMPONENT_UPDATER_STARTED, "");

  VLOG(1) << "First update attempt will take place in "
          << config_->InitialDelay() << " seconds";
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(config_->InitialDelay()),
               this,
               &CrxUpdateService::ProcessPendingItems);
  return kOk;
}

// Stop the main check + update loop. In flight operations will be
// completed.
ComponentUpdateService::Status CrxUpdateService::Stop() {
  VLOG(1) << "CrxUpdateService stopping";
  running_ = false;
  timer_.Stop();
  return kOk;
}

bool CrxUpdateService::HasOnDemandItems() const {
  class Helper {
   public:
    static bool IsOnDemand(CrxUpdateItem* item) { return item->on_demand; }
  };
  return std::find_if(work_items_.begin(),
                      work_items_.end(),
                      Helper::IsOnDemand) != work_items_.end();
}

// This function sets the timer which will call ProcessPendingItems() or
// ProcessRequestedItem() if there is an on_demand item.  There
// are three kinds of waits:
//  - a short delay, when there is immediate work to be done.
//  - a medium delay, when there are updates to be applied within the current
//    update cycle, or there are components that are still unchecked.
//  - a long delay when a full check/update cycle has completed for all
//    components.
void CrxUpdateService::ScheduleNextRun(StepDelayInterval step_delay) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!update_checker_);
  CHECK(!timer_.IsRunning());
  // It could be the case that Stop() had been called while a url request
  // or unpacking was in flight, if so we arrive here but |running_| is
  // false. In that case do not loop again.
  if (!running_)
    return;

  // Keep the delay short if in the middle of an update (step_delay),
  // or there are new requested_work_items_ that have not been processed yet.
  int64 delay_seconds = 0;
  if (!HasOnDemandItems()) {
    switch (step_delay) {
      case kStepDelayShort:
        delay_seconds = config_->StepDelay();
        break;
      case kStepDelayMedium:
        delay_seconds = config_->StepDelayMedium();
        break;
      case kStepDelayLong:
        delay_seconds = config_->NextCheckDelay();
        break;
    }
  } else {
    delay_seconds = config_->StepDelay();
  }

  if (step_delay != kStepDelayShort) {
    NotifyObservers(Observer::COMPONENT_UPDATER_SLEEPING, "");

    // Zero is only used for unit tests.
    if (0 == delay_seconds)
      return;
  }

  VLOG(1) << "Scheduling next run to occur in " << delay_seconds << " seconds";
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(delay_seconds),
               this,
               &CrxUpdateService::ProcessPendingItems);
}

// Given a extension-like component id, find the associated component.
CrxUpdateItem* CrxUpdateService::FindUpdateItemById(
    const std::string& id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  CrxUpdateItem::FindById finder(id);
  UpdateItems::const_iterator it =
      std::find_if(work_items_.begin(), work_items_.end(), finder);
  return it != work_items_.end() ? *it : NULL;
}

// Changes a component's status, clearing on_demand and firing notifications as
// necessary. By convention, this is the only function that can change a
// CrxUpdateItem's |status|.
// TODO(waffles): Do we want to add DCHECKS for valid state transitions here?
void CrxUpdateService::ChangeItemState(CrxUpdateItem* item,
                                       CrxUpdateItem::Status to) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (to == CrxUpdateItem::kNoUpdate || to == CrxUpdateItem::kUpdated ||
      to == CrxUpdateItem::kUpToDate) {
    item->on_demand = false;
  }

  item->status = to;

  switch (to) {
    case CrxUpdateItem::kCanUpdate:
      NotifyObservers(Observer::COMPONENT_UPDATE_FOUND, item->id);
      break;
    case CrxUpdateItem::kUpdatingDiff:
    case CrxUpdateItem::kUpdating:
      NotifyObservers(Observer::COMPONENT_UPDATE_READY, item->id);
      break;
    case CrxUpdateItem::kUpdated:
      NotifyObservers(Observer::COMPONENT_UPDATED, item->id);
      break;
    case CrxUpdateItem::kUpToDate:
    case CrxUpdateItem::kNoUpdate:
      NotifyObservers(Observer::COMPONENT_NOT_UPDATED, item->id);
      break;
    case CrxUpdateItem::kNew:
    case CrxUpdateItem::kChecking:
    case CrxUpdateItem::kDownloading:
    case CrxUpdateItem::kDownloadingDiff:
    case CrxUpdateItem::kLastStatus:
      // No notification for these states.
      break;
  }

  // Free possible pending network requests.
  if ((to == CrxUpdateItem::kUpdated) || (to == CrxUpdateItem::kUpToDate) ||
      (to == CrxUpdateItem::kNoUpdate)) {
    for (std::vector<base::Closure>::iterator it =
             item->ready_callbacks.begin();
         it != item->ready_callbacks.end();
         ++it) {
      it->Run();
    }
    item->ready_callbacks.clear();
  }
}

// Changes all the components in |work_items_| that have |from| status to
// |to| status and returns how many have been changed.
size_t CrxUpdateService::ChangeItemStatus(CrxUpdateItem::Status from,
                                          CrxUpdateItem::Status to) {
  DCHECK(thread_checker_.CalledOnValidThread());
  size_t count = 0;
  for (UpdateItems::iterator it = work_items_.begin();
       it != work_items_.end();
       ++it) {
    CrxUpdateItem* item = *it;
    if (item->status == from) {
      ChangeItemState(item, to);
      ++count;
    }
  }
  return count;
}

// Adds a component to be checked for upgrades. If the component exists it
// it will be replaced and the return code is kReplaced.
ComponentUpdateService::Status CrxUpdateService::RegisterComponent(
    const CrxComponent& component) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (component.pk_hash.empty() || !component.version.IsValid() ||
      !component.installer)
    return kError;

  std::string id(GetCrxComponentID(component));
  CrxUpdateItem* uit = FindUpdateItemById(id);
  if (uit) {
    uit->component = component;
    return kReplaced;
  }

  uit = new CrxUpdateItem;
  uit->id.swap(id);
  uit->component = component;

  work_items_.push_back(uit);

  // If this is the first component registered we call Start to
  // schedule the first timer. Otherwise, reset the timer to trigger another
  // pass over the work items, if the component updater is sleeping, fact
  // indicated by a running timer. If the timer is not running, it means that
  // the service is busy updating something, and in that case, this component
  // will be picked up at the next pass.
  if (running_) {
    if (work_items_.size() == 1) {
      Start();
    } else if (timer_.IsRunning()) {
        timer_.Start(FROM_HERE,
                     base::TimeDelta::FromSeconds(config_->InitialDelay()),
                     this,
                     &CrxUpdateService::ProcessPendingItems);
    }
  }

  return kOk;
}

std::vector<std::string> CrxUpdateService::GetComponentIDs() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<std::string> component_ids;
  for (UpdateItems::const_iterator it = work_items_.begin();
       it != work_items_.end();
       ++it) {
    const CrxUpdateItem* item = *it;
    component_ids.push_back(item->id);
  }
  return component_ids;
}

OnDemandUpdater& CrxUpdateService::GetOnDemandUpdater() {
  return *this;
}

void CrxUpdateService::MaybeThrottle(const std::string& crx_id,
                                     const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Check if we can on-demand update, else unblock the request anyway.
  CrxUpdateItem* item = FindUpdateItemById(crx_id);
  Status status = OnDemandUpdateWithCooldown(item);
  if (status == kOk || status == kInProgress) {
    item->ready_callbacks.push_back(callback);
    return;
  }
  callback.Run();
}

scoped_refptr<base::SequencedTaskRunner>
CrxUpdateService::GetSequencedTaskRunner() {
  return config_->GetSequencedTaskRunner();
}

bool CrxUpdateService::GetComponentDetails(const std::string& component_id,
                                           CrxUpdateItem* item) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  const CrxUpdateItem* crx_update_item(FindUpdateItemById(component_id));
  if (crx_update_item)
    *item = *crx_update_item;
  return crx_update_item != NULL;
}

// Start the process of checking for an update, for a particular component
// that was previously registered.
// |component_id| is a value returned from GetCrxComponentID().
ComponentUpdateService::Status CrxUpdateService::OnDemandUpdate(
    const std::string& component_id) {
  return OnDemandUpdateInternal(FindUpdateItemById(component_id));
}

// This is the main loop of the component updater. It updates one component
// at a time if updates are available. Otherwise, it does an update check or
// takes a long sleep until the loop runs again.
void CrxUpdateService::ProcessPendingItems() {
  DCHECK(thread_checker_.CalledOnValidThread());

  CrxUpdateItem* ready_upgrade = FindReadyComponent();
  if (ready_upgrade) {
    UpdateComponent(ready_upgrade);
    return;
  }

  if (!CheckForUpdates())
    ScheduleNextRun(kStepDelayLong);
}

CrxUpdateItem* CrxUpdateService::FindReadyComponent() const {
  class Helper {
   public:
    static bool IsReadyOnDemand(CrxUpdateItem* item) {
      return item->on_demand && IsReady(item);
    }
    static bool IsReady(CrxUpdateItem* item) {
      return item->status == CrxUpdateItem::kCanUpdate;
    }
  };

  std::vector<CrxUpdateItem*>::const_iterator it = std::find_if(
      work_items_.begin(), work_items_.end(), Helper::IsReadyOnDemand);
  if (it != work_items_.end())
    return *it;
  it = std::find_if(work_items_.begin(), work_items_.end(), Helper::IsReady);
  if (it != work_items_.end())
    return *it;
  return NULL;
}

// Prepares the components for an update check and initiates the request.
// On demand components are always included in the update check request.
// Otherwise, only include components that have not been checked recently.
bool CrxUpdateService::CheckForUpdates() {
  const base::TimeDelta minimum_recheck_wait_time =
      base::TimeDelta::FromSeconds(config_->MinimumReCheckWait());
  const base::Time now(base::Time::Now());

  std::vector<CrxUpdateItem*> items_to_check;
  for (size_t i = 0; i != work_items_.size(); ++i) {
    CrxUpdateItem* item = work_items_[i];
    DCHECK(item->status == CrxUpdateItem::kNew ||
           item->status == CrxUpdateItem::kNoUpdate ||
           item->status == CrxUpdateItem::kUpToDate ||
           item->status == CrxUpdateItem::kUpdated);

    const base::TimeDelta time_since_last_checked(now - item->last_check);

    if (!item->on_demand &&
        time_since_last_checked < minimum_recheck_wait_time) {
      VLOG(1) << "Skipping check for component update: id=" << item->id
              << ", time_since_last_checked="
              << time_since_last_checked.InSeconds()
              << " seconds: too soon to check for an update";
      continue;
    }

    VLOG(1) << "Scheduling update check for component id=" << item->id
            << ", time_since_last_checked="
            << time_since_last_checked.InSeconds() << " seconds";

    item->last_check = now;
    item->crx_urls.clear();
    item->crx_diffurls.clear();
    item->previous_version = item->component.version;
    item->next_version = Version();
    item->previous_fp = item->component.fingerprint;
    item->next_fp.clear();
    item->diff_update_failed = false;
    item->error_category = 0;
    item->error_code = 0;
    item->extra_code1 = 0;
    item->diff_error_category = 0;
    item->diff_error_code = 0;
    item->diff_extra_code1 = 0;
    item->download_metrics.clear();

    items_to_check.push_back(item);

    ChangeItemState(item, CrxUpdateItem::kChecking);
  }

  if (items_to_check.empty())
    return false;

  update_checker_ =
      UpdateChecker::Create(*config_,
                            base::Bind(&CrxUpdateService::UpdateCheckComplete,
                                       base::Unretained(this))).Pass();
  return update_checker_->CheckForUpdates(items_to_check,
                                          config_->ExtraRequestParams());
}

void CrxUpdateService::UpdateComponent(CrxUpdateItem* workitem) {
  scoped_ptr<CRXContext> crx_context(new CRXContext);
  crx_context->pk_hash = workitem->component.pk_hash;
  crx_context->id = workitem->id;
  crx_context->installer = workitem->component.installer;
  crx_context->fingerprint = workitem->next_fp;
  const std::vector<GURL>* urls = NULL;
  bool allow_background_download = false;
  if (CanTryDiffUpdate(workitem, *config_)) {
    urls = &workitem->crx_diffurls;
    ChangeItemState(workitem, CrxUpdateItem::kDownloadingDiff);
  } else {
    // Background downloads are enabled only for selected components and
    // only for full downloads (see issue 340448).
    allow_background_download = workitem->component.allow_background_download;
    urls = &workitem->crx_urls;
    ChangeItemState(workitem, CrxUpdateItem::kDownloading);
  }

  // On demand component updates are always downloaded in foreground.
  const bool is_background_download = !workitem->on_demand &&
                                      allow_background_download &&
                                      config_->UseBackgroundDownloader();

  crx_downloader_.reset(
      CrxDownloader::Create(is_background_download,
                            config_->RequestContext(),
                            blocking_task_runner_,
                            config_->GetSingleThreadTaskRunner()));
  crx_downloader_->set_progress_callback(
      base::Bind(&CrxUpdateService::DownloadProgress,
                 base::Unretained(this),
                 crx_context->id));
  crx_downloader_->StartDownload(*urls,
                                 base::Bind(&CrxUpdateService::DownloadComplete,
                                            base::Unretained(this),
                                            base::Passed(&crx_context)));
}

void CrxUpdateService::UpdateCheckComplete(
    int error,
    const std::string& error_message,
    const UpdateResponse::Results& results) {
  DCHECK(thread_checker_.CalledOnValidThread());
  update_checker_.reset();
  if (!error)
    OnUpdateCheckSucceeded(results);
  else
    OnUpdateCheckFailed(error, error_message);
}

// Handles a valid Omaha update check response by matching the results with
// the registered components which were checked for updates.
// If updates are found, prepare the components for the actual version upgrade.
// One of these components will be drafted for the upgrade next time
// ProcessPendingItems is called.
void CrxUpdateService::OnUpdateCheckSucceeded(
    const UpdateResponse::Results& results) {
  size_t num_updates_pending = 0;
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "Update check succeeded.";
  std::vector<UpdateResponse::Result>::const_iterator it;
  for (it = results.list.begin(); it != results.list.end(); ++it) {
    CrxUpdateItem* crx = FindUpdateItemById(it->extension_id);
    if (!crx)
      continue;

    if (crx->status != CrxUpdateItem::kChecking) {
      NOTREACHED();
      continue;  // Not updating this component now.
    }

    if (it->manifest.version.empty()) {
      // No version means no update available.
      ChangeItemState(crx, CrxUpdateItem::kNoUpdate);
      VLOG(1) << "No update available for component: " << crx->id;
      continue;
    }

    if (!IsVersionNewer(crx->component.version, it->manifest.version)) {
      // The component is up to date.
      ChangeItemState(crx, CrxUpdateItem::kUpToDate);
      VLOG(1) << "Component already up-to-date: " << crx->id;
      continue;
    }

    if (!it->manifest.browser_min_version.empty()) {
      if (IsVersionNewer(config_->GetBrowserVersion(),
                         it->manifest.browser_min_version)) {
        // The component is not compatible with this Chrome version.
        VLOG(1) << "Ignoring incompatible component: " << crx->id;
        ChangeItemState(crx, CrxUpdateItem::kNoUpdate);
        continue;
      }
    }

    if (it->manifest.packages.size() != 1) {
      // Assume one and only one package per component.
      VLOG(1) << "Ignoring multiple packages for component: " << crx->id;
      ChangeItemState(crx, CrxUpdateItem::kNoUpdate);
      continue;
    }

    // Parse the members of the result and queue an upgrade for this component.
    crx->next_version = Version(it->manifest.version);

    VLOG(1) << "Update found for component: " << crx->id;

    typedef UpdateResponse::Result::Manifest::Package Package;
    const Package& package(it->manifest.packages[0]);
    crx->next_fp = package.fingerprint;

    // Resolve the urls by combining the base urls with the package names.
    for (size_t i = 0; i != it->crx_urls.size(); ++i) {
      const GURL url(it->crx_urls[i].Resolve(package.name));
      if (url.is_valid())
        crx->crx_urls.push_back(url);
    }
    for (size_t i = 0; i != it->crx_diffurls.size(); ++i) {
      const GURL url(it->crx_diffurls[i].Resolve(package.namediff));
      if (url.is_valid())
        crx->crx_diffurls.push_back(url);
    }

    ChangeItemState(crx, CrxUpdateItem::kCanUpdate);
    ++num_updates_pending;
  }

  // All components that are not included in the update response are
  // considered up to date.
  ChangeItemStatus(CrxUpdateItem::kChecking, CrxUpdateItem::kUpToDate);

  // If there are updates pending we do a short wait, otherwise we take
  // a longer delay until we check the components again.
  ScheduleNextRun(num_updates_pending > 0 ? kStepDelayShort : kStepDelayLong);
}

void CrxUpdateService::OnUpdateCheckFailed(int error,
                                           const std::string& error_message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(error);
  size_t count =
      ChangeItemStatus(CrxUpdateItem::kChecking, CrxUpdateItem::kNoUpdate);
  DCHECK_GT(count, 0ul);
  VLOG(1) << "Update check failed.";
  ScheduleNextRun(kStepDelayLong);
}

// Called when progress is being made downloading a CRX. The progress may
// not monotonically increase due to how the CRX downloader switches between
// different downloaders and fallback urls.
void CrxUpdateService::DownloadProgress(
    const std::string& component_id,
    const CrxDownloader::Result& download_result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  NotifyObservers(Observer::COMPONENT_UPDATE_DOWNLOADING, component_id);
}

// Called when the CRX package has been downloaded to a temporary location.
// Here we fire the notifications and schedule the component-specific installer
// to be called in the file thread.
void CrxUpdateService::DownloadComplete(
    scoped_ptr<CRXContext> crx_context,
    const CrxDownloader::Result& download_result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CrxUpdateItem* crx = FindUpdateItemById(crx_context->id);
  DCHECK(crx->status == CrxUpdateItem::kDownloadingDiff ||
         crx->status == CrxUpdateItem::kDownloading);

  AppendDownloadMetrics(crx_downloader_->download_metrics(),
                        &crx->download_metrics);

  crx_downloader_.reset();

  if (download_result.error) {
    if (crx->status == CrxUpdateItem::kDownloadingDiff) {
      crx->diff_error_category = kNetworkError;
      crx->diff_error_code = download_result.error;
      crx->diff_update_failed = true;
      size_t count = ChangeItemStatus(CrxUpdateItem::kDownloadingDiff,
                                      CrxUpdateItem::kCanUpdate);
      DCHECK_EQ(count, 1ul);

      ScheduleNextRun(kStepDelayShort);
      return;
    }
    crx->error_category = kNetworkError;
    crx->error_code = download_result.error;
    size_t count =
        ChangeItemStatus(CrxUpdateItem::kDownloading, CrxUpdateItem::kNoUpdate);
    DCHECK_EQ(count, 1ul);

    // At this point, since both the differential and the full downloads failed,
    // the update for this component has finished with an error.
    ping_manager_->OnUpdateComplete(crx);

    // Move on to the next update, if there is one available.
    ScheduleNextRun(kStepDelayMedium);
  } else {
    size_t count = 0;
    if (crx->status == CrxUpdateItem::kDownloadingDiff) {
      count = ChangeItemStatus(CrxUpdateItem::kDownloadingDiff,
                               CrxUpdateItem::kUpdatingDiff);
    } else {
      count = ChangeItemStatus(CrxUpdateItem::kDownloading,
                               CrxUpdateItem::kUpdating);
    }
    DCHECK_EQ(count, 1ul);

    // Why unretained? See comment at top of file.
    blocking_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CrxUpdateService::Install,
                   base::Unretained(this),
                   base::Passed(&crx_context),
                   download_result.response),
        base::TimeDelta::FromMilliseconds(config_->StepDelay()));
  }
}

// Install consists of digital signature verification, unpacking and then
// calling the component specific installer. All that is handled by the
// |unpacker_|. If there is an error this function is in charge of deleting
// the files created.
void CrxUpdateService::Install(scoped_ptr<CRXContext> context,
                               const base::FilePath& crx_path) {
  // This function owns the file at |crx_path| and the |context| object.
  unpacker_ = new ComponentUnpacker(context->pk_hash,
                                    crx_path,
                                    context->fingerprint,
                                    context->installer,
                                    config_->CreateOutOfProcessPatcher(),
                                    blocking_task_runner_);
  unpacker_->Unpack(base::Bind(&CrxUpdateService::EndUnpacking,
                               base::Unretained(this),
                               context->id,
                               crx_path));
}

void CrxUpdateService::EndUnpacking(const std::string& component_id,
                                    const base::FilePath& crx_path,
                                    ComponentUnpacker::Error error,
                                    int extended_error) {
  if (!DeleteFileAndEmptyParentDirectory(crx_path))
    NOTREACHED() << crx_path.value();
  main_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CrxUpdateService::DoneInstalling,
                 base::Unretained(this),
                 component_id,
                 error,
                 extended_error),
      base::TimeDelta::FromMilliseconds(config_->StepDelay()));
  // Reset the unpacker last, otherwise we free our own arguments.
  unpacker_ = NULL;
}

// Installation has been completed. Adjust the component status and
// schedule the next check. Schedule a short delay before trying the full
// update when the differential update failed.
void CrxUpdateService::DoneInstalling(const std::string& component_id,
                                      ComponentUnpacker::Error error,
                                      int extra_code) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ErrorCategory error_category = kErrorNone;
  switch (error) {
    case ComponentUnpacker::kNone:
      break;
    case ComponentUnpacker::kInstallerError:
      error_category = kInstallError;
      break;
    default:
      error_category = kUnpackError;
      break;
  }

  const bool is_success = error == ComponentUnpacker::kNone;

  CrxUpdateItem* item = FindUpdateItemById(component_id);
  if (item->status == CrxUpdateItem::kUpdatingDiff && !is_success) {
    item->diff_error_category = error_category;
    item->diff_error_code = error;
    item->diff_extra_code1 = extra_code;
    item->diff_update_failed = true;
    size_t count = ChangeItemStatus(CrxUpdateItem::kUpdatingDiff,
                                    CrxUpdateItem::kCanUpdate);
    DCHECK_EQ(count, 1ul);
    ScheduleNextRun(kStepDelayShort);
    return;
  }

  if (is_success) {
    item->component.version = item->next_version;
    item->component.fingerprint = item->next_fp;
    ChangeItemState(item, CrxUpdateItem::kUpdated);
  } else {
    item->error_category = error_category;
    item->error_code = error;
    item->extra_code1 = extra_code;
    ChangeItemState(item, CrxUpdateItem::kNoUpdate);
  }

  ping_manager_->OnUpdateComplete(item);

  // Move on to the next update, if there is one available.
  ScheduleNextRun(kStepDelayMedium);
}

void CrxUpdateService::NotifyObservers(Observer::Events event,
                                       const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(Observer, observer_list_, OnEvent(event, id));
}

ComponentUpdateService::Status CrxUpdateService::OnDemandUpdateWithCooldown(
    CrxUpdateItem* uit) {
  if (!uit)
    return kError;

  // Check if the request is too soon.
  base::TimeDelta delta = base::Time::Now() - uit->last_check;
  if (delta < base::TimeDelta::FromSeconds(config_->OnDemandDelay()))
    return kError;

  return OnDemandUpdateInternal(uit);
}

ComponentUpdateService::Status CrxUpdateService::OnDemandUpdateInternal(
    CrxUpdateItem* uit) {
  if (!uit)
    return kError;

  uit->on_demand = true;

  // If there is an update available for this item, then continue processing
  // the update. This is an artifact of how update checks are done: in addition
  // to the on-demand item, the update check may include other items as well.
  if (uit->status != CrxUpdateItem::kCanUpdate) {
    Status service_status = GetServiceStatus(uit->status);
    // If the item is already in the process of being updated, there is
    // no point in this call, so return kInProgress.
    if (service_status == kInProgress)
      return service_status;

    // Otherwise the item was already checked a while back (or it is new),
    // set its status to kNew to give it a slightly higher priority.
    ChangeItemState(uit, CrxUpdateItem::kNew);
  }

  // In case the current delay is long, set the timer to a shorter value
  // to get the ball rolling.
  if (timer_.IsRunning()) {
    timer_.Stop();
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(config_->StepDelay()),
                 this,
                 &CrxUpdateService::ProcessPendingItems);
  }

  return kOk;
}

ComponentUpdateService::Status CrxUpdateService::GetServiceStatus(
    CrxUpdateItem::Status status) {
  switch (status) {
    case CrxUpdateItem::kChecking:
    case CrxUpdateItem::kCanUpdate:
    case CrxUpdateItem::kDownloadingDiff:
    case CrxUpdateItem::kDownloading:
    case CrxUpdateItem::kUpdatingDiff:
    case CrxUpdateItem::kUpdating:
      return kInProgress;
    case CrxUpdateItem::kNew:
    case CrxUpdateItem::kUpdated:
    case CrxUpdateItem::kUpToDate:
    case CrxUpdateItem::kNoUpdate:
      return kOk;
    case CrxUpdateItem::kLastStatus:
      NOTREACHED() << status;
  }
  return kError;
}

///////////////////////////////////////////////////////////////////////////////

// The component update factory. Using the component updater as a singleton
// is the job of the browser process.
ComponentUpdateService* ComponentUpdateServiceFactory(Configurator* config) {
  DCHECK(config);
  return new CrxUpdateService(config);
}

}  // namespace component_updater
