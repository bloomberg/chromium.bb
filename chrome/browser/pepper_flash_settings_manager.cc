// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pepper_flash_settings_manager.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/pepper_flash_settings_helper.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/content_constants.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "webkit/plugins/plugin_constants.h"
#include "webkit/plugins/webplugininfo.h"

using content::BrowserThread;

class PepperFlashSettingsManager::Core
    : public IPC::Channel::Listener,
      public base::RefCountedThreadSafe<Core, BrowserThread::DeleteOnIOThread> {
 public:
  Core(PepperFlashSettingsManager* manager,
       content::BrowserContext* browser_context);

  // Stops sending notifications to |manager_| and sets it to NULL.
  void Detach();

  void DeauthorizeContentLicenses(uint32 request_id);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<Core>;

  enum RequestType {
    INVALID_REQUEST_TYPE = 0,
    DEAUTHORIZE_CONTENT_LICENSES
  };

  struct PendingRequest {
    PendingRequest() : id(0), type(INVALID_REQUEST_TYPE) {}

    uint32 id;
    RequestType type;
  };

  virtual ~Core();

  void Initialize();
  void ConnectToChannel(bool success, const IPC::ChannelHandle& handle);

  void DeauthorizeContentLicensesOnIOThread(uint32 request_id);
  void NotifyErrorFromIOThread();

  void NotifyDeauthorizeContentLicensesCompleted(uint32 request_id,
                                                 bool success);
  void NotifyError(
      const std::vector<std::pair<uint32, RequestType> >& notifications);

  // Message handlers.
  void OnDeauthorizeContentLicensesResult(uint32 request_id, bool success);

  // Used only on the UI thread.
  PepperFlashSettingsManager* manager_;

  // Used only on the I/O thread.
  FilePath plugin_data_path_;

  // The channel is NULL until we have opened a connection to the broker
  // process. Used only on the I/O thread.
  scoped_ptr<IPC::Channel> channel_;

  // Used only on the I/O thread.
  bool initialized_;

  // Requests that need to be sent once the channel to the broker process is
  // established. Used only on the I/O thread.
  std::vector<PendingRequest> pending_requests_;
  // Requests that have been sent but haven't got replied. Used only on the
  // I/O thread.
  std::map<uint32, RequestType> pending_responses_;

  // Used only on the I/O thread.
  scoped_refptr<content::PepperFlashSettingsHelper> helper_;

  // Path for the current profile. Must be retrieved on the UI thread from the
  // browser context when we start so we can use it later on the I/O thread.
  FilePath browser_context_path_;

  scoped_refptr<PluginPrefs> plugin_prefs_;
};

PepperFlashSettingsManager::Core::Core(PepperFlashSettingsManager* manager,
                                       content::BrowserContext* browser_context)
    : manager_(manager),
      initialized_(false),
      browser_context_path_(browser_context->GetPath()),
      plugin_prefs_(PluginPrefs::GetForProfile(
          Profile::FromBrowserContext(browser_context))) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Core::Initialize, this));
}

PepperFlashSettingsManager::Core::~Core() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void PepperFlashSettingsManager::Core::Detach() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  manager_ = NULL;
}

void PepperFlashSettingsManager::Core::DeauthorizeContentLicenses(
    uint32 request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&Core::DeauthorizeContentLicensesOnIOThread, this,
                 request_id));
}

bool PepperFlashSettingsManager::Core::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(Core, message)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_DeauthorizeContentLicensesResult,
                        OnDeauthorizeContentLicensesResult)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

  return true;
}

void PepperFlashSettingsManager::Core::OnChannelError() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  NotifyErrorFromIOThread();
}

void PepperFlashSettingsManager::Core::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!initialized_);

  webkit::WebPluginInfo plugin_info;
  if (!PepperFlashSettingsManager::IsPepperFlashInUse(plugin_prefs_.get(),
                                                      &plugin_info)) {
    NotifyErrorFromIOThread();
    return;
  }

  FilePath profile_path =
      browser_context_path_.Append(content::kPepperDataDirname);
#if defined(OS_WIN)
  plugin_data_path_ = profile_path.Append(plugin_info.name);
#else
  plugin_data_path_ = profile_path.Append(UTF16ToUTF8(plugin_info.name));
#endif

  helper_ = content::PepperFlashSettingsHelper::Create();
  content::PepperFlashSettingsHelper::OpenChannelCallback callback =
      base::Bind(&Core::ConnectToChannel, this);
  helper_->OpenChannelToBroker(plugin_info.path, callback);
}

