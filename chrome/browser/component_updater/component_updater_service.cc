// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_service.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/component_patcher.h"
#include "chrome/browser/component_updater/component_unpacker.h"
#include "chrome/browser/component_updater/component_updater_ping_manager.h"
#include "chrome/browser/component_updater/crx_update_item.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::UtilityProcessHost;
using content::UtilityProcessHostClient;
using extensions::Extension;

// The component updater is designed to live until process shutdown, so
// base::Bind() calls are not refcounted.

namespace {

// Extends an omaha compatible update check url |query| string. Does
// not mutate the string if it would be longer than |limit| chars.
bool AddQueryString(const std::string& id,
                    const std::string& version,
                    const std::string& fingerprint,
                    bool ondemand,
                    size_t limit,
                    std::string* query) {
  std::string additional =
      base::StringPrintf("id=%s&v=%s&fp=%s&uc%s",
                         id.c_str(),
                         version.c_str(),
                         fingerprint.c_str(),
                         ondemand ? "&installsource=ondemand" : "");
  additional = "x=" + net::EscapeQueryParamValue(additional, true);
  if ((additional.size() + query->size() + 1) > limit)
    return false;
  if (!query->empty())
    query->append(1, '&');
  query->append(additional);
  return true;
}

// Create the final omaha compatible query. The |extra| is optional and can
// be null. It should contain top level (non-escaped) parameters.
std::string MakeFinalQuery(const std::string& host,
                           const std::string& query,
                           const char* extra) {
  std::string request(host);
  request.append(1, '?');
  if (extra) {
    request.append(extra);
    request.append(1, '&');
  }
  request.append(query);
  return request;
}

// Produces an extension-like friendly |id|. This might be removed in the
// future if we roll our on packing tools.
static std::string HexStringToID(const std::string& hexstr) {
  std::string id;
  for (size_t i = 0; i < hexstr.size(); ++i) {
    int val;
    if (base::HexStringToInt(base::StringPiece(hexstr.begin() + i,
                                               hexstr.begin() + i + 1),
                             &val)) {
      id.append(1, val + 'a');
    } else {
      id.append(1, 'a');
    }
  }
  DCHECK(Extension::IdIsValid(id));
  return id;
}

// Helper to do version check for components.
bool IsVersionNewer(const Version& current, const std::string& proposed) {
  Version proposed_ver(proposed);
  if (!proposed_ver.IsValid())
    return false;
  return (current.CompareTo(proposed_ver) < 0);
}

// Helper template class that allows our main class to have separate
// OnURLFetchComplete() callbacks for different types of url requests
// they are differentiated by the |Ctx| type.
template <typename Del, typename Ctx>
class DelegateWithContext : public net::URLFetcherDelegate {
 public:
  DelegateWithContext(Del* delegate, Ctx* context)
    : delegate_(delegate), context_(context) {}

  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    delegate_->OnURLFetchComplete(source, context_);
    delete this;
  }

 private:
  ~DelegateWithContext() {}

  Del* delegate_;
  Ctx* context_;
};
// This function creates the right DelegateWithContext using template inference.
template <typename Del, typename Ctx>
net::URLFetcherDelegate* MakeContextDelegate(Del* delegate, Ctx* context) {
  return new DelegateWithContext<Del, Ctx>(delegate, context);
}

// Helper to start a url request using |fetcher| with the common flags.
void StartFetch(net::URLFetcher* fetcher,
                net::URLRequestContextGetter* context_getter,
                bool save_to_file,
                scoped_refptr<base::SequencedTaskRunner> task_runner) {
  fetcher->SetRequestContext(context_getter);
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DISABLE_CACHE);
  // TODO(cpu): Define our retry and backoff policy.
  fetcher->SetAutomaticallyRetryOn5xx(false);
  if (save_to_file) {
    fetcher->SaveResponseToTemporaryFile(task_runner);
  }
  fetcher->Start();
}

// Returns true if the url request of |fetcher| was succesful.
bool FetchSuccess(const net::URLFetcher& fetcher) {
  return (fetcher.GetStatus().status() == net::URLRequestStatus::SUCCESS) &&
         (fetcher.GetResponseCode() == 200);
}

