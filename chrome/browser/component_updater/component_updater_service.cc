// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_service.h"

#include <algorithm>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/component_unpacker.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/utility_process_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "content/public/common/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"

using content::BrowserThread;

// The component updater is designed to live until process shutdown, so
// base::Bind() calls are not refcounted.

namespace {
// Extends an omaha compatible update check url |query| string. Does
// not mutate the string if it would be longer than |limit| chars.
bool AddQueryString(std::string id, std::string version,
                    size_t limit, std::string* query) {
  std::string additional =
      base::StringPrintf("id=%s&v=%s&uc", id.c_str(), version.c_str());
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
    if (base::HexStringToInt(hexstr.begin() + i, hexstr.begin() + i + 1, &val))
      id.append(1, val + 'a');
    else
      id.append(1, 'a');
  }
  DCHECK(Extension::IdIsValid(id));
  return id;
}

// Returns given a crx id it returns a small number, less than 100, that has a
// decent chance of being unique among the registered components. It also has
// the nice property that can be trivially computed by hand.
static int CrxIdtoUMAId(const std::string& id) {
  CHECK(id.size() > 2);
  return id[0] + id[1] + id[2] - ('a' * 3);
}

// Helper to do version check for components.
bool IsVersionNewer(const Version& current, const std::string& proposed) {
  Version proposed_ver(proposed);
  if (!proposed_ver.IsValid())
    return false;
  return (current.CompareTo(proposed_ver) < 0);
}

// Helper template class that allows our main class to have separate
// OnURLFetchComplete() callbacks for diffent types of url requests
// they are differentiated by the |Ctx| type.
template <typename Del, typename Ctx>
class DelegateWithContext : public content::URLFetcherDelegate {
 public:
  DelegateWithContext(Del* delegate, Ctx* context)
    : delegate_(delegate), context_(context) {}

  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE {
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
content::URLFetcherDelegate* MakeContextDelegate(Del* delegate, Ctx* context) {
  return new DelegateWithContext<Del, Ctx>(delegate, context);
}

// Helper to start a url request using |fetcher| with the common flags.
void StartFetch(content::URLFetcher* fetcher,
                net::URLRequestContextGetter* context_getter,
                bool save_to_file) {
  fetcher->SetRequestContext(context_getter);
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DISABLE_CACHE);
  // TODO(cpu): Define our retry and backoff policy.
  fetcher->SetAutomaticallyRetryOn5xx(false);
  if (save_to_file) {
    fetcher->SaveResponseToTemporaryFile(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  }
  fetcher->Start();
}

// Returs true if the url request of |fetcher| was succesful.
bool FetchSuccess(const content::URLFetcher& fetcher) {
  return (fetcher.GetStatus().status() == net::URLRequestStatus::SUCCESS) &&
         (fetcher.GetResponseCode() == 200);
}

// This is the one and only per-item state structure. Designed to be hosted
// in a std::vector or a std::list. The two main members are |component|
// which is supplied by the the component updater client and |status| which
// is modified as the item is processed by the update pipeline. The expected
// transition graph is:
//                                error          error         error
//              +--kNoUpdate<------<-------+------<------+------<------+
//              |                          |             |             |
//              V                  yes     |             |             |
//  kNew --->kChecking-->[update?]----->kCanUpdate-->kDownloading-->kUpdating
//              ^               |                                      |
//              |               |no                                    |
//              |--kUpToDate<---+                                      |
//              |                                  success             |
//              +--kUpdated<-------------------------------------------+
//
struct CrxUpdateItem {
  enum Status {
    kNew,
    kChecking,
    kCanUpdate,
    kDownloading,
    kUpdating,
    kUpdated,
    kUpToDate,
    kNoUpdate,
    kLastStatus
  };

  Status status;
  GURL crx_url;
  std::string id;
  base::Time last_check;
  CrxComponent component;
  Version next_version;

  CrxUpdateItem() : status(kNew) {}

  // Function object used to find a specific component.
  class FindById {
   public:
    explicit FindById(const std::string& id) : id_(id) {}

    bool operator() (CrxUpdateItem* item) const {
      return (item->id == id_);
    }
   private:
    const std::string& id_;
  };
};

}  // namespace.

typedef ComponentUpdateService::Configurator Config;

CrxComponent::CrxComponent() {}
CrxComponent::~CrxComponent() {}

//////////////////////////////////////////////////////////////////////////////
// The one and only implementation of the ComponentUpdateService interface. In
// charge of running the show. The main method is ProcessPendingItems() which
// is called periodically to do the updgrades/installs or the update checks.
// An important consideration here is to be as "low impact" as we can to the
// rest of the browser, so even if we have many components registered and
// elegible for update, we only do one thing at a time with pauses in between
// the tasks. Also when we do network requests there is only one |url_fetcher_|
// in flight at at a time.
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

