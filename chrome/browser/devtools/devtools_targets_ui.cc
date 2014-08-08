// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_targets_ui.h"

#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
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

namespace {

const char kTargetSourceField[]  = "source";
const char kTargetSourceRenderer[]  = "renderers";
const char kTargetSourceWorker[]  = "workers";
const char kTargetSourceAdb[]  = "adb";

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
const char kAdbBrowserVersionField[] = "adbBrowserVersion";
const char kAdbBrowserChromeVersionField[] = "adbBrowserChromeVersion";
const char kCompatibleVersion[] = "compatibleVersion";
const char kAdbPagesList[] = "pages";

const char kAdbScreenWidthField[] = "adbScreenWidth";
const char kAdbScreenHeightField[] = "adbScreenHeight";
const char kAdbAttachedForeignField[]  = "adbAttachedForeign";

// CancelableTimer ------------------------------------------------------------

class CancelableTimer {
 public:
  CancelableTimer(base::Closure callback, base::TimeDelta delay)
      : callback_(callback),
        weak_factory_(this) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CancelableTimer::Fire, weak_factory_.GetWeakPtr()),
        delay);
  }

 private:
  void Fire() { callback_.Run(); }

  base::Closure callback_;
  base::WeakPtrFactory<CancelableTimer> weak_factory_;
};

// RenderViewHostTargetsUIHandler ---------------------------------------------

class RenderViewHostTargetsUIHandler
    : public DevToolsTargetsUIHandler,
      public content::NotificationObserver {
 public:
  explicit RenderViewHostTargetsUIHandler(const Callback& callback);
  virtual ~RenderViewHostTargetsUIHandler();

 private:
  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void UpdateTargets();

  content::NotificationRegistrar notification_registrar_;
  scoped_ptr<CancelableTimer> timer_;
};

RenderViewHostTargetsUIHandler::RenderViewHostTargetsUIHandler(
    const Callback& callback)
    : DevToolsTargetsUIHandler(kTargetSourceRenderer, callback) {
  notification_registrar_.Add(this,
                              content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                              content::NotificationService::AllSources());
  UpdateTargets();
}

RenderViewHostTargetsUIHandler::~RenderViewHostTargetsUIHandler() {
  notification_registrar_.RemoveAll();
}

void RenderViewHostTargetsUIHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  const int kUpdateDelay = 100;
  timer_.reset(
      new CancelableTimer(
          base::Bind(&RenderViewHostTargetsUIHandler::UpdateTargets,
                     base::Unretained(this)),
          base::TimeDelta::FromMilliseconds(kUpdateDelay)));
}

void RenderViewHostTargetsUIHandler::UpdateTargets() {
  base::ListValue list_value;

  std::map<std::string, base::DictionaryValue*> id_to_descriptor;

  DevToolsTargetImpl::List targets =
      DevToolsTargetImpl::EnumerateWebContentsTargets();

  STLDeleteValues(&targets_);
  for (DevToolsTargetImpl::List::iterator it = targets.begin();
      it != targets.end(); ++it) {
    DevToolsTargetImpl* target = *it;
    targets_[target->GetId()] = target;
    id_to_descriptor[target->GetId()] = Serialize(*target);
  }

  for (TargetMap::iterator it(targets_.begin()); it != targets_.end(); ++it) {
    DevToolsTargetImpl* target = it->second;
    base::DictionaryValue* descriptor = id_to_descriptor[target->GetId()];

    std::string parent_id = target->GetParentId();
    if (parent_id.empty() || id_to_descriptor.count(parent_id) == 0) {
      list_value.Append(descriptor);
    } else {
      base::DictionaryValue* parent = id_to_descriptor[parent_id];
      base::ListValue* guests = NULL;
      if (!parent->GetList(kGuestList, &guests)) {
        guests = new base::ListValue();
        parent->Set(kGuestList, guests);
      }
      guests->Append(descriptor);
    }
  }

  SendSerializedTargets(list_value);
}

// WorkerObserver -------------------------------------------------------------