// Returns the error code which occured during the fetch.The function returns 0
// if the fetch was successful. If errors happen, the function could return a
// network error, an http response code, or the status of the fetch, if the
// fetch is pending or canceled.
int GetFetchError(const net::URLFetcher& fetcher) {
  if (FetchSuccess(fetcher))
    return 0;

  const net::URLRequestStatus::Status status(fetcher.GetStatus().status());
  if (status == net::URLRequestStatus::FAILED)
    return fetcher.GetStatus().error();

  if (status == net::URLRequestStatus::IO_PENDING ||
      status == net::URLRequestStatus::CANCELED)
    return status;

  const int response_code(fetcher.GetResponseCode());
  if (status == net::URLRequestStatus::SUCCESS && response_code != 200)
    return response_code;

  return -1;
  }

// Returns true if a differential update is available for the update item.
bool IsDiffUpdateAvailable(const CrxUpdateItem* update_item) {
  return update_item->diff_crx_url.is_valid();
}

// Returns true if a differential update is available, it has not failed yet,
// and the configuration allows it.
bool CanTryDiffUpdate(const CrxUpdateItem* update_item,
                      const ComponentUpdateService::Configurator& config) {
  return IsDiffUpdateAvailable(update_item) &&
         !update_item->diff_update_failed &&
         config.DeltasEnabled();
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
    : installer(NULL),
      observer(NULL) {
}

CrxComponent::~CrxComponent() {
}

std::string GetCrxComponentID(const CrxComponent& component) {
  return HexStringToID(StringToLowerASCII(base::HexEncode(&component.pk_hash[0],
                                          component.pk_hash.size()/2)));
}

CrxComponentInfo::CrxComponentInfo() {
}

CrxComponentInfo::~CrxComponentInfo() {
}

///////////////////////////////////////////////////////////////////////////////
// In charge of blocking url requests until the |crx_id| component has been
// updated. This class is touched solely from the IO thread. The UI thread
// can post tasks to it via weak pointers. By default the request is blocked
// unless the CrxUpdateService calls Unblock().
// The lifetime is controlled by Chrome's resource loader so the component
// updater cannot touch objects from this class except via weak pointers.
class CUResourceThrottle
    : public content::ResourceThrottle,
      public base::SupportsWeakPtr<CUResourceThrottle> {
 public:
  explicit CUResourceThrottle(const net::URLRequest* request);
  virtual ~CUResourceThrottle();
  // Overriden from ResourceThrottle.
  virtual void WillStartRequest(bool* defer) OVERRIDE;
  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) OVERRIDE;

  // Component updater calls this function via PostTask to unblock the request.
  void Unblock();

  typedef std::vector<base::WeakPtr<CUResourceThrottle> > WeakPtrVector;

 private:
   enum State {
     NEW,
     BLOCKED,
     UNBLOCKED
   };

   State state_;
};

void UnblockResourceThrottle(base::WeakPtr<CUResourceThrottle> rt) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CUResourceThrottle::Unblock, rt));
}

