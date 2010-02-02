// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/dom_storage_area.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/task.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/in_process_webkit/dom_storage_context.h"
#include "chrome/browser/in_process_webkit/dom_storage_dispatcher_host.h"
#include "chrome/browser/in_process_webkit/dom_storage_namespace.h"
#include "chrome/browser/in_process_webkit/dom_storage_permission_request.h"
#include "chrome/browser/host_content_settings_map.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebSecurityOrigin;
using WebKit::WebStorageArea;
using WebKit::WebString;
using WebKit::WebURL;

DOMStorageArea::DOMStorageArea(
    const string16& origin,
    int64 id,
    DOMStorageNamespace* owner,
    HostContentSettingsMap* host_content_settings_map)
    : origin_(origin),
      id_(id),
      owner_(owner),
      host_content_settings_map_(host_content_settings_map),
      host_(GURL(origin).host()) {
  DCHECK(owner_);
  DCHECK(host_content_settings_map_);
}

DOMStorageArea::~DOMStorageArea() {
}

unsigned DOMStorageArea::Length() {
  if (!CheckContentSetting())
    return 0;  // Pretend we're an empty store.

  CreateWebStorageAreaIfNecessary();
  return storage_area_->length();
}

NullableString16 DOMStorageArea::Key(unsigned index) {
  if (!CheckContentSetting())
    return NullableString16(true);  // Null string.

  CreateWebStorageAreaIfNecessary();
  return storage_area_->key(index);
}

NullableString16 DOMStorageArea::GetItem(const string16& key) {
  if (!CheckContentSetting())
    return NullableString16(true);  // Null string.

  CreateWebStorageAreaIfNecessary();
  return storage_area_->getItem(key);
}

NullableString16 DOMStorageArea::SetItem(
    const string16& key, const string16& value, bool* quota_exception) {
  if (!CheckContentSetting()) {
    *quota_exception = true;
    return NullableString16(true);  // Ignored if exception is true.
  }

  CreateWebStorageAreaIfNecessary();
  WebString old_value;
  storage_area_->setItem(key, value, WebURL(), *quota_exception, old_value);
  return old_value;
}

NullableString16 DOMStorageArea::RemoveItem(const string16& key) {
  if (!CheckContentSetting())
    return NullableString16(true);  // Indicates nothing removed.

  CreateWebStorageAreaIfNecessary();
  WebString old_value;
  storage_area_->removeItem(key, WebURL(), old_value);
  return old_value;
}

bool DOMStorageArea::Clear() {
  if (!CheckContentSetting())
    return false;  // Nothing cleared.

  CreateWebStorageAreaIfNecessary();
  bool somethingCleared;
  storage_area_->clear(WebURL(), somethingCleared);
  return somethingCleared;
}

void DOMStorageArea::PurgeMemory() {
  storage_area_.reset();
}

void DOMStorageArea::CreateWebStorageAreaIfNecessary() {
  if (!storage_area_.get())
    storage_area_.reset(owner_->CreateWebStorageArea(origin_));
}

bool DOMStorageArea::CheckContentSetting() {
  ContentSetting content_setting =
      host_content_settings_map_->GetContentSetting(
          host_, CONTENT_SETTINGS_TYPE_COOKIES);

  if (content_setting == CONTENT_SETTING_ASK) {
    WebSecurityOrigin security_origin(
        WebSecurityOrigin::createFromString(origin_));
    FilePath::StringType file_name = webkit_glue::WebStringToFilePath(
        security_origin.databaseIdentifier()).value();
    file_name.append(DOMStorageContext::kLocalStorageExtension);
    FilePath file_path = webkit_glue::WebStringToFilePath(
        owner_->data_dir_path()).Append(file_name);

    bool file_exists = false;
    int64 size = 0;
    base::Time last_modified;
    file_util::FileInfo file_info;
    if (file_util::GetFileInfo(file_path, &file_info)) {
      file_exists = true;
      size = file_info.size;
      last_modified = file_info.last_modified;
    }
    DOMStoragePermissionRequest request(host_, file_exists, size,
                                        last_modified);
    // TODO(jorlow/darin): Do something useful instead of calling DoSomething.
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(&DOMStoragePermissionRequest::DoSomething,
                            &request));
    content_setting = request.WaitOnResponse();
  }

  if (content_setting == CONTENT_SETTING_ALLOW)
    return true;
  DCHECK(content_setting == CONTENT_SETTING_BLOCK);
  return false;
}