class WorkerObserver
    : public content::WorkerServiceObserver,
      public base::RefCountedThreadSafe<WorkerObserver> {
 public:
  WorkerObserver() {}

  void Start(DevToolsTargetImpl::Callback callback) {
    DCHECK(callback_.is_null());
    DCHECK(!callback.is_null());
    callback_ = callback;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WorkerObserver::StartOnIOThread, this));
  }

  void Stop() {
    DCHECK(!callback_.is_null());
    callback_ = DevToolsTargetImpl::Callback();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WorkerObserver::StopOnIOThread, this));
  }

  void Enumerate() {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WorkerObserver::EnumerateOnIOThread,
                   this));
  }

 private:
  friend class base::RefCountedThreadSafe<WorkerObserver>;
  virtual ~WorkerObserver() {}

  // content::WorkerServiceObserver overrides:
  virtual void WorkerCreated(
      const GURL& url,
      const base::string16& name,
      int process_id,
      int route_id) OVERRIDE {
    EnumerateOnIOThread();
  }

  virtual void WorkerDestroyed(int process_id, int route_id) OVERRIDE {
    EnumerateOnIOThread();
  }

  void StartOnIOThread() {
    content::WorkerService::GetInstance()->AddObserver(this);
    EnumerateOnIOThread();
  }

  void StopOnIOThread() {
    content::WorkerService::GetInstance()->RemoveObserver(this);
  }

  void EnumerateOnIOThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    DevToolsTargetImpl::EnumerateWorkerTargets(
        base::Bind(&WorkerObserver::RespondOnUIThread, this));
  }

  void RespondOnUIThread(const DevToolsTargetImpl::List& targets) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (callback_.is_null())
      return;
    callback_.Run(targets);
  }

  DevToolsTargetImpl::Callback callback_;
};

// WorkerTargetsUIHandler -----------------------------------------------------

class WorkerTargetsUIHandler
    : public DevToolsTargetsUIHandler {
 public:
  explicit WorkerTargetsUIHandler(const Callback& callback);
  virtual ~WorkerTargetsUIHandler();

 private:
  void UpdateTargets(const DevToolsTargetImpl::List& targets);

  scoped_refptr<WorkerObserver> observer_;
};

WorkerTargetsUIHandler::WorkerTargetsUIHandler(const Callback& callback)
    : DevToolsTargetsUIHandler(kTargetSourceWorker, callback),
      observer_(new WorkerObserver()) {
  observer_->Start(base::Bind(&WorkerTargetsUIHandler::UpdateTargets,
                              base::Unretained(this)));
}

WorkerTargetsUIHandler::~WorkerTargetsUIHandler() {
  observer_->Stop();
}

void WorkerTargetsUIHandler::UpdateTargets(
    const DevToolsTargetImpl::List& targets) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::ListValue list_value;
  STLDeleteValues(&targets_);
  for (DevToolsTargetImpl::List::const_iterator it = targets.begin();
      it != targets.end(); ++it) {
    DevToolsTargetImpl* target = *it;
    list_value.Append(Serialize(*target));
    targets_[target->GetId()] = target;
  }
  SendSerializedTargets(list_value);
}

// AdbTargetsUIHandler --------------------------------------------------------

class AdbTargetsUIHandler
    : public DevToolsTargetsUIHandler,
      public DevToolsAndroidBridge::DeviceListListener {
 public:
  AdbTargetsUIHandler(const Callback& callback, Profile* profile);
  virtual ~AdbTargetsUIHandler();

  virtual void Open(const std::string& browser_id,
                    const std::string& url,
                    const DevToolsTargetsUIHandler::TargetCallback&) OVERRIDE;

  virtual scoped_refptr<content::DevToolsAgentHost> GetBrowserAgentHost(
      const std::string& browser_id) OVERRIDE;

 private:
  // DevToolsAndroidBridge::Listener overrides.
  virtual void DeviceListChanged(
      const DevToolsAndroidBridge::RemoteDevices& devices) OVERRIDE;

  Profile* profile_;

  typedef std::map<std::string,
      scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> > RemoteBrowsers;
  RemoteBrowsers remote_browsers_;
};

AdbTargetsUIHandler::AdbTargetsUIHandler(const Callback& callback,
                                         Profile* profile)
    : DevToolsTargetsUIHandler(kTargetSourceAdb, callback),
      profile_(profile) {
  DevToolsAndroidBridge* android_bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile_);
  if (android_bridge)
    android_bridge->AddDeviceListListener(this);
}