void UnblockandReapAllThrottles(CUResourceThrottle::WeakPtrVector* throttles) {
  CUResourceThrottle::WeakPtrVector::iterator it;
  for (it = throttles->begin(); it != throttles->end(); ++it)
    UnblockResourceThrottle(*it);
  throttles->clear();
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
// only from the UI thread. The unpack and installation is done in the file
// thread and the network requests are done in the IO thread and in the file
// thread.
class CrxUpdateService : public ComponentUpdateService {
 public:
  explicit CrxUpdateService(ComponentUpdateService::Configurator* config);

  virtual ~CrxUpdateService();

  // Overrides for ComponentUpdateService.
  virtual Status Start() OVERRIDE;
  virtual Status Stop() OVERRIDE;
  virtual Status RegisterComponent(const CrxComponent& component) OVERRIDE;
  virtual Status OnDemandUpdate(const std::string& component_id) OVERRIDE;
  virtual void GetComponents(
      std::vector<CrxComponentInfo>* components) OVERRIDE;
  virtual content::ResourceThrottle* GetOnDemandResourceThrottle(
      net::URLRequest* request, const std::string& crx_id) OVERRIDE;

  // The only purpose of this class is to forward the
  // UtilityProcessHostClient callbacks so CrxUpdateService does
  // not have to derive from it because that is refcounted.
  class ManifestParserBridge : public UtilityProcessHostClient {
   public:
    explicit ManifestParserBridge(CrxUpdateService* service)
        : service_(service) {}

    virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
      bool handled = true;
      IPC_BEGIN_MESSAGE_MAP(ManifestParserBridge, message)
        IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseUpdateManifest_Succeeded,
                            OnParseUpdateManifestSucceeded)
        IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseUpdateManifest_Failed,
                            OnParseUpdateManifestFailed)
        IPC_MESSAGE_UNHANDLED(handled = false)
      IPC_END_MESSAGE_MAP()
      return handled;
    }

   private:
    virtual ~ManifestParserBridge() {}

    // Omaha update response XML was successfully parsed.
    void OnParseUpdateManifestSucceeded(const UpdateManifest::Results& r) {
      service_->OnParseUpdateManifestSucceeded(r);
    }
    // Omaha update response XML could not be parsed.
    void OnParseUpdateManifestFailed(const std::string& e) {
      service_->OnParseUpdateManifestFailed(e);
    }

    CrxUpdateService* service_;
    DISALLOW_COPY_AND_ASSIGN(ManifestParserBridge);
  };

  // Context for a update check url request. See DelegateWithContext above.
  struct UpdateContext {
    base::Time start;
    UpdateContext() : start(base::Time::Now()) {}
  };

  // Context for a crx download url request. See DelegateWithContext above.
  struct CRXContext {
    ComponentInstaller* installer;
    std::vector<uint8> pk_hash;
    std::string id;
    std::string fingerprint;
    CRXContext() : installer(NULL) {}
  };

  void OnURLFetchComplete(const net::URLFetcher* source,
                          UpdateContext* context);

  void OnURLFetchComplete(const net::URLFetcher* source,
                          CRXContext* context);

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

  // See ManifestParserBridge.
  void OnParseUpdateManifestSucceeded(const UpdateManifest::Results& results);

  // See ManifestParserBridge.
  void OnParseUpdateManifestFailed(const std::string& error_message);

  bool AddItemToUpdateCheck(CrxUpdateItem* item, std::string* query);

  Status OnDemandUpdateInternal(CrxUpdateItem* item);

  void ProcessPendingItems();

  CrxUpdateItem* FindReadyComponent();

  void UpdateComponent(CrxUpdateItem* workitem);

  void AddUpdateCheckItems(std::string* query);

  void DoUpdateCheck(const std::string& query);

  void ScheduleNextRun(StepDelayInterval step_delay);

  void ParseManifest(const std::string& xml);

  void Install(const CRXContext* context, const base::FilePath& crx_path);

  void DoneInstalling(const std::string& component_id,
                      ComponentUnpacker::Error error,
                      int extended_error);

  void ChangeItemState(CrxUpdateItem* item, CrxUpdateItem::Status to);

  size_t ChangeItemStatus(CrxUpdateItem::Status from,
                          CrxUpdateItem::Status to);

  CrxUpdateItem* FindUpdateItemById(const std::string& id);

  void NotifyComponentObservers(ComponentObserver::Events event,
                                int extra) const;

  bool HasOnDemandItems() const;

  void OnNewResourceThrottle(base::WeakPtr<CUResourceThrottle> rt,
                             const std::string& crx_id);

  scoped_ptr<ComponentUpdateService::Configurator> config_;

  scoped_ptr<ComponentPatcher> component_patcher_;

  scoped_ptr<net::URLFetcher> url_fetcher_;

  scoped_ptr<component_updater::PingManager> ping_manager_;

  // A collection of every work item.
  typedef std::vector<CrxUpdateItem*> UpdateItems;
  UpdateItems work_items_;

  base::OneShotTimer<CrxUpdateService> timer_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  const Version chrome_version_;

  bool running_;

  DISALLOW_COPY_AND_ASSIGN(CrxUpdateService);
};

//////////////////////////////////////////////////////////////////////////////

CrxUpdateService::CrxUpdateService(ComponentUpdateService::Configurator* config)
    : config_(config),
      component_patcher_(config->CreateComponentPatcher()),
      ping_manager_(new component_updater::PingManager(
          config->PingUrl(),
          config->RequestContext())),
      blocking_task_runner_(BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(
              BrowserThread::GetBlockingPool()->GetSequenceToken(),
              base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)),
      chrome_version_(chrome::VersionInfo().Version()),
      running_(false) {
}

CrxUpdateService::~CrxUpdateService() {
  // Because we are a singleton, at this point only the UI thread should be
  // alive, this simplifies the management of the work that could be in
  // flight in other threads.
  Stop();
  STLDeleteElements(&work_items_);
}

