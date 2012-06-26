// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/device_attached_intent_source.h"

#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/browser/web_contents_delegate.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/glue/web_intent_data.h"

using base::SystemMonitor;
using content::WebContentsDelegate;

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

void DeviceAttachedIntentSource::OnMediaDeviceAttached(
    const base::SystemMonitor::DeviceIdType& id,
    const std::string& name,
    const FilePath& device_path) {
  if (!browser_->window()->IsActive())
    return;

  // Sanity checks for |device_path|.
  if (!device_path.IsAbsolute() || device_path.ReferencesParent() ||
      device_path.BaseName().IsAbsolute() || device_path.BaseName().empty()) {
    return;
  }

  // Register device path as an isolated file system.
  std::set<FilePath> fileset;
  fileset.insert(device_path);
  const std::string filesystem_id =
      fileapi::IsolatedContext::GetInstance()->
          RegisterIsolatedFileSystem(fileset);
  CHECK(!filesystem_id.empty());
  webkit_glue::WebIntentData intent(
      ASCIIToUTF16("chrome-extension://attach"),
      ASCIIToUTF16("chrome-extension://filesystem"),
      device_path,
      filesystem_id);

  content::WebIntentsDispatcher* dispatcher =
      content::WebIntentsDispatcher::Create(intent);
  delegate_->WebIntentDispatch(NULL, dispatcher);
}
