// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_target_impl.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"

using content::BrowserThread;
using content::DevToolsAgentHost;
using content::WebContents;

DevToolsTargetImpl::~DevToolsTargetImpl() {
}

DevToolsTargetImpl::DevToolsTargetImpl(
    scoped_refptr<DevToolsAgentHost> agent_host)
    : devtools_discovery::BasicTargetDescriptor(agent_host) {
}

// static
std::vector<DevToolsTargetImpl*> DevToolsTargetImpl::EnumerateAll() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<DevToolsTargetImpl*> result;
  DevToolsAgentHost::List agents = DevToolsAgentHost::GetOrCreateAll();
  for (DevToolsAgentHost::List::iterator it = agents.begin();
       it != agents.end(); ++it) {
    result.push_back(new DevToolsTargetImpl(*it));
  }
  return result;
}