ComponentUpdateService::Status CrxUpdateService::Start() {
  // Note that RegisterComponent will call Start() when the first
  // component is registered, so it can be called twice. This way
  // we avoid scheduling the timer if there is no work to do.
  running_ = true;
  if (work_items_.empty())
    return kOk;

  NotifyComponentObservers(ComponentObserver::COMPONENT_UPDATER_STARTED, 0);

  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(config_->InitialDelay()),
               this, &CrxUpdateService::ProcessPendingItems);
  return kOk;
}

// Stop the main check + update loop. In flight operations will be
// completed.
ComponentUpdateService::Status CrxUpdateService::Stop() {
  running_ = false;
  timer_.Stop();
  return kOk;
}

bool CrxUpdateService::HasOnDemandItems() const {
  class Helper {
   public:
    static bool IsOnDemand(CrxUpdateItem* item) {
      return item->on_demand;
    }
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(url_fetcher_.get() == NULL);
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
    NotifyComponentObservers(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0);

    // Zero is only used for unit tests.
    if (0 == delay_seconds)
      return;
  }

  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(delay_seconds),
               this, &CrxUpdateService::ProcessPendingItems);
}

// Given a extension-like component id, find the associated component.
CrxUpdateItem* CrxUpdateService::FindUpdateItemById(const std::string& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CrxUpdateItem::FindById finder(id);
  UpdateItems::iterator it = std::find_if(work_items_.begin(),
                                          work_items_.end(),
                                          finder);
  if (it == work_items_.end())
    return NULL;
  return (*it);
}

// Changes a component's status, clearing on_demand and firing notifications as
// necessary. By convention, this is the only function that can change a
// CrxUpdateItem's |status|.
// TODO(waffles): Do we want to add DCHECKS for valid state transitions here?
void CrxUpdateService::ChangeItemState(CrxUpdateItem* item,
                                       CrxUpdateItem::Status to) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (to == CrxUpdateItem::kNoUpdate ||
      to == CrxUpdateItem::kUpdated ||
      to == CrxUpdateItem::kUpToDate) {
    item->on_demand = false;
  }

  item->status = to;

  ComponentObserver* observer = item->component.observer;
  if (observer) {
    switch (to) {
      case CrxUpdateItem::kCanUpdate:
        observer->OnEvent(ComponentObserver::COMPONENT_UPDATE_FOUND, 0);
        break;
      case CrxUpdateItem::kUpdatingDiff:
      case CrxUpdateItem::kUpdating:
        observer->OnEvent(ComponentObserver::COMPONENT_UPDATE_READY, 0);
        break;
      case CrxUpdateItem::kUpdated:
        observer->OnEvent(ComponentObserver::COMPONENT_UPDATED, 0);
        break;
      case CrxUpdateItem::kUpToDate:
      case CrxUpdateItem::kNoUpdate:
        observer->OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0);
        break;
      case CrxUpdateItem::kNew:
      case CrxUpdateItem::kChecking:
      case CrxUpdateItem::kDownloading:
      case CrxUpdateItem::kDownloadingDiff:
      case CrxUpdateItem::kLastStatus:
        // No notification for these states.
        break;
    }
  }

  // Free possible pending network requests.
  if ((to == CrxUpdateItem::kUpdated) ||
      (to == CrxUpdateItem::kUpToDate) ||
      (to == CrxUpdateItem::kNoUpdate)) {
    UnblockandReapAllThrottles(&item->throttles);
  }
}

// Changes all the components in |work_items_| that have |from| status to
// |to| status and returns how many have been changed.
size_t CrxUpdateService::ChangeItemStatus(CrxUpdateItem::Status from,
                                          CrxUpdateItem::Status to) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  size_t count = 0;
  for (UpdateItems::iterator it = work_items_.begin();
       it != work_items_.end(); ++it) {
    CrxUpdateItem* item = *it;
    if (item->status != from)
      continue;
    ChangeItemState(item, to);
    ++count;
  }
  return count;
}

// Adds a component to be checked for upgrades. If the component exists it
// it will be replaced and the return code is kReplaced.
//
// TODO(cpu): Evaluate if we want to support un-registration.
ComponentUpdateService::Status CrxUpdateService::RegisterComponent(
    const CrxComponent& component) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (component.pk_hash.empty() ||
      !component.version.IsValid() ||
      !component.installer)
    return kError;

  std::string id =
    HexStringToID(StringToLowerASCII(base::HexEncode(&component.pk_hash[0],
                                     component.pk_hash.size()/2)));
  CrxUpdateItem* uit;
  uit = FindUpdateItemById(id);
  if (uit) {
    uit->component = component;
    return kReplaced;
  }

  uit = new CrxUpdateItem;
  uit->id.swap(id);
  uit->component = component;

  work_items_.push_back(uit);
  // If this is the first component registered we call Start to
  // schedule the first timer.
  if (running_ && (work_items_.size() == 1))
    Start();

  return kOk;
}

