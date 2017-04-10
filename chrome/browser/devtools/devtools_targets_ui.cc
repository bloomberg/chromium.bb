// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_targets_ui.h"

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/worker_service.h"
#include "content/public/browser/worker_service_observer.h"
#include "content/public/common/process_type.h"
#include "net/base/escape.h"

using content::BrowserThread;
using content::DevToolsAgentHost;

namespace {

const char kTargetSourceField[]  = "source";
const char kTargetSourceLocal[]  = "local";
const char kTargetSourceRemote[]  = "remote";

const char kTargetIdField[]  = "id";
const char kTargetTypeField[]  = "type";
const char kAttachedField[]  = "attached";
const char kUrlField[]  = "url";
const char kNameField[]  = "name";
const char kFaviconUrlField[] = "faviconUrl";
const char kDescriptionField[] = "description";

const char kGuestList[] = "guests";

const char kAdbModelField[] = "adbModel";
const char kAdbConnectedField[] = "adbConnected";
const char kAdbSerialField[] = "adbSerial";
const char kAdbBrowsersList[] = "browsers";
const char kAdbDeviceIdFormat[] = "device:%s";

const char kAdbBrowserNameField[] = "adbBrowserName";
const char kAdbBrowserUserField[] = "adbBrowserUser";
const char kAdbBrowserVersionField[] = "adbBrowserVersion";
const char kAdbBrowserChromeVersionField[] = "adbBrowserChromeVersion";
const char kAdbPagesList[] = "pages";

const char kAdbScreenWidthField[] = "adbScreenWidth";
const char kAdbScreenHeightField[] = "adbScreenHeight";

const char kPortForwardingPorts[] = "ports";
const char kPortForwardingBrowserId[] = "browserId";

// CancelableTimer ------------------------------------------------------------

class CancelableTimer {
 public:
  CancelableTimer(base::Closure callback, base::TimeDelta delay)
      : callback_(callback),
        weak_factory_(this) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CancelableTimer::Fire, weak_factory_.GetWeakPtr()), delay);
  }

 private:
  void Fire() { callback_.Run(); }

  base::Closure callback_;
  base::WeakPtrFactory<CancelableTimer> weak_factory_;
};

// WorkerObserver -------------------------------------------------------------

class WorkerObserver
    : public content::WorkerServiceObserver,
      public base::RefCountedThreadSafe<WorkerObserver> {
 public:
  WorkerObserver() {}

  void Start(base::Closure callback) {
    DCHECK(callback_.is_null());
    DCHECK(!callback.is_null());
    callback_ = callback;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WorkerObserver::StartOnIOThread, this));
  }

  void Stop() {
    DCHECK(!callback_.is_null());
    callback_ = base::Closure();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WorkerObserver::StopOnIOThread, this));
  }

 private:
  friend class base::RefCountedThreadSafe<WorkerObserver>;
  ~WorkerObserver() override {}

  // content::WorkerServiceObserver overrides:
  void WorkerCreated(const GURL& url,
                     const base::string16& name,
                     int process_id,
                     int route_id) override {
    NotifyOnIOThread();
  }

  void WorkerDestroyed(int process_id, int route_id) override {
    NotifyOnIOThread();
  }

  void StartOnIOThread() {
    content::WorkerService::GetInstance()->AddObserver(this);
  }

  void StopOnIOThread() {
    content::WorkerService::GetInstance()->RemoveObserver(this);
  }

  void NotifyOnIOThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WorkerObserver::NotifyOnUIThread, this));
  }

  void NotifyOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (callback_.is_null())
      return;
    callback_.Run();
  }

  // Accessed on UI thread.
  base::Closure callback_;
};

// LocalTargetsUIHandler ---------------------------------------------

