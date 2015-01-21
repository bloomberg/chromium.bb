// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_plugin_service.h"

namespace content {

FakePluginService::FakePluginService() {
}

FakePluginService::~FakePluginService() {
}

void FakePluginService::Init() {
}

void FakePluginService::StartWatchingPlugins() {
}

bool FakePluginService::GetPluginInfoArray(
    const GURL& url,
    const std::string& mime_type,
    bool allow_wildcard,
    std::vector<WebPluginInfo>* plugins,
    std::vector<std::string>* actual_mime_types) {
  return false;
}

bool FakePluginService::GetPluginInfo(int render_process_id,
                                      int render_frame_id,
                                      ResourceContext* context,
                                      const GURL& url,
                                      const GURL& page_url,
                                      const std::string& mime_type,
                                      bool allow_wildcard,
                                      bool* is_stale,
                                      WebPluginInfo* info,
                                      std::string* actual_mime_type) {
  *is_stale = false;
  return false;
}

bool FakePluginService::GetPluginInfoByPath(const base::FilePath& plugin_path,
                                            WebPluginInfo* info) {
  return false;
}

base::string16 FakePluginService::GetPluginDisplayNameByPath(
    const base::FilePath& path) {
  return base::string16();
}

void FakePluginService::GetPlugins(const GetPluginsCallback& callback) {
}

PepperPluginInfo* FakePluginService::GetRegisteredPpapiPluginInfo(
    const base::FilePath& plugin_path) {
  return nullptr;
}

void FakePluginService::SetFilter(PluginServiceFilter* filter) {
}

PluginServiceFilter* FakePluginService::GetFilter() {
  return nullptr;
}

void FakePluginService::ForcePluginShutdown(const base::FilePath& plugin_path) {
}

bool FakePluginService::IsPluginUnstable(const base::FilePath& path) {
  return false;
}

void FakePluginService::RefreshPlugins() {
}

void FakePluginService::AddExtraPluginPath(const base::FilePath& path) {
}

void FakePluginService::RemoveExtraPluginPath(const base::FilePath& path) {
}

void FakePluginService::AddExtraPluginDir(const base::FilePath& path) {
}

void FakePluginService::RegisterInternalPlugin(
    const WebPluginInfo& info,
    bool add_at_beginning) {
}

void FakePluginService::UnregisterInternalPlugin(const base::FilePath& path) {
}

void FakePluginService::GetInternalPlugins(
    std::vector<WebPluginInfo>* plugins) {
}

bool FakePluginService::NPAPIPluginsSupported() {
  return false;
}

void FakePluginService::EnableNpapiPluginsForTesting() {
}

void FakePluginService::DisablePluginsDiscoveryForTesting() {
}

#if defined(OS_MACOSX)
void FakePluginService::AppActivated() {
}
#elif defined(OS_WIN)
bool FakePluginService::GetPluginInfoFromWindow(
    HWND window,
    base::string16* plugin_name,
    base::string16* plugin_version) {
  return false;
}
#endif

bool FakePluginService::PpapiDevChannelSupported(
    BrowserContext* browser_context,
    const GURL& document_url) {
  return false;
}

}  // namespace content