// Sets a component to be checked for updates.
// The component to add is |item| and the |query| string is modified with the
// required omaha compatible query. Returns false when the query string is
// longer than specified by UrlSizeLimit().
bool CrxUpdateService::AddItemToUpdateCheck(CrxUpdateItem* item,
                                            std::string* query) {
  if (!AddQueryString(item->id,
                      item->component.version.GetString(),
                      item->component.fingerprint,
                      item->on_demand,
                      config_->UrlSizeLimit(),
                      query))
    return false;

  ChangeItemState(item, CrxUpdateItem::kChecking);
  item->last_check = base::Time::Now();
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
  return true;
}

// Start the process of checking for an update, for a particular component
// that was previously registered.
// |component_id| is a value returned from GetCrxComponentID().
ComponentUpdateService::Status CrxUpdateService::OnDemandUpdate(
    const std::string& component_id) {
  return OnDemandUpdateInternal(FindUpdateItemById(component_id));
}

ComponentUpdateService::Status CrxUpdateService::OnDemandUpdateInternal(
    CrxUpdateItem* uit) {
  if (!uit)
    return kError;
  // Check if the request is too soon.
  base::TimeDelta delta = base::Time::Now() - uit->last_check;
  if (delta < base::TimeDelta::FromSeconds(config_->OnDemandDelay()))
    return kError;

  switch (uit->status) {
    // If the item is already in the process of being updated, there is
    // no point in this call, so return kInProgress.
    case CrxUpdateItem::kChecking:
    case CrxUpdateItem::kCanUpdate:
    case CrxUpdateItem::kDownloadingDiff:
    case CrxUpdateItem::kDownloading:
    case CrxUpdateItem::kUpdatingDiff:
    case CrxUpdateItem::kUpdating:
      return kInProgress;
    // Otherwise the item was already checked a while back (or it is new),
    // set its status to kNew to give it a slightly higher priority.
    case CrxUpdateItem::kNew:
    case CrxUpdateItem::kUpdated:
    case CrxUpdateItem::kUpToDate:
    case CrxUpdateItem::kNoUpdate:
      ChangeItemState(uit, CrxUpdateItem::kNew);
      uit->on_demand = true;
      break;
    case CrxUpdateItem::kLastStatus:
      NOTREACHED() << uit->status;
  }

  // In case the current delay is long, set the timer to a shorter value
  // to get the ball rolling.
  if (timer_.IsRunning()) {
    timer_.Stop();
    timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(config_->StepDelay()),
                 this, &CrxUpdateService::ProcessPendingItems);
  }

  return kOk;
}

void CrxUpdateService::GetComponents(
    std::vector<CrxComponentInfo>* components) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (UpdateItems::const_iterator it = work_items_.begin();
       it != work_items_.end(); ++it) {
    const CrxUpdateItem* item = *it;
    CrxComponentInfo info;
    info.id = GetCrxComponentID(item->component);
    info.version = item->component.version.GetString();
    info.name = item->component.name;
    components->push_back(info);
  }
}

// This is the main loop of the component updater.
void CrxUpdateService::ProcessPendingItems() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CrxUpdateItem* ready_upgrade = FindReadyComponent();
  if (ready_upgrade) {
    UpdateComponent(ready_upgrade);
    return;
  }
  std::string query;
  AddUpdateCheckItems(&query);
  if (!query.empty()) {
    DoUpdateCheck(query);
    return;
  }
  // No components to update. The next check will be after a long sleep.
  ScheduleNextRun(kStepDelayLong);
}

CrxUpdateItem* CrxUpdateService::FindReadyComponent() {
  class Helper {
   public:
    static bool IsReadyOnDemand(CrxUpdateItem* item) {
      return item->on_demand && IsReady(item);
    }
    static bool IsReady(CrxUpdateItem* item) {
      return item->status == CrxUpdateItem::kCanUpdate;
    }
  };

  std::vector<CrxUpdateItem*>::iterator it = std::find_if(
      work_items_.begin(), work_items_.end(), Helper::IsReadyOnDemand);
  if (it != work_items_.end())
    return *it;
  it = std::find_if(work_items_.begin(), work_items_.end(), Helper::IsReady);
  if (it != work_items_.end())
    return *it;
  return NULL;
}

