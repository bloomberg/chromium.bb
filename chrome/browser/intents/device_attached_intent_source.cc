// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/device_attached_intent_source.h"

#include <vector>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/string16.h"
#include "base/memory/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/browser/web_contents_delegate.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_service_data.h"

using base::SystemMonitor;
using chrome::MediaStorageUtil;
using content::WebContentsDelegate;
using webkit_glue::WebIntentServiceData;

namespace {

// Specifies device attached web intent kAction.
const char kAction[] = "chrome-extension://attach";

// Specifies device attached web intent type.
const char kIntentType[] = "chrome-extension://filesystem";

// Dispatch intent only when there is a list of registered services. This is a
// helper class that stores the attached media device information and decides
// whether to dispatch an intent or not.
class DispatchIntentTaskHelper
    : public base::RefCountedThreadSafe<DispatchIntentTaskHelper> {
 public:
  DispatchIntentTaskHelper(
      const base::WeakPtr<DeviceAttachedIntentSource> source,
      SystemMonitor::RemovableStorageInfo device_info)
      : source_(source),
        device_info_(device_info) {
  }

  // Query callback for WebIntentsRegistry::GetIntentServices function.
  void MayDispatchIntentForService(
      const std::vector<WebIntentServiceData>& services) {
    if (!services.empty() && source_)
      source_->DispatchIntentsForService(device_info_);
  }

 private:
  friend class base::RefCountedThreadSafe<DispatchIntentTaskHelper>;

  ~DispatchIntentTaskHelper() {}

  // A weak pointer to |DeviceAttachedIntentSource| object to dispatch a
  // web intent.
  base::WeakPtr<DeviceAttachedIntentSource> source_;

  // Store the device info. This is used while registering the device as file
  // system.
  const SystemMonitor::RemovableStorageInfo device_info_;

  DISALLOW_COPY_AND_ASSIGN(DispatchIntentTaskHelper);
};

}  // namespace

DeviceAttachedIntentSource::DeviceAttachedIntentSource(
    Browser* browser, WebContentsDelegate* delegate)
    : browser_(browser), delegate_(delegate) {
  SystemMonitor* sys_monitor = SystemMonitor::Get();
  if (sys_monitor)
    sys_monitor->AddDevicesChangedObserver(this);
}

DeviceAttachedIntentSource::~DeviceAttachedIntentSource() {
  SystemMonitor* sys_monitor = SystemMonitor::Get();
  if (sys_monitor)
    sys_monitor->RemoveDevicesChangedObserver(this);
}

void DeviceAttachedIntentSource::OnRemovableStorageAttached(
    const std::string& id,
    const string16& name,
    const FilePath::StringType& location) {
  if (!browser_->window()->IsActive())
    return;

  // TODO(kmadhusu): Dispatch intents on incognito window.
  if (browser_->profile()->IsOffTheRecord())
    return;

  // Only handle mass storage for now.
  // TODO(kmadhusu): Handle all device types. http://crbug.com/140353.
  if (!MediaStorageUtil::IsMassStorageDevice(id))
    return;
  DCHECK(MediaStorageUtil::IsRemovableDevice(id));

  // Sanity checks for |device_path|.
  const FilePath device_path(location);
  if (!device_path.IsAbsolute() || device_path.ReferencesParent())
    return;

  SystemMonitor::RemovableStorageInfo device_info(id, name, location);
  scoped_refptr<DispatchIntentTaskHelper> task = new DispatchIntentTaskHelper(
      AsWeakPtr(), device_info);
  WebIntentsRegistryFactory::GetForProfile(browser_->profile())->
      GetIntentServices(
          UTF8ToUTF16(kAction), UTF8ToUTF16(kIntentType),
          base::Bind(&DispatchIntentTaskHelper::MayDispatchIntentForService,
                     task.get()));
}

void DeviceAttachedIntentSource::DispatchIntentsForService(
    const base::SystemMonitor::RemovableStorageInfo& device_info) {
  // Store the media device info locally.
  device_id_map_.insert(std::make_pair(device_info.device_id, device_info));

  std::string device_name(UTF16ToUTF8(device_info.name));
  const FilePath device_path(device_info.location);

  // TODO(kinuko, kmadhusu): Use a different file system type for MTP.
  // TODO(kmadhusu): To manage the registered file systems efficiently, register
  // the attached device media file system using MediaFileSystemRegistry.
  const std::string fs_id = fileapi::IsolatedContext::GetInstance()->
      RegisterFileSystemForPath(fileapi::kFileSystemTypeNativeMedia,
                                device_path, &device_name);

  DCHECK(!fs_id.empty());
  webkit_glue::WebIntentData intent(
      UTF8ToUTF16(kAction), UTF8ToUTF16(kIntentType), device_name, fs_id);

  delegate_->WebIntentDispatch(NULL  /* no WebContents */,
                               content::WebIntentsDispatcher::Create(intent));
}

void DeviceAttachedIntentSource::OnRemovableStorageDetached(
    const std::string& id) {
  DeviceIdToInfoMap::iterator it = device_id_map_.find(id);
  if (it == device_id_map_.end())
    return;

  FilePath path(it->second.location);
  fileapi::IsolatedContext::GetInstance()->RevokeFileSystemByPath(path);
  device_id_map_.erase(it);
}