class LocalTargetsUIHandler
    : public DevToolsTargetsUIHandler,
      public content::NotificationObserver {
 public:
  explicit LocalTargetsUIHandler(const Callback& callback);
  ~LocalTargetsUIHandler() override;

  // DevToolsTargetsUIHandler overrides.
  void ForceUpdate() override;

private:
  // content::NotificationObserver overrides.
 void Observe(int type,
              const content::NotificationSource& source,
              const content::NotificationDetails& details) override;

  void ScheduleUpdate();
  void UpdateTargets();
  void SendTargets(const DevToolsAgentHost::List& targets);

  content::NotificationRegistrar notification_registrar_;
  std::unique_ptr<CancelableTimer> timer_;
  scoped_refptr<WorkerObserver> observer_;
  base::WeakPtrFactory<LocalTargetsUIHandler> weak_factory_;
};

LocalTargetsUIHandler::LocalTargetsUIHandler(
    const Callback& callback)
    : DevToolsTargetsUIHandler(kTargetSourceLocal, callback),
      observer_(new WorkerObserver()),
      weak_factory_(this) {
  notification_registrar_.Add(this,
                              content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                              content::NotificationService::AllSources());
  observer_->Start(base::Bind(&LocalTargetsUIHandler::ScheduleUpdate,
                              base::Unretained(this)));
  UpdateTargets();
}

LocalTargetsUIHandler::~LocalTargetsUIHandler() {
  notification_registrar_.RemoveAll();
  observer_->Stop();
}

void LocalTargetsUIHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  ScheduleUpdate();
}

void LocalTargetsUIHandler::ForceUpdate() {
  ScheduleUpdate();
}

void LocalTargetsUIHandler::ScheduleUpdate() {
  const int kUpdateDelay = 100;
  timer_.reset(
      new CancelableTimer(
          base::Bind(&LocalTargetsUIHandler::UpdateTargets,
                     base::Unretained(this)),
          base::TimeDelta::FromMilliseconds(kUpdateDelay)));
}

void LocalTargetsUIHandler::UpdateTargets() {
  SendTargets(DevToolsAgentHost::GetOrCreateAll());
}

void LocalTargetsUIHandler::SendTargets(
    const content::DevToolsAgentHost::List& targets) {
  base::ListValue list_value;
  std::map<std::string, base::DictionaryValue*> id_to_descriptor;

  targets_.clear();
  for (scoped_refptr<DevToolsAgentHost> host : targets) {
    targets_[host->GetId()] = host;
    id_to_descriptor[host->GetId()] = Serialize(host).release();
  }

  for (auto& it : targets_) {
    scoped_refptr<DevToolsAgentHost> host = it.second;
    base::DictionaryValue* descriptor = id_to_descriptor[host->GetId()];
    DCHECK(descriptor);
    std::string parent_id = host->GetParentId();
    if (parent_id.empty() || id_to_descriptor.count(parent_id) == 0) {
      list_value.Append(base::WrapUnique(descriptor));
    } else {
      base::DictionaryValue* parent = id_to_descriptor[parent_id];
      base::ListValue* guests = NULL;
      if (!parent->GetList(kGuestList, &guests)) {
        guests = new base::ListValue();
        parent->Set(kGuestList, guests);
      }
      guests->Append(base::WrapUnique(descriptor));
    }
  }

  SendSerializedTargets(list_value);
}

// AdbTargetsUIHandler --------------------------------------------------------

class AdbTargetsUIHandler
    : public DevToolsTargetsUIHandler,
      public DevToolsAndroidBridge::DeviceListListener {
 public:
  AdbTargetsUIHandler(const Callback& callback, Profile* profile);
  ~AdbTargetsUIHandler() override;

  void Open(const std::string& browser_id, const std::string& url) override;

  scoped_refptr<DevToolsAgentHost> GetBrowserAgentHost(
      const std::string& browser_id) override;

 private:
  // DevToolsAndroidBridge::Listener overrides.
  void DeviceListChanged(
      const DevToolsAndroidBridge::RemoteDevices& devices) override;

  DevToolsAndroidBridge* GetAndroidBridge();

  Profile* const profile_;
  DevToolsAndroidBridge* const android_bridge_;

  typedef std::map<std::string,
      scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> > RemoteBrowsers;
  RemoteBrowsers remote_browsers_;
};

AdbTargetsUIHandler::AdbTargetsUIHandler(const Callback& callback,
                                         Profile* profile)
    : DevToolsTargetsUIHandler(kTargetSourceRemote, callback),
      profile_(profile),
      android_bridge_(
          DevToolsAndroidBridge::Factory::GetForProfile(profile_)) {
  if (android_bridge_)
    android_bridge_->AddDeviceListListener(this);
}