void PepperFlashSettingsManager::Core::ConnectToChannel(
    bool success,
    const IPC::ChannelHandle& handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!initialized_);
  DCHECK(!channel_.get());

  if (!success) {
    LOG(ERROR) << "Couldn't open plugin channel";
    NotifyErrorFromIOThread();
    return;
  }

  channel_.reset(new IPC::Channel(handle, IPC::Channel::MODE_CLIENT, this));
  if (!channel_->Connect()) {
    LOG(ERROR) << "Couldn't connect to plugin";
    NotifyErrorFromIOThread();
    return;
  }

  initialized_ = true;

  std::vector<PendingRequest> temp_pending_requests;
  temp_pending_requests.swap(pending_requests_);
  for (std::vector<PendingRequest>::iterator iter =
           temp_pending_requests.begin();
       iter != temp_pending_requests.end(); ++iter) {
    switch (iter->type) {
      case DEAUTHORIZE_CONTENT_LICENSES:
        DeauthorizeContentLicensesOnIOThread(iter->id);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

void PepperFlashSettingsManager::Core::DeauthorizeContentLicensesOnIOThread(
    uint32 request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!initialized_) {
    PendingRequest request;
    request.id = request_id;
    request.type = DEAUTHORIZE_CONTENT_LICENSES;
    pending_requests_.push_back(request);
    return;
  }

  pending_responses_.insert(
      std::make_pair(request_id, DEAUTHORIZE_CONTENT_LICENSES));
  IPC::Message* msg =
      new PpapiMsg_DeauthorizeContentLicenses(request_id, plugin_data_path_);
  if (!channel_->Send(msg)) {
    LOG(ERROR) << "Couldn't send DeauthorizeContentLicenses message";
    // A failure notification for the current request will be sent since
    // |pending_responses_| has been updated.
    NotifyErrorFromIOThread();
  }
}

void PepperFlashSettingsManager::Core::NotifyErrorFromIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  std::vector<std::pair<uint32, RequestType> > notifications;
  for (std::vector<PendingRequest>::iterator iter = pending_requests_.begin();
       iter != pending_requests_.end(); ++iter) {
    notifications.push_back(std::make_pair(iter->id, iter->type));
  }
  pending_requests_.clear();
  notifications.insert(notifications.end(), pending_responses_.begin(),
                       pending_responses_.end());
  pending_responses_.clear();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Core::NotifyError, this, notifications));
}

void
PepperFlashSettingsManager::Core::NotifyDeauthorizeContentLicensesCompleted(
    uint32 request_id,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (manager_) {
    manager_->client_->OnDeauthorizeContentLicensesCompleted(
        request_id, success);
  }
}

void PepperFlashSettingsManager::Core::NotifyError(
    const std::vector<std::pair<uint32, RequestType> >& notifications) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_refptr<Core> protector(this);
  for (std::vector<std::pair<uint32, RequestType> >::const_iterator iter =
      notifications.begin(); iter != notifications.end(); ++iter) {
    // Check |manager_| for each iteration in case Detach() happens in one of
    // the callbacks.
    if (manager_) {
      switch (iter->second) {
        case DEAUTHORIZE_CONTENT_LICENSES:
          manager_->client_->OnDeauthorizeContentLicensesCompleted(iter->first,
                                                                   false);
          break;
        default:
          NOTREACHED();
          break;
      }
    }
  }

  if (manager_)
    manager_->OnError();
}

void PepperFlashSettingsManager::Core::OnDeauthorizeContentLicensesResult(
    uint32 request_id,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  LOG_IF(ERROR, !success) << "DeauthorizeContentLicenses returned error";

  std::map<uint32, RequestType>::iterator iter =
      pending_responses_.find(request_id);
  if (iter != pending_responses_.end()) {
    DCHECK_EQ(iter->second, DEAUTHORIZE_CONTENT_LICENSES);

    pending_responses_.erase(iter);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&Core::NotifyDeauthorizeContentLicensesCompleted, this,
                   request_id, success));
  }
}

PepperFlashSettingsManager::PepperFlashSettingsManager(
    Client* client,
    content::BrowserContext* browser_context)
    : client_(client),
      browser_context_(browser_context),
      next_request_id_(1) {
  DCHECK(client);
  DCHECK(browser_context);
}

PepperFlashSettingsManager::~PepperFlashSettingsManager() {
  if (core_.get()) {
    core_->Detach();
    core_ = NULL;
  }
}

// static
bool PepperFlashSettingsManager::IsPepperFlashInUse(
    PluginPrefs* plugin_prefs,
    webkit::WebPluginInfo* plugin_info) {
  if (!plugin_prefs)
    return false;

  content::PluginService* plugin_service =
      content::PluginService::GetInstance();
  std::vector<webkit::WebPluginInfo> plugins;
  plugin_service->GetPluginInfoArray(
      GURL(), kFlashPluginSwfMimeType, false, &plugins, NULL);

  for (std::vector<webkit::WebPluginInfo>::iterator iter = plugins.begin();
       iter != plugins.end(); ++iter) {
    if (webkit::IsPepperPlugin(*iter) && plugin_prefs->IsPluginEnabled(*iter)) {
      if (plugin_info)
        *plugin_info = *iter;
      return true;
    }
  }
  return false;
}

// static
void PepperFlashSettingsManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kDeauthorizeContentLicenses,
                             false,
                             PrefService::UNSYNCABLE_PREF);

  prefs->RegisterBooleanPref(prefs::kPepperFlashSettingsEnabled,
                             true,
                             PrefService::UNSYNCABLE_PREF);
}

uint32 PepperFlashSettingsManager::DeauthorizeContentLicenses() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  EnsureCoreExists();
  uint32 id = GetNextRequestId();
  core_->DeauthorizeContentLicenses(id);
  return id;
}

uint32 PepperFlashSettingsManager::GetNextRequestId() {
  return next_request_id_++;
}

void PepperFlashSettingsManager::EnsureCoreExists() {
  if (!core_.get())
    core_ = new Core(this, browser_context_);
}

void PepperFlashSettingsManager::OnError() {
  if (core_.get()) {
    core_->Detach();
    core_ = NULL;
  } else {
    NOTREACHED();
  }
}

