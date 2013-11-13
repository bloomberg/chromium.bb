// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_targets_ui.h"

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_adb_bridge.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/devtools/port_forwarding_controller.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/worker_service.h"
#include "content/public/browser/worker_service_observer.h"
#include "content/public/common/process_type.h"
#include "net/base/escape.h"

using content::BrowserThread;
using content::WebContents;

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
const char kAdbPortStatus[] = "adbPortStatus";
const char kAdbBrowsersList[] = "browsers";

const char kAdbBrowserNameField[] = "adbBrowserName";
const char kAdbBrowserVersionField[] = "adbBrowserVersion";
const char kAdbBrowserChromeVersionField[] = "adbBrowserChromeVersion";
const char kAdbPagesList[] = "pages";

const char kAdbScreenWidthField[] = "adbScreenWidth";
const char kAdbScreenHeightField[] = "adbScreenHeight";
const char kAdbAttachedForeignField[]  = "adbAttachedForeign";

// RenderViewHostTargetsUIHandler ---------------------------------------------

class RenderViewHostTargetsUIHandler
    : public DevToolsTargetsUIHandler,
      public content::NotificationObserver {
 public:
  explicit RenderViewHostTargetsUIHandler(Callback callback);
  virtual ~RenderViewHostTargetsUIHandler();
 private:
  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void UpdateTargets();

  content::NotificationRegistrar notification_registrar_;
};

RenderViewHostTargetsUIHandler::RenderViewHostTargetsUIHandler(
    Callback callback)
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
  UpdateTargets();
}

void RenderViewHostTargetsUIHandler::UpdateTargets() {
  scoped_ptr<ListValue> list_value(new ListValue());

  std::map<WebContents*, DictionaryValue*> web_contents_to_descriptor_;
  std::vector<DevToolsTargetImpl*> guest_targets;

  DevToolsTargetImpl::List targets =
      DevToolsTargetImpl::EnumerateRenderViewHostTargets();

  STLDeleteValues(&targets_);
  for (DevToolsTargetImpl::List::iterator it = targets.begin();
      it != targets.end(); ++it) {
    scoped_ptr<DevToolsTargetImpl> target(*it);
    content::RenderViewHost* rvh = target->GetRenderViewHost();
    if (!rvh)
      continue;
    WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
    if (!web_contents)
      continue;

    DevToolsTargetImpl* target_ptr = target.get();
    targets_[target_ptr->GetId()] = target.release();
    if (rvh->GetProcess()->IsGuest()) {
      guest_targets.push_back(target_ptr);
    } else {
      DictionaryValue* descriptor = Serialize(*target_ptr);
      list_value->Append(descriptor);
      web_contents_to_descriptor_[web_contents] = descriptor;
    }
  }

  // Add the list of guest-views to each of its embedders.
  for (std::vector<DevToolsTargetImpl*>::iterator it(guest_targets.begin());
       it != guest_targets.end(); ++it) {
    DevToolsTargetImpl* guest = (*it);
    WebContents* guest_web_contents =
        WebContents::FromRenderViewHost(guest->GetRenderViewHost());
    WebContents* embedder = guest_web_contents->GetEmbedderWebContents();
    if (embedder && web_contents_to_descriptor_.count(embedder) > 0) {
      DictionaryValue* parent = web_contents_to_descriptor_[embedder];
      ListValue* guests = NULL;
      if (!parent->GetList(kGuestList, &guests)) {
        guests = new ListValue();
        parent->Set(kGuestList, guests);
      }
      guests->Append(Serialize(*guest));
    }
  }

  SendSerializedTargets(list_value.Pass());
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
      const string16& name,
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
    : public DevToolsTargetsUIHandler,
      public content::BrowserChildProcessObserver {
 public:
  explicit WorkerTargetsUIHandler(Callback callback);
  virtual ~WorkerTargetsUIHandler();

 private:
  // content::BrowserChildProcessObserver overrides.
  virtual void BrowserChildProcessHostConnected(
      const content::ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) OVERRIDE;

  void UpdateTargets(const DevToolsTargetImpl::List& targets);

  scoped_refptr<WorkerObserver> observer_;
};

WorkerTargetsUIHandler::WorkerTargetsUIHandler(Callback callback)
    : DevToolsTargetsUIHandler(kTargetSourceWorker, callback),
      observer_(new WorkerObserver()) {
  observer_->Start(base::Bind(&WorkerTargetsUIHandler::UpdateTargets,
                              base::Unretained(this)));
  BrowserChildProcessObserver::Add(this);
}

WorkerTargetsUIHandler::~WorkerTargetsUIHandler() {
  BrowserChildProcessObserver::Remove(this);
  observer_->Stop();
}

void WorkerTargetsUIHandler::BrowserChildProcessHostConnected(
    const content::ChildProcessData& data) {
  if (data.process_type == content::PROCESS_TYPE_WORKER)
    observer_->Enumerate();
}

void WorkerTargetsUIHandler::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  if (data.process_type == content::PROCESS_TYPE_WORKER)
    observer_->Enumerate();
}