AdbTargetsUIHandler::~AdbTargetsUIHandler() {
  DevToolsAndroidBridge* android_bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile_);
  if (android_bridge)
    android_bridge->RemoveDeviceListListener(this);
}

static void CallOnTarget(
    const DevToolsTargetsUIHandler::TargetCallback& callback,
    DevToolsAndroidBridge::RemotePage* page) {
  scoped_ptr<DevToolsAndroidBridge::RemotePage> my_page(page);
  callback.Run(my_page ? my_page->GetTarget() : NULL);
}

void AdbTargetsUIHandler::Open(
    const std::string& browser_id,
    const std::string& url,
    const DevToolsTargetsUIHandler::TargetCallback& callback) {
  RemoteBrowsers::iterator it = remote_browsers_.find(browser_id);
  if (it !=  remote_browsers_.end())
    it->second->Open(url, base::Bind(&CallOnTarget, callback));
}

scoped_refptr<content::DevToolsAgentHost>
AdbTargetsUIHandler::GetBrowserAgentHost(
    const std::string& browser_id) {
  RemoteBrowsers::iterator it = remote_browsers_.find(browser_id);
  return it != remote_browsers_.end() ? it->second->GetAgentHost() : NULL;
}

void AdbTargetsUIHandler::DeviceListChanged(
    const DevToolsAndroidBridge::RemoteDevices& devices) {
  remote_browsers_.clear();
  STLDeleteValues(&targets_);

  base::ListValue device_list;
  for (DevToolsAndroidBridge::RemoteDevices::const_iterator dit =
      devices.begin(); dit != devices.end(); ++dit) {
    DevToolsAndroidBridge::RemoteDevice* device = dit->get();
    base::DictionaryValue* device_data = new base::DictionaryValue();
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
      base::DictionaryValue* browser_data = new base::DictionaryValue();
      browser_data->SetString(kAdbBrowserNameField, browser->display_name());
      browser_data->SetString(kAdbBrowserVersionField, browser->version());
      DevToolsAndroidBridge::RemoteBrowser::ParsedVersion parsed =
          browser->GetParsedVersion();
      browser_data->SetInteger(
          kAdbBrowserChromeVersionField,
          browser->IsChrome() && !parsed.empty() ? parsed[0] : 0);
      std::string browser_id = base::StringPrintf(
          "browser:%s:%s:%s:%s",
          device->serial().c_str(), // Ensure uniqueness across devices.
          browser->display_name().c_str(),  // Sort by display name.
          browser->version().c_str(),  // Then by version.
          browser->socket().c_str());  // Ensure uniqueness on the device.
      browser_data->SetString(kTargetIdField, browser_id);
      browser_data->SetString(kTargetSourceField, source_id());

      base::Version remote_version;
      remote_version = base::Version(browser->version());

      chrome::VersionInfo version_info;
      base::Version local_version(version_info.Version());

      browser_data->SetBoolean(kCompatibleVersion,
          (!remote_version.IsValid()) || (!local_version.IsValid()) ||
          remote_version.components()[0] <= local_version.components()[0]);

      base::ListValue* page_list = new base::ListValue();
      remote_browsers_[browser_id] = browser;
            browser_data->Set(kAdbPagesList, page_list);
      std::vector<DevToolsAndroidBridge::RemotePage*> pages =
          browser->CreatePages();
      for (std::vector<DevToolsAndroidBridge::RemotePage*>::iterator it =
          pages.begin(); it != pages.end(); ++it) {
        DevToolsAndroidBridge::RemotePage* page =  *it;
        DevToolsTargetImpl* target = page->GetTarget();
        base::DictionaryValue* target_data = Serialize(*target);
        target_data->SetBoolean(
            kAdbAttachedForeignField,
            target->IsAttached() &&
                !DevToolsAndroidBridge::HasDevToolsWindow(target->GetId()));
        // Pass the screen size in the target object to make sure that
        // the caching logic does not prevent the target item from updating
        // when the screen size changes.
        gfx::Size screen_size = device->screen_size();
        target_data->SetInteger(kAdbScreenWidthField, screen_size.width());
        target_data->SetInteger(kAdbScreenHeightField, screen_size.height());
        targets_[target->GetId()] = target;
        page_list->Append(target_data);
      }
      browser_list->Append(browser_data);
    }

    device_list.Append(device_data);
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
  STLDeleteValues(&targets_);
}