AdbTargetsUIHandler::~AdbTargetsUIHandler() {
  if (android_bridge_)
    android_bridge_->RemoveDeviceListListener(this);
}

void AdbTargetsUIHandler::Open(const std::string& browser_id,
                               const std::string& url) {
  RemoteBrowsers::iterator it = remote_browsers_.find(browser_id);
  if (it != remote_browsers_.end() && android_bridge_)
    android_bridge_->OpenRemotePage(it->second, url);
}

scoped_refptr<DevToolsAgentHost>
AdbTargetsUIHandler::GetBrowserAgentHost(
    const std::string& browser_id) {
  RemoteBrowsers::iterator it = remote_browsers_.find(browser_id);
  if (it == remote_browsers_.end() || !android_bridge_)
    return nullptr;

  return android_bridge_->GetBrowserAgentHost(it->second);
}

void AdbTargetsUIHandler::DeviceListChanged(
    const DevToolsAndroidBridge::RemoteDevices& devices) {
  remote_browsers_.clear();
  targets_.clear();
  if (!android_bridge_)
    return;

  base::ListValue device_list;
  for (DevToolsAndroidBridge::RemoteDevices::const_iterator dit =
      devices.begin(); dit != devices.end(); ++dit) {
    DevToolsAndroidBridge::RemoteDevice* device = dit->get();
    std::unique_ptr<base::DictionaryValue> device_data(
        new base::DictionaryValue());
    device_data->SetString(kAdbModelField, device->model());
    device_data->SetString(kAdbSerialField, device->serial());
    device_data->SetBoolean(kAdbConnectedField, device->is_connected());
    std::string device_id = base::StringPrintf(
        kAdbDeviceIdFormat,
        device->serial().c_str());
    device_data->SetString(kTargetIdField, device_id);
    base::ListValue* browser_list = new base::ListValue();
    device_data->Set(kAdbBrowsersList, browser_list);

    DevToolsAndroidBridge::RemoteBrowsers& browsers = device->browsers();
    for (DevToolsAndroidBridge::RemoteBrowsers::iterator bit =
        browsers.begin(); bit != browsers.end(); ++bit) {
      DevToolsAndroidBridge::RemoteBrowser* browser = bit->get();
      std::unique_ptr<base::DictionaryValue> browser_data(
          new base::DictionaryValue());
      browser_data->SetString(kAdbBrowserNameField, browser->display_name());
      browser_data->SetString(kAdbBrowserUserField, browser->user());
      browser_data->SetString(kAdbBrowserVersionField, browser->version());
      DevToolsAndroidBridge::RemoteBrowser::ParsedVersion parsed =
          browser->GetParsedVersion();
      browser_data->SetInteger(
          kAdbBrowserChromeVersionField,
          browser->IsChrome() && !parsed.empty() ? parsed[0] : 0);
      std::string browser_id = browser->GetId();
      browser_data->SetString(kTargetIdField, browser_id);
      browser_data->SetString(kTargetSourceField, source_id());

      base::ListValue* page_list = new base::ListValue();
      remote_browsers_[browser_id] = browser;
            browser_data->Set(kAdbPagesList, page_list);
      for (const auto& page : browser->pages()) {
        scoped_refptr<DevToolsAgentHost> host = page->CreateTarget();
        std::unique_ptr<base::DictionaryValue> target_data = Serialize(host);
        // Pass the screen size in the target object to make sure that
        // the caching logic does not prevent the target item from updating
        // when the screen size changes.
        gfx::Size screen_size = device->screen_size();
        target_data->SetInteger(kAdbScreenWidthField, screen_size.width());
        target_data->SetInteger(kAdbScreenHeightField, screen_size.height());
        targets_[host->GetId()] = host;
        page_list->Append(std::move(target_data));
      }
      browser_list->Append(std::move(browser_data));
    }

    device_list.Append(std::move(device_data));
  }
  SendSerializedTargets(device_list);
}

} // namespace

