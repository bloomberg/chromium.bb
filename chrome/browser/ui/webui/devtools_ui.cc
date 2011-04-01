// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools_ui.h"

#include "base/string_util.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager_backend.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/devtools_resources_map.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

std::string PathWithoutParams(const std::string& path) {
  return GURL(std::string("chrome-devtools://devtools/") + path)
      .path().substr(1);
}

}

class DevToolsDataSource : public ChromeURLDataManager::DataSource {
 public:
  DevToolsDataSource();

  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string& path) const;

 private:
  ~DevToolsDataSource() {}
  DISALLOW_COPY_AND_ASSIGN(DevToolsDataSource);
};


DevToolsDataSource::DevToolsDataSource()
    : DataSource(chrome::kChromeUIDevToolsHost, NULL) {
}

void DevToolsDataSource::StartDataRequest(const std::string& path,
                                          bool is_incognito,
                                          int request_id) {
  std::string filename = PathWithoutParams(path);

  int resource_id = -1;
  for (size_t i = 0; i < kDevtoolsResourcesSize; ++i) {
    if (filename == kDevtoolsResources[i].name) {
      resource_id = kDevtoolsResources[i].value;
      break;
    }
  }

  DLOG_IF(WARNING, -1 == resource_id) << "Unable to find dev tool resource: "
      << filename << ". If you compiled with debug_devtools=1, try running"
      " with --debug-devtools.";
  const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  scoped_refptr<RefCountedStaticMemory> bytes(rb.LoadDataResourceBytes(
      resource_id));
  SendResponse(request_id, bytes);
}

std::string DevToolsDataSource::GetMimeType(const std::string& path) const {
  std::string filename = PathWithoutParams(path);
  if (EndsWith(filename, ".html", false)) {
    return "text/html";
  } else if (EndsWith(filename, ".css", false)) {
    return "text/css";
  } else if (EndsWith(filename, ".js", false)) {
    return "application/javascript";
  } else if (EndsWith(filename, ".png", false)) {
    return "image/png";
  } else if (EndsWith(filename, ".gif", false)) {
    return "image/gif";
  }
  NOTREACHED();
  return "text/plain";
}

// static
void DevToolsUI::RegisterDevToolsDataSource() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  static bool registered = false;
  if (!registered) {
    DevToolsDataSource* data_source = new DevToolsDataSource();
    ChromeURLRequestContext* context = static_cast<ChromeURLRequestContext*>(
        Profile::GetDefaultRequestContext()->GetURLRequestContext());
    context->GetChromeURLDataManagerBackend()->AddDataSource(data_source);
    registered = true;
  }
}

DevToolsUI::DevToolsUI(TabContents* contents) : WebUI(contents) {
  DevToolsDataSource* data_source = new DevToolsDataSource();
  contents->profile()->GetChromeURLDataManager()->AddDataSource(data_source);
}

void DevToolsUI::RenderViewCreated(RenderViewHost* render_view_host) {
  render_view_host->Send(new DevToolsMsg_SetupDevToolsClient(
      render_view_host->routing_id()));
}