void CrxUpdateService::UpdateComponent(CrxUpdateItem* workitem) {
  CRXContext* context = new CRXContext;
  context->pk_hash = workitem->component.pk_hash;
  context->id = workitem->id;
  context->installer = workitem->component.installer;
  context->fingerprint = workitem->next_fp;
  GURL package_url;
  if (CanTryDiffUpdate(workitem, *config_)) {
    package_url = workitem->diff_crx_url;
    ChangeItemState(workitem, CrxUpdateItem::kDownloadingDiff);
  } else {
    package_url = workitem->crx_url;
    ChangeItemState(workitem, CrxUpdateItem::kDownloading);
  }
  url_fetcher_.reset(net::URLFetcher::Create(
      0, package_url, net::URLFetcher::GET,
      MakeContextDelegate(this, context)));
  StartFetch(url_fetcher_.get(),
             config_->RequestContext(),
             true,
             blocking_task_runner_);
}

// Given that our |work_items_| list is expected to contain relatively few
// items, we simply loop several times.
void CrxUpdateService::AddUpdateCheckItems(std::string* query){
  for (UpdateItems::const_iterator it = work_items_.begin();
       it != work_items_.end(); ++it) {
    CrxUpdateItem* item = *it;
    if (item->status != CrxUpdateItem::kNew)
      continue;
    if (!AddItemToUpdateCheck(item, query))
      break;
  }

  // Next we can go back to components we already checked, here
  // we can also batch them in a single url request, as long as
  // we have not checked them recently.
  const base::TimeDelta min_delta_time =
      base::TimeDelta::FromSeconds(config_->MinimumReCheckWait());

  for (UpdateItems::const_iterator it = work_items_.begin();
       it != work_items_.end(); ++it) {
    CrxUpdateItem* item = *it;
    if ((item->status != CrxUpdateItem::kNoUpdate) &&
        (item->status != CrxUpdateItem::kUpToDate))
      continue;
    base::TimeDelta delta = base::Time::Now() - item->last_check;
    if (delta < min_delta_time)
      continue;
    if (!AddItemToUpdateCheck(item, query))
      break;
  }

  // Finally, we check components that we already updated as long as
  // we have not checked them recently.
  for (UpdateItems::const_iterator it = work_items_.begin();
       it != work_items_.end(); ++it) {
    CrxUpdateItem* item = *it;
    if (item->status != CrxUpdateItem::kUpdated)
      continue;
    base::TimeDelta delta = base::Time::Now() - item->last_check;
    if (delta < min_delta_time)
      continue;
    if (!AddItemToUpdateCheck(item, query))
      break;
  }
}

void CrxUpdateService::DoUpdateCheck(const std::string& query) {
  const std::string full_query =
      MakeFinalQuery(config_->UpdateUrl().spec(),
                     query,
                     config_->ExtraRequestParams());

  url_fetcher_.reset(net::URLFetcher::Create(
      0, GURL(full_query), net::URLFetcher::GET,
      MakeContextDelegate(this, new UpdateContext())));
  StartFetch(url_fetcher_.get(),
             config_->RequestContext(),
             false,
             blocking_task_runner_);
}

// Called when we got a response from the update server. It consists of an xml
// document following the omaha update scheme.
void CrxUpdateService::OnURLFetchComplete(const net::URLFetcher* source,
                                          UpdateContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (FetchSuccess(*source)) {
    std::string xml;
    source->GetResponseAsString(&xml);
    url_fetcher_.reset();
    ParseManifest(xml);
  } else {
    url_fetcher_.reset();
    CrxUpdateService::OnParseUpdateManifestFailed("network error");
  }
  delete context;
}

// Parsing the manifest is either done right now for tests or in a sandboxed
// process for the production environment. This mitigates the case where an
// attacker was able to feed us a malicious xml string.
void CrxUpdateService::ParseManifest(const std::string& xml) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (config_->InProcess()) {
    UpdateManifest manifest;
    if (!manifest.Parse(xml))
       CrxUpdateService::OnParseUpdateManifestFailed(manifest.errors());
    else
       CrxUpdateService::OnParseUpdateManifestSucceeded(manifest.results());
  } else {
    UtilityProcessHost* host =
        UtilityProcessHost::Create(new ManifestParserBridge(this),
                                   base::MessageLoopProxy::current().get());
    host->Send(new ChromeUtilityMsg_ParseUpdateManifest(xml));
  }
}

