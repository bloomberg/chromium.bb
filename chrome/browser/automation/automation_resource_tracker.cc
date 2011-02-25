// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_resource_tracker.h"

#include "chrome/common/automation_messages.h"

AutomationResourceTrackerImpl::AutomationResourceTrackerImpl(
    IPC::Message::Sender* sender)
    : sender_(sender) {
}

AutomationResourceTrackerImpl::~AutomationResourceTrackerImpl() {
}

int AutomationResourceTrackerImpl::AddImpl(const void* resource) {
  if (ContainsResourceImpl(resource))
    return resource_to_handle_[resource];

  int handle = GenerateHandle();
  DCHECK(!ContainsHandleImpl(handle));

  resource_to_handle_[resource] = handle;
  handle_to_resource_[handle] = resource;

  AddObserverTypeProxy(resource);

  return handle;
}

void AutomationResourceTrackerImpl::RemoveImpl(const void* resource) {
  if (!ContainsResourceImpl(resource))
    return;

  int handle = resource_to_handle_[resource];
  DCHECK_EQ(resource, handle_to_resource_[handle]);

  RemoveObserverTypeProxy(resource);

  resource_to_handle_.erase(resource);
  handle_to_resource_.erase(handle);
}

int AutomationResourceTrackerImpl::GenerateHandle() {
  static int handle = 0;
  return ++handle;
}

bool AutomationResourceTrackerImpl::ContainsResourceImpl(const void* resource) {
  return resource_to_handle_.find(resource) != resource_to_handle_.end();
}

bool AutomationResourceTrackerImpl::ContainsHandleImpl(int handle) {
  return handle_to_resource_.find(handle) != handle_to_resource_.end();
}

const void* AutomationResourceTrackerImpl::GetResourceImpl(int handle) {
  HandleToResourceMap::const_iterator iter = handle_to_resource_.find(handle);
  if (iter == handle_to_resource_.end())
    return NULL;

  return iter->second;
}

int AutomationResourceTrackerImpl::GetHandleImpl(const void* resource) {
  ResourceToHandleMap::const_iterator iter =
    resource_to_handle_.find(resource);
  if (iter == resource_to_handle_.end())
    return 0;

  return iter->second;
}

void AutomationResourceTrackerImpl::HandleCloseNotification(
    const void* resource) {
  if (!ContainsResourceImpl(resource))
    return;

  sender_->Send(
    new AutomationMsg_InvalidateHandle(resource_to_handle_[resource]));

  RemoveImpl(resource);
}