  // The only purpose of this class is to forward the
  // UtilityProcessHost::Client callbacks so CrxUpdateService does
  // not have to derive from it because that is refcounted.
  class ManifestParserBridge : public UtilityProcessHost::Client {
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
    // Omaha update response XML was succesfuly parsed.
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
    CRXContext() : installer(NULL) {}
  };

  void OnURLFetchComplete(const content::URLFetcher* source,
                          UpdateContext* context);

  void OnURLFetchComplete(const content::URLFetcher* source,
                          CRXContext* context);

 private:
  // See ManifestParserBridge.
  void OnParseUpdateManifestSucceeded(
      const UpdateManifest::Results& results);

  // See ManifestParserBridge.
  void OnParseUpdateManifestFailed(
      const std::string& error_message);

  bool AddItemToUpdateCheck(CrxUpdateItem* item, std::string* query);

  void ProcessPendingItems();

  void ScheduleNextRun(bool step_delay);

  void ParseManifest(const std::string& xml);

  void Install(const CRXContext* context, const FilePath& crx_path);

  void DoneInstalling(const std::string& component_id,
                      ComponentUnpacker::Error error);

  size_t ChangeItemStatus(CrxUpdateItem::Status from,
                          CrxUpdateItem::Status to);

  CrxUpdateItem* FindUpdateItemById(const std::string& id);

  scoped_ptr<Config> config_;

  scoped_ptr<content::URLFetcher> url_fetcher_;

  typedef std::vector<CrxUpdateItem*> UpdateItems;
  UpdateItems work_items_;

  base::OneShotTimer<CrxUpdateService> timer_;

  Version chrome_version_;

  bool running_;

  DISALLOW_COPY_AND_ASSIGN(CrxUpdateService);
};

//////////////////////////////////////////////////////////////////////////////

CrxUpdateService::CrxUpdateService(
    ComponentUpdateService::Configurator* config)
    : config_(config),
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

  content::NotificationService::current()->Notify(
    chrome::NOTIFICATION_COMPONENT_UPDATER_STARTED,
    content::Source<ComponentUpdateService>(this),
    content::NotificationService::NoDetails());

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

// This function sets the timer which will call ProcessPendingItems() there
// are two kind of waits, the short one (with step_delay = true) and the
// long one.
void CrxUpdateService::ScheduleNextRun(bool step_delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(url_fetcher_.get() == NULL);
  CHECK(!timer_.IsRunning());
  // It could be the case that Stop() had been called while a url request
  // or unpacking was in flight, if so we arrive here but |running_| is
  // false. In that case do not loop again.
  if (!running_)
    return;

  int64 delay = step_delay ? config_->StepDelay() : config_->NextCheckDelay();

  if (!step_delay) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_COMPONENT_UPDATER_SLEEPING,
        content::Source<ComponentUpdateService>(this),
        content::NotificationService::NoDetails());
    // Zero is only used for unit tests.
    if (0 == delay)
      return;
  }

  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(delay),
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