// A valid Omaha update check has arrived, from only the list of components that
// we are currently upgrading we check for a match in which the server side
// version is newer, if so we queue them for an upgrade. The next time we call
// ProcessPendingItems() one of them will be drafted for the upgrade process.
void CrxUpdateService::OnParseUpdateManifestSucceeded(
    const UpdateManifest::Results& results) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  size_t update_pending = 0;
  std::vector<UpdateManifest::Result>::const_iterator it;
  for (it = results.list.begin(); it != results.list.end(); ++it) {
    CrxUpdateItem* crx = FindUpdateItemById(it->extension_id);
    if (!crx)
      continue;

    if (crx->status != CrxUpdateItem::kChecking)
      continue;  // Not updating this component now.

    if (it->version.empty()) {
      // No version means no update available.
      ChangeItemState(crx, CrxUpdateItem::kNoUpdate);
      continue;
    }
    if (!IsVersionNewer(crx->component.version, it->version)) {
      // Our component is up to date.
      ChangeItemState(crx, CrxUpdateItem::kUpToDate);
      continue;
    }
    if (!it->browser_min_version.empty()) {
      if (IsVersionNewer(chrome_version_, it->browser_min_version)) {
        // Does not apply for this chrome version.
        ChangeItemState(crx, CrxUpdateItem::kNoUpdate);
        continue;
      }
    }
    // All test passed. Queue an upgrade for this component and fire the
    // notifications.
    crx->crx_url = it->crx_url;
    crx->diff_crx_url = it->diff_crx_url;
    ChangeItemState(crx, CrxUpdateItem::kCanUpdate);
    crx->next_version = Version(it->version);
    crx->next_fp = it->package_fingerprint;
    ++update_pending;
  }

  // All the components that are not mentioned in the manifest we
  // consider them up to date.
  ChangeItemStatus(CrxUpdateItem::kChecking, CrxUpdateItem::kUpToDate);

  // If there are updates pending we do a short wait, otherwise we take
  // a longer delay until we check the components again.
  ScheduleNextRun(update_pending > 0 ? kStepDelayShort : kStepDelayMedium);
}

void CrxUpdateService::OnParseUpdateManifestFailed(
    const std::string& error_message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  size_t count = ChangeItemStatus(CrxUpdateItem::kChecking,
                                  CrxUpdateItem::kNoUpdate);
  DCHECK_GT(count, 0ul);
  ScheduleNextRun(kStepDelayLong);
}

// Called when the CRX package has been downloaded to a temporary location.
// Here we fire the notifications and schedule the component-specific installer
// to be called in the file thread.
void CrxUpdateService::OnURLFetchComplete(const net::URLFetcher* source,
                                          CRXContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<CRXContext> crx_context(context);

  CrxUpdateItem* crx = FindUpdateItemById(crx_context->id);
  DCHECK(crx->status == CrxUpdateItem::kDownloadingDiff ||
         crx->status == CrxUpdateItem::kDownloading);

  if (!FetchSuccess(*source)) {
    if (crx->status == CrxUpdateItem::kDownloadingDiff) {
      crx->diff_error_category = kNetworkError;
      crx->diff_error_code = GetFetchError(*source);
      crx->diff_update_failed = true;
      size_t count = ChangeItemStatus(CrxUpdateItem::kDownloadingDiff,
                                      CrxUpdateItem::kCanUpdate);
      DCHECK_EQ(count, 1ul);
      url_fetcher_.reset();

      ScheduleNextRun(kStepDelayShort);
      return;
    }
    crx->error_category = kNetworkError;
    crx->error_code = GetFetchError(*source);
    size_t count = ChangeItemStatus(CrxUpdateItem::kDownloading,
                                    CrxUpdateItem::kNoUpdate);
    DCHECK_EQ(count, 1ul);
    url_fetcher_.reset();

    // At this point, since both the differential and the full downloads failed,
    // the update for this component has finished with an error.
    ping_manager_->OnUpdateComplete(crx);

    // Move on to the next update, if there is one available.
    ScheduleNextRun(kStepDelayMedium);
  } else {
    base::FilePath temp_crx_path;
    CHECK(source->GetResponseAsFilePath(true, &temp_crx_path));

    size_t count = 0;
    if (crx->status == CrxUpdateItem::kDownloadingDiff) {
      count = ChangeItemStatus(CrxUpdateItem::kDownloadingDiff,
                               CrxUpdateItem::kUpdatingDiff);
    } else {
      count = ChangeItemStatus(CrxUpdateItem::kDownloading,
                               CrxUpdateItem::kUpdating);
    }
    DCHECK_EQ(count, 1ul);

    url_fetcher_.reset();

    // Why unretained? See comment at top of file.
    blocking_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CrxUpdateService::Install,
                   base::Unretained(this),
                   crx_context.release(),
                   temp_crx_path),
        base::TimeDelta::FromMilliseconds(config_->StepDelay()));
  }
}

