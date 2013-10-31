// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/fake_browser_context.h"

FakeBrowserContext::FakeBrowserContext(
    const std::string& name, const base::FilePath& path)
    : name_(name),
      path_(path) {
}

FakeBrowserContext::~FakeBrowserContext() {
}

base::FilePath FakeBrowserContext::GetPath() const {
  return path_;
}

bool FakeBrowserContext::IsOffTheRecord() const {
  return false;
}

net::URLRequestContextGetter* FakeBrowserContext::GetRequestContext() {
  return NULL;
}

net::URLRequestContextGetter*
    FakeBrowserContext::GetRequestContextForRenderProcess(
        int renderer_child_id) {
  return NULL;
}

net::URLRequestContextGetter* FakeBrowserContext::GetMediaRequestContext() {
  return NULL;
}

net::URLRequestContextGetter*
    FakeBrowserContext::GetMediaRequestContextForRenderProcess(
        int renderer_child_id) {
  return NULL;
}

net::URLRequestContextGetter*
    FakeBrowserContext::GetMediaRequestContextForStoragePartition(
        const base::FilePath& partition_path,
        bool in_memory) {
  return NULL;
}

void FakeBrowserContext::RequestMIDISysExPermission(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    const MIDISysExPermissionCallback& callback) {
}

void FakeBrowserContext::CancelMIDISysExPermissionRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
}

content::ResourceContext* FakeBrowserContext::GetResourceContext() {
  return NULL;
}

content::DownloadManagerDelegate*
    FakeBrowserContext::GetDownloadManagerDelegate() {
  return NULL;
}

content::GeolocationPermissionContext*
    FakeBrowserContext::GetGeolocationPermissionContext() {
  return NULL;
}

quota::SpecialStoragePolicy* FakeBrowserContext::GetSpecialStoragePolicy() {
  return NULL;
}