// Changes all the components in |work_items_| that have |from| status to
// |to| statatus and returns how many have been changed.
size_t CrxUpdateService::ChangeItemStatus(CrxUpdateItem::Status from,
                                          CrxUpdateItem::Status to) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  size_t count = 0;
  for (UpdateItems::iterator it = work_items_.begin();
       it != work_items_.end(); ++it) {
    CrxUpdateItem* item = *it;
    if (item->status != from)
      continue;
    item->status = to;
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
// The componet to add is |crxit| and the |query| string is modified with the
// required omaha compatible query. Returns false when the query strings
// is longer than specified by UrlSizeLimit().
bool CrxUpdateService::AddItemToUpdateCheck(CrxUpdateItem* item,
                                            std::string* query) {
  if (!AddQueryString(item->id,
                      item->component.version.GetString(),
                      config_->UrlSizeLimit(), query))
    return false;
  item->status = CrxUpdateItem::kChecking;
  item->last_check = base::Time::Now();
  return true;
}

// Here is where the work gets scheduled. Given that our |work_items_| list
// is expected to be ten or less items, we simply loop several times.
void CrxUpdateService::ProcessPendingItems() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // First check for ready upgrades and do one. The first
  // step is to fetch the crx package.
  for (UpdateItems::const_iterator it = work_items_.begin();
       it != work_items_.end(); ++it) {
    CrxUpdateItem* item = *it;
    if (item->status != CrxUpdateItem::kCanUpdate)
      continue;
    // Found component to update, start the process.
    item->status = CrxUpdateItem::kDownloading;
    CRXContext* context = new CRXContext;
    context->pk_hash = item->component.pk_hash;
    context->id = item->id;
    context->installer = item->component.installer;
    url_fetcher_.reset(content::URLFetcher::Create(
        0, item->crx_url, content::URLFetcher::GET,
        MakeContextDelegate(this, context)));
    StartFetch(url_fetcher_.get(), config_->RequestContext(), true);
    return;
  }

  std::string query;
  // If no pending upgrades, we check the if there are new
  // components we have not checked against the server. We
  // can batch a bunch in a single url request.
  for (UpdateItems::const_iterator it = work_items_.begin();
       it != work_items_.end(); ++it) {
    CrxUpdateItem* item = *it;
    if (item->status != CrxUpdateItem::kNew)
      continue;
    if (!AddItemToUpdateCheck(item, &query))
      break;
  }

  // Next we can go back to components we already checked, here
  // we can also batch them in a single url request, as long as
  // we have not checked them recently.
  base::TimeDelta min_delta_time =
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
    if (!AddItemToUpdateCheck(item, &query))
      break;
  }
  // Finally, we check components that we already updated.
  for (UpdateItems::const_iterator it = work_items_.begin();
       it != work_items_.end(); ++it) {
    CrxUpdateItem* item = *it;
    if (item->status != CrxUpdateItem::kUpdated)
      continue;
    base::TimeDelta delta = base::Time::Now() - item->last_check;
    if (delta < min_delta_time)
      continue;
    if (!AddItemToUpdateCheck(item, &query))
      break;
  }

  if (query.empty()) {
    // Next check after the long sleep.
    ScheduleNextRun(false);
    return;
  }

  // We got components to check. Start the url request.
  const std::string full_query = MakeFinalQuery(config_->UpdateUrl().spec(),
                                                query,
                                                config_->ExtraRequestParams());
  url_fetcher_.reset(content::URLFetcher::Create(
      0, GURL(full_query), content::URLFetcher::GET,
      MakeContextDelegate(this, new UpdateContext())));
  StartFetch(url_fetcher_.get(), config_->RequestContext(), false);
}

// Caled when we got a response from the update server. It consists of an xml
// document following the omaha update scheme.
void CrxUpdateService::OnURLFetchComplete(const content::URLFetcher* source,
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
    if (!manifest.Parse(xml)) {
       CrxUpdateService::OnParseUpdateManifestFailed(manifest.errors());
    } else {
       CrxUpdateService::OnParseUpdateManifestSucceeded(manifest.results());
    }
  } else {
    UtilityProcessHost* host =
        new UtilityProcessHost(new ManifestParserBridge(this),
                               BrowserThread::UI);
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
  int update_pending = 0;
  std::vector<UpdateManifest::Result>::const_iterator it;
  for (it = results.list.begin(); it != results.list.end(); ++it) {
    CrxUpdateItem* crx = FindUpdateItemById(it->extension_id);
    if (!crx)
      continue;

    if (crx->status != CrxUpdateItem::kChecking)
      continue;  // Not updating this component now.

    config_->OnEvent(Configurator::kManifestCheck, CrxIdtoUMAId(crx->id));

    if (it->version.empty()) {
      // No version means no update available.
      crx->status = CrxUpdateItem::kNoUpdate;
      continue;
    }
    if (!IsVersionNewer(crx->component.version, it->version)) {
      // Our component is up to date.
      crx->status = CrxUpdateItem::kUpToDate;
      continue;
    }
    if (!it->browser_min_version.empty()) {
      if (IsVersionNewer(chrome_version_, it->browser_min_version)) {
        // Does not apply for this chrome version.
        crx->status = CrxUpdateItem::kNoUpdate;
        continue;
      }
    }
    // All test passed. Queue an upgrade for this component and fire the
    // notifications.
    crx->crx_url = it->crx_url;
    crx->status = CrxUpdateItem::kCanUpdate;
    crx->next_version = Version(it->version);
    ++update_pending;

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_COMPONENT_UPDATE_FOUND,
        content::Source<std::string>(&crx->id),
        content::NotificationService::NoDetails());
  }

  // All the components that are not mentioned in the manifest we
  // consider them up to date.
  ChangeItemStatus(CrxUpdateItem::kChecking, CrxUpdateItem::kUpToDate);

  // If there are updates pending we do a short wait.
  ScheduleNextRun(update_pending ? true : false);
}