void WorkerTargetsUIHandler::UpdateTargets(
    const DevToolsTargetImpl::List& targets) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<ListValue> list_value(new ListValue());
  STLDeleteValues(&targets_);
  for (DevToolsTargetImpl::List::const_iterator it = targets.begin();
      it != targets.end(); ++it) {
    DevToolsTargetImpl* target = *it;
    list_value->Append(Serialize(*target));
    targets_[target->GetId()] = target;
  }
  SendSerializedTargets(list_value.Pass());
}

// AdbTargetsUIHandler --------------------------------------------------------

class AdbTargetsUIHandler
    : public DevToolsRemoteTargetsUIHandler,
      public DevToolsAdbBridge::Listener {
 public:
  AdbTargetsUIHandler(Callback callback, Profile* profile);
  virtual ~AdbTargetsUIHandler();

  virtual void Open(const std::string& browser_id,
                    const std::string& url) OVERRIDE;

 private:
  // DevToolsAdbBridge::Listener overrides.
  virtual void RemoteDevicesChanged(
      DevToolsAdbBridge::RemoteDevices* devices) OVERRIDE;

  Profile* profile_;

  typedef std::map<std::string,
      scoped_refptr<DevToolsAdbBridge::RemoteBrowser> > RemoteBrowsers;
  RemoteBrowsers remote_browsers_;
};

AdbTargetsUIHandler::AdbTargetsUIHandler(Callback callback, Profile* profile)
    : DevToolsRemoteTargetsUIHandler(kTargetSourceAdb, callback),
      profile_(profile) {
  DevToolsAdbBridge* adb_bridge =
      DevToolsAdbBridge::Factory::GetForProfile(profile_);
  if (adb_bridge)
    adb_bridge->AddListener(this);
}

AdbTargetsUIHandler::~AdbTargetsUIHandler() {
  DevToolsAdbBridge* adb_bridge =
      DevToolsAdbBridge::Factory::GetForProfile(profile_);
  if (adb_bridge)
    adb_bridge->RemoveListener(this);
}

void AdbTargetsUIHandler::Open(const std::string& browser_id,
                           const std::string& url) {
  RemoteBrowsers::iterator it = remote_browsers_.find(browser_id);
  if (it !=  remote_browsers_.end())
    it->second->Open(url);
}

