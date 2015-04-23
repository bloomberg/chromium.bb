// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_permission_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"
#include "content/shell/browser/layout_test/layout_test_notification_manager.h"

namespace content {

LayoutTestPermissionManager::PermissionDescription::PermissionDescription(
    PermissionType type,
    const GURL& origin,
    const GURL& embedding_origin)
    : type(type),
      origin(origin),
      embedding_origin(embedding_origin) {
}

bool LayoutTestPermissionManager::PermissionDescription::operator==(
    const PermissionDescription& other) const {
  return type == other.type &&
         origin == other.origin &&
         embedding_origin == other.embedding_origin;
}

size_t LayoutTestPermissionManager::PermissionDescription::Hash::operator()(
    const PermissionDescription& description) const {
  size_t hash = BASE_HASH_NAMESPACE::hash<int>()(static_cast<int>(
      description.type));
  hash += BASE_HASH_NAMESPACE::hash<std::string>()(
      description.embedding_origin.spec());
  hash += BASE_HASH_NAMESPACE::hash<std::string>()(
      description.origin.spec());
  return hash;
}

LayoutTestPermissionManager::LayoutTestPermissionManager()
    : PermissionManager() {
}

LayoutTestPermissionManager::~LayoutTestPermissionManager() {
}

void LayoutTestPermissionManager::RequestPermission(
    PermissionType permission,
    WebContents* web_contents,
    int request_id,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(PermissionStatus)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  callback.Run(GetPermissionStatus(
      permission, requesting_origin,
      web_contents->GetLastCommittedURL().GetOrigin()));
}

void LayoutTestPermissionManager::CancelPermissionRequest(
    PermissionType permission,
    WebContents* web_contents,
    int request_id,
    const GURL& requesting_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void LayoutTestPermissionManager::ResetPermission(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::AutoLock lock(permissions_lock_);

  auto it = permissions_.find(
      PermissionDescription(permission, requesting_origin, embedding_origin));
  if (it == permissions_.end())
    return;
  permissions_.erase(it);;
}

PermissionStatus LayoutTestPermissionManager::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::AutoLock lock(permissions_lock_);

  auto it = permissions_.find(
      PermissionDescription(permission, requesting_origin, embedding_origin));
  if (it == permissions_.end())
    return PERMISSION_STATUS_DENIED;
  return it->second;
}

void LayoutTestPermissionManager::RegisterPermissionUsage(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

int LayoutTestPermissionManager::SubscribePermissionStatusChange(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(PermissionStatus)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(mlamouri): to be implemented, see https://crbug.com/475141
  return -1;
}

void LayoutTestPermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(mlamouri): to be implemented, see https://crbug.com/475141
}

void LayoutTestPermissionManager::SetPermission(PermissionType permission,
                                                PermissionStatus status,
                                                const GURL& origin,
                                                const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PermissionDescription description(permission, origin, embedding_origin);

  base::AutoLock lock(permissions_lock_);

  auto it = permissions_.find(description);
  if (it == permissions_.end()) {
    permissions_.insert(std::pair<PermissionDescription, PermissionStatus>(
        description, status));
  } else {
    it->second = status;
  }
}

void LayoutTestPermissionManager::ResetPermissions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::AutoLock lock(permissions_lock_);
  permissions_.clear();
}

}  // namespace content