// DevToolsTargetsUIHandler ---------------------------------------------------

DevToolsTargetsUIHandler::DevToolsTargetsUIHandler(
    const std::string& source_id,
    const Callback& callback)
    : source_id_(source_id),
      callback_(callback) {
}

DevToolsTargetsUIHandler::~DevToolsTargetsUIHandler() {
}

// static
std::unique_ptr<DevToolsTargetsUIHandler>
DevToolsTargetsUIHandler::CreateForLocal(
    const DevToolsTargetsUIHandler::Callback& callback) {
  return std::unique_ptr<DevToolsTargetsUIHandler>(
      new LocalTargetsUIHandler(callback));
}

// static
std::unique_ptr<DevToolsTargetsUIHandler>
DevToolsTargetsUIHandler::CreateForAdb(
    const DevToolsTargetsUIHandler::Callback& callback,
    Profile* profile) {
  return std::unique_ptr<DevToolsTargetsUIHandler>(
      new AdbTargetsUIHandler(callback, profile));
}

scoped_refptr<DevToolsAgentHost> DevToolsTargetsUIHandler::GetTarget(
    const std::string& target_id) {
  TargetMap::iterator it = targets_.find(target_id);
  if (it != targets_.end())
    return it->second;
  return NULL;
}

void DevToolsTargetsUIHandler::Open(const std::string& browser_id,
                                    const std::string& url) {
}

scoped_refptr<DevToolsAgentHost>
DevToolsTargetsUIHandler::GetBrowserAgentHost(const std::string& browser_id) {
  return NULL;
}

std::unique_ptr<base::DictionaryValue> DevToolsTargetsUIHandler::Serialize(
    scoped_refptr<DevToolsAgentHost> host) {
  auto target_data = base::MakeUnique<base::DictionaryValue>();
  target_data->SetString(kTargetSourceField, source_id_);
  target_data->SetString(kTargetIdField, host->GetId());
  target_data->SetString(kTargetTypeField, host->GetType());
  target_data->SetBoolean(kAttachedField, host->IsAttached());
  target_data->SetString(kUrlField, host->GetURL().spec());
  target_data->SetString(kNameField, host->GetTitle());
  target_data->SetString(kFaviconUrlField, host->GetFaviconURL().spec());
  target_data->SetString(kDescriptionField, host->GetDescription());
  return target_data;
}

void DevToolsTargetsUIHandler::SendSerializedTargets(
    const base::ListValue& list) {
  callback_.Run(source_id_, list);
}

void DevToolsTargetsUIHandler::ForceUpdate() {
}

// PortForwardingStatusSerializer ---------------------------------------------

PortForwardingStatusSerializer::PortForwardingStatusSerializer(
    const Callback& callback, Profile* profile)
      : callback_(callback),
        profile_(profile) {
  DevToolsAndroidBridge* android_bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile_);
  if (android_bridge)
    android_bridge->AddPortForwardingListener(this);
}

PortForwardingStatusSerializer::~PortForwardingStatusSerializer() {
  DevToolsAndroidBridge* android_bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile_);
  if (android_bridge)
    android_bridge->RemovePortForwardingListener(this);
}

void PortForwardingStatusSerializer::PortStatusChanged(
    const ForwardingStatus& status) {
  base::DictionaryValue result;
  for (ForwardingStatus::const_iterator sit = status.begin();
      sit != status.end(); ++sit) {
    base::DictionaryValue* port_status_dict = new base::DictionaryValue();
    const PortStatusMap& port_status_map = sit->second;
    for (PortStatusMap::const_iterator it = port_status_map.begin();
         it != port_status_map.end(); ++it) {
      port_status_dict->SetInteger(base::IntToString(it->first), it->second);
    }

    base::DictionaryValue* device_status_dict = new base::DictionaryValue();
    device_status_dict->Set(kPortForwardingPorts, port_status_dict);
    device_status_dict->SetString(kPortForwardingBrowserId,
                                  sit->first->GetId());

    std::string device_id = base::StringPrintf(
        kAdbDeviceIdFormat,
        sit->first->serial().c_str());
    result.Set(device_id, device_status_dict);
  }
  callback_.Run(result);
}