void AdbTargetsUIHandler::RemoteDevicesChanged(
    DevToolsAdbBridge::RemoteDevices* devices) {
  PortForwardingController* port_forwarding_controller =
      PortForwardingController::Factory::GetForProfile(profile_);
  PortForwardingController::DevicesStatus port_forwarding_status;
  if (port_forwarding_controller)
    port_forwarding_status =
        port_forwarding_controller->UpdateDeviceList(*devices);

  remote_browsers_.clear();
  STLDeleteValues(&targets_);

  scoped_ptr<ListValue> device_list(new ListValue());
  for (DevToolsAdbBridge::RemoteDevices::iterator dit = devices->begin();
       dit != devices->end(); ++dit) {
    DevToolsAdbBridge::RemoteDevice* device = dit->get();
    DictionaryValue* device_data = new DictionaryValue();
    device_data->SetString(kAdbModelField, device->GetModel());
    device_data->SetString(kAdbSerialField, device->GetSerial());
    device_data->SetBoolean(kAdbConnectedField, device->IsConnected());
    std::string device_id = base::StringPrintf(
        "device:%s",
        device->GetSerial().c_str());
    device_data->SetString(kTargetIdField, device_id);
    ListValue* browser_list = new ListValue();
    device_data->Set(kAdbBrowsersList, browser_list);

    DevToolsAdbBridge::RemoteBrowsers& browsers = device->browsers();
    for (DevToolsAdbBridge::RemoteBrowsers::iterator bit =
        browsers.begin(); bit != browsers.end(); ++bit) {
      DevToolsAdbBridge::RemoteBrowser* browser = bit->get();
      DictionaryValue* browser_data = new DictionaryValue();
      browser_data->SetString(kAdbBrowserNameField, browser->display_name());
      browser_data->SetString(kAdbBrowserVersionField, browser->version());
      DevToolsAdbBridge::RemoteBrowser::ParsedVersion parsed =
          browser->GetParsedVersion();
      browser_data->SetInteger(
          kAdbBrowserChromeVersionField,
          browser->IsChrome() && !parsed.empty() ? parsed[0] : 0);
      std::string browser_id = base::StringPrintf(
          "browser:%s:%s:%s:%s",
          device->GetSerial().c_str(), // Ensure uniqueness across devices.
          browser->display_name().c_str(),  // Sort by display name.
          browser->version().c_str(),  // Then by version.
          browser->socket().c_str());  // Ensure uniqueness on the device.
      browser_data->SetString(kTargetIdField, browser_id);
      browser_data->SetString(kTargetSourceField, source_id());
      remote_browsers_[browser_id] = browser;
      ListValue* page_list = new ListValue();
      browser_data->Set(kAdbPagesList, page_list);

      DevToolsTargetImpl::List pages = browser->CreatePageTargets();
      for (DevToolsTargetImpl::List::iterator it =
          pages.begin(); it != pages.end(); ++it) {
        DevToolsTargetImpl* target =  *it;
        DictionaryValue* target_data = Serialize(*target);
        target_data->SetBoolean(
            kAdbAttachedForeignField,
            target->IsAttached() &&
                !DevToolsAdbBridge::HasDevToolsWindow(target->GetId()));
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

    if (port_forwarding_controller) {
      PortForwardingController::DevicesStatus::iterator sit =
          port_forwarding_status.find(device->GetSerial());
      if (sit != port_forwarding_status.end()) {
        DictionaryValue* port_status_dict = new DictionaryValue();
        typedef PortForwardingController::PortStatusMap StatusMap;
        const StatusMap& port_status = sit->second;
        for (StatusMap::const_iterator it = port_status.begin();
             it != port_status.end(); ++it) {
          port_status_dict->SetInteger(
              base::StringPrintf("%d", it->first), it->second);
        }
        device_data->Set(kAdbPortStatus, port_status_dict);
      }
    }

    device_list->Append(device_data);
  }
  SendSerializedTargets(device_list.Pass());
}

} // namespace

// DevToolsTargetsUIHandler ---------------------------------------------------

DevToolsTargetsUIHandler::DevToolsTargetsUIHandler(
    const std::string& source_id,
    Callback callback)
    : source_id_(source_id),
      callback_(callback) {
}

DevToolsTargetsUIHandler::~DevToolsTargetsUIHandler() {
  STLDeleteValues(&targets_);
}

// static
scoped_ptr<DevToolsTargetsUIHandler>
DevToolsTargetsUIHandler::CreateForRenderers(
    DevToolsTargetsUIHandler::Callback callback) {
  return scoped_ptr<DevToolsTargetsUIHandler>(
      new RenderViewHostTargetsUIHandler(callback));
}

// static
scoped_ptr<DevToolsTargetsUIHandler>
DevToolsTargetsUIHandler::CreateForWorkers(
    DevToolsTargetsUIHandler::Callback callback) {
  return scoped_ptr<DevToolsTargetsUIHandler>(
      new WorkerTargetsUIHandler(callback));
}

void DevToolsTargetsUIHandler::Inspect(const std::string& target_id,
                                            Profile* profile) {
  TargetMap::iterator it = targets_.find(target_id);
  if (it != targets_.end())
    it->second->Inspect(profile);
}

void DevToolsTargetsUIHandler::Activate(const std::string& target_id) {
  TargetMap::iterator it = targets_.find(target_id);
  if (it != targets_.end())
    it->second->Activate();
}

void DevToolsTargetsUIHandler::Close(const std::string& target_id) {
  TargetMap::iterator it = targets_.find(target_id);
  if (it != targets_.end())
    it->second->Close();
}

void DevToolsTargetsUIHandler::Reload(const std::string& target_id) {
  TargetMap::iterator it = targets_.find(target_id);
  if (it != targets_.end())
    it->second->Reload();
}

base::DictionaryValue*
DevToolsTargetsUIHandler::Serialize(
    const DevToolsTargetImpl& target) {
  DictionaryValue* target_data = new DictionaryValue();
  target_data->SetString(kTargetSourceField, source_id_);
  target_data->SetString(kTargetIdField, target.GetId());
  target_data->SetString(kTargetTypeField, target.GetType());
  target_data->SetBoolean(kAttachedField, target.IsAttached());
  target_data->SetString(kUrlField, target.GetUrl().spec());
  target_data->SetString(kNameField, net::EscapeForHTML(target.GetTitle()));
  target_data->SetString(kFaviconUrlField, target.GetFaviconUrl().spec());
  target_data->SetString(kDescriptionField, target.GetDescription());
  return target_data;
}

void DevToolsTargetsUIHandler::SendSerializedTargets(
    scoped_ptr<ListValue> list) {
  callback_.Run(source_id_, list.Pass());
}

// DevToolsRemoteTargetsUIHandler ---------------------------------------------

DevToolsRemoteTargetsUIHandler::DevToolsRemoteTargetsUIHandler(
    const std::string& source_id,
    Callback callback)
    : DevToolsTargetsUIHandler(source_id, callback) {
}

// static
scoped_ptr<DevToolsRemoteTargetsUIHandler>
DevToolsRemoteTargetsUIHandler::CreateForAdb(
    DevToolsTargetsUIHandler::Callback callback, Profile* profile) {
  return scoped_ptr<DevToolsRemoteTargetsUIHandler>(
      new AdbTargetsUIHandler(callback, profile));
}