// Install consists of digital signature verification, unpacking and then
// calling the component specific installer. All that is handled by the
// |unpacker|. If there is an error this function is in charge of deleting
// the files created.
void CrxUpdateService::Install(const CRXContext* context,
                               const base::FilePath& crx_path) {
  // This function owns the file at |crx_path| and the |context| object.
  ComponentUnpacker unpacker(context->pk_hash,
                             crx_path,
                             context->fingerprint,
                             component_patcher_.get(),
                             context->installer);
  if (!base::DeleteFile(crx_path, false))
    NOTREACHED() << crx_path.value();
  // Why unretained? See comment at top of file.
  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CrxUpdateService::DoneInstalling, base::Unretained(this),
                 context->id, unpacker.error(), unpacker.extended_error()),
      base::TimeDelta::FromMilliseconds(config_->StepDelay()));
  delete context;
}

// Installation has been completed. Adjust the component status and
// schedule the next check. Schedule a short delay before trying the full
// update when the differential update failed.
void CrxUpdateService::DoneInstalling(const std::string& component_id,
                                      ComponentUnpacker::Error error,
                                      int extra_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
    ChangeItemState(item, CrxUpdateItem::kUpdated);
    item->component.version = item->next_version;
    item->component.fingerprint = item->next_fp;
  } else {
    ChangeItemState(item, CrxUpdateItem::kNoUpdate);
    item->error_category = error_category;
    item->error_code = error;
    item->extra_code1 = extra_code;
  }

  ping_manager_->OnUpdateComplete(item);

  // Move on to the next update, if there is one available.
  ScheduleNextRun(kStepDelayMedium);
}

void CrxUpdateService::NotifyComponentObservers(
    ComponentObserver::Events event, int extra) const {
  for (UpdateItems::const_iterator it = work_items_.begin();
       it != work_items_.end(); ++it) {
    ComponentObserver* observer = (*it)->component.observer;
    if (observer)
      observer->OnEvent(event, 0);
  }
}

content::ResourceThrottle* CrxUpdateService::GetOnDemandResourceThrottle(
     net::URLRequest* request, const std::string& crx_id) {
  // We give the raw pointer to the caller, who will delete it at will
  // and we keep for ourselves a weak pointer to it so we can post tasks
  // from the UI thread without having to track lifetime directly.
  CUResourceThrottle* rt = new CUResourceThrottle(request);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CrxUpdateService::OnNewResourceThrottle,
                 base::Unretained(this),
                 rt->AsWeakPtr(),
                 crx_id));
  return rt;
}

void CrxUpdateService::OnNewResourceThrottle(
    base::WeakPtr<CUResourceThrottle> rt, const std::string& crx_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Check if we can on-demand update, else unblock the request anyway.
  CrxUpdateItem* item = FindUpdateItemById(crx_id);
  Status status = OnDemandUpdateInternal(item);
  if (status == kOk || status == kInProgress) {
    item->throttles.push_back(rt);
    return;
  }
  UnblockResourceThrottle(rt);
}

///////////////////////////////////////////////////////////////////////////////

CUResourceThrottle::CUResourceThrottle(const net::URLRequest* request)
    : state_(NEW) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

CUResourceThrottle::~CUResourceThrottle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void CUResourceThrottle::WillStartRequest(bool* defer) {
  if (state_ != UNBLOCKED) {
    state_ = BLOCKED;
    *defer = true;
  } else {
    *defer = false;
  }
}

void CUResourceThrottle::WillRedirectRequest(const GURL& new_url, bool* defer) {
  WillStartRequest(defer);
}

void CUResourceThrottle::Unblock() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (state_ == BLOCKED)
    controller()->Resume();
  state_ = UNBLOCKED;
}

// The component update factory. Using the component updater as a singleton
// is the job of the browser process.
ComponentUpdateService* ComponentUpdateServiceFactory(
    ComponentUpdateService::Configurator* config) {
  DCHECK(config);
  return new CrxUpdateService(config);
}