// static
scoped_ptr<DevToolsTargetsUIHandler>
DevToolsTargetsUIHandler::CreateForRenderers(
    const DevToolsTargetsUIHandler::Callback& callback) {
  return scoped_ptr<DevToolsTargetsUIHandler>(
      new RenderViewHostTargetsUIHandler(callback));
}

// static
scoped_ptr<DevToolsTargetsUIHandler>
DevToolsTargetsUIHandler::CreateForWorkers(
    const DevToolsTargetsUIHandler::Callback& callback) {
  return scoped_ptr<DevToolsTargetsUIHandler>(
      new WorkerTargetsUIHandler(callback));
}

// static
scoped_ptr<DevToolsTargetsUIHandler>
DevToolsTargetsUIHandler::CreateForAdb(
    const DevToolsTargetsUIHandler::Callback& callback, Profile* profile) {
  return scoped_ptr<DevToolsTargetsUIHandler>(
      new AdbTargetsUIHandler(callback, profile));
}

DevToolsTargetImpl* DevToolsTargetsUIHandler::GetTarget(
    const std::string& target_id) {
  TargetMap::iterator it = targets_.find(target_id);
  if (it != targets_.end())
    return it->second;
  return NULL;
}

void DevToolsTargetsUIHandler::Open(const std::string& browser_id,
                                    const std::string& url,
                                    const TargetCallback& callback) {
  callback.Run(NULL);
}

scoped_refptr<content::DevToolsAgentHost>
DevToolsTargetsUIHandler::GetBrowserAgentHost(const std::string& browser_id) {
  return NULL;
}

base::DictionaryValue* DevToolsTargetsUIHandler::Serialize(
    const DevToolsTargetImpl& target) {
  base::DictionaryValue* target_data = new base::DictionaryValue();
  target_data->SetString(kTargetSourceField, source_id_);
  target_data->SetString(kTargetIdField, target.GetId());
  target_data->SetString(kTargetTypeField, target.GetType());
  target_data->SetBoolean(kAttachedField, target.IsAttached());
  target_data->SetString(kUrlField, target.GetURL().spec());
  target_data->SetString(kNameField, net::EscapeForHTML(target.GetTitle()));
  target_data->SetString(kFaviconUrlField, target.GetFaviconURL().spec());
  target_data->SetString(kDescriptionField, target.GetDescription());
  return target_data;
}

void DevToolsTargetsUIHandler::SendSerializedTargets(
    const base::ListValue& list) {
  callback_.Run(source_id_, list);
}

// PortForwardingStatusSerializer ---------------------------------------------

PortForwardingStatusSerializer::PortForwardingStatusSerializer(
    const Callback& callback, Profile* profile)
      : callback_(callback),
        profile_(profile) {
  PortForwardingController* port_forwarding_controller =
      PortForwardingController::Factory::GetForProfile(profile_);
  if (port_forwarding_controller)
    port_forwarding_controller->AddListener(this);
}

PortForwardingStatusSerializer::~PortForwardingStatusSerializer() {
  PortForwardingController* port_forwarding_controller =
      PortForwardingController::Factory::GetForProfile(profile_);
  if (port_forwarding_controller)
    port_forwarding_controller->RemoveListener(this);
}

void PortForwardingStatusSerializer::PortStatusChanged(
    const DevicesStatus& status) {
  base::DictionaryValue result;
  for (DevicesStatus::const_iterator sit = status.begin();
      sit != status.end(); ++sit) {
    base::DictionaryValue* device_status_dict = new base::DictionaryValue();
    const PortStatusMap& device_status_map = sit->second;
    for (PortStatusMap::const_iterator it = device_status_map.begin();
         it != device_status_map.end(); ++it) {
      device_status_dict->SetInteger(
          base::StringPrintf("%d", it->first), it->second);
    }

    std::string device_id = base::StringPrintf(
        kAdbDeviceIdFormat,
        sit->first.c_str());
    result.Set(device_id, device_status_dict);
  }
  callback_.Run(result);
}