void CrxUpdateService::OnParseUpdateManifestFailed(
    const std::string& error_message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  size_t count = ChangeItemStatus(CrxUpdateItem::kChecking,
                                  CrxUpdateItem::kNoUpdate);
  config_->OnEvent(Configurator::kManifestError, static_cast<int>(count));
  DCHECK_GT(count, 0ul);
  ScheduleNextRun(false);
}

// Called when the CRX package has been downloaded to a temporary location.
// Here we fire the notifications and schedule the component-specific installer
// to be called in the file thread.
void CrxUpdateService::OnURLFetchComplete(const content::URLFetcher* source,
                                          CRXContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::PlatformFileError error_code;

  if (source->FileErrorOccurred(&error_code) || !FetchSuccess(*source)) {
    size_t count = ChangeItemStatus(CrxUpdateItem::kDownloading,
                                    CrxUpdateItem::kNoUpdate);
    DCHECK_EQ(count, 1ul);
    config_->OnEvent(Configurator::kNetworkError, CrxIdtoUMAId(context->id));
    url_fetcher_.reset();
    ScheduleNextRun(false);
  } else {
    FilePath temp_crx_path;
    CHECK(source->GetResponseAsFilePath(true, &temp_crx_path));
    size_t count = ChangeItemStatus(CrxUpdateItem::kDownloading,
                                    CrxUpdateItem::kUpdating);
    DCHECK_EQ(count, 1ul);
    url_fetcher_.reset();

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_COMPONENT_UPDATE_READY,
        content::Source<std::string>(&context->id),
        content::NotificationService::NoDetails());

    // Why unretained? See comment at top of file.
    BrowserThread::PostDelayedTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&CrxUpdateService::Install,
                   base::Unretained(this),
                   context,
                   temp_crx_path),
        config_->StepDelay());
  }
}

// Install consists of digital signature verification, unpacking and then
// calling the component specific installer. All that is handled by the
// |unpacker|. If there is an error this function is in charge of deleting
// the files created.
void CrxUpdateService::Install(const CRXContext* context,
                               const FilePath& crx_path) {
  // This function owns the |crx_path| and the |context| object.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  ComponentUnpacker
      unpacker(context->pk_hash, crx_path, context->installer);
  if (!file_util::Delete(crx_path, false)) {
    NOTREACHED() << crx_path.value();
  }
  // Why unretained? See comment at top of file.
  BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&CrxUpdateService::DoneInstalling, base::Unretained(this),
                 context->id, unpacker.error()),
      config_->StepDelay());
  delete context;
}

// Installation has been completed. Adjust the component status and
// schedule the next check.
void CrxUpdateService::DoneInstalling(const std::string& component_id,
                                      ComponentUnpacker::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  CrxUpdateItem* item = FindUpdateItemById(component_id);
  item->status = (error == ComponentUnpacker::kNone) ? CrxUpdateItem::kUpdated :
                                                       CrxUpdateItem::kNoUpdate;
  if (item->status == CrxUpdateItem::kUpdated)
    item->component.version = item->next_version;

  Configurator::Events event;
  switch (error) {
    case ComponentUnpacker::kNone:
      event = Configurator::kComponentUpdated;
      break;
    case ComponentUnpacker::kInstallerError:
      event = Configurator::kInstallerError;
      break;
    default:
      event = Configurator::kUnpackError;
      break;
  }

  config_->OnEvent(event, CrxIdtoUMAId(component_id));
  ScheduleNextRun(false);
}

// The component update factory. Using the component updater as a singleton
// is the job of the browser process.
ComponentUpdateService* ComponentUpdateServiceFactory(
    ComponentUpdateService::Configurator* config) {
  DCHECK(config);
  return new CrxUpdateService(config);
}
