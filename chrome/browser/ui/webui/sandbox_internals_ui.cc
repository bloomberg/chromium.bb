// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sandbox_internals_ui.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

#if defined(OS_LINUX)
#include "content/public/browser/zygote_host_linux.h"
#include "services/service_manager/sandbox/sandbox.h"
#endif

namespace {

#if defined(OS_LINUX)
static void SetSandboxStatusData(content::WebUIDataSource* source) {
  // Get expected sandboxing status of renderers.
  const int status =
      content::ZygoteHost::GetInstance()->GetRendererSandboxStatus();

  source->AddBoolean("suid", status & service_manager::Sandbox::kSUID);
  source->AddBoolean("userNs", status & service_manager::Sandbox::kUserNS);
  source->AddBoolean("pidNs", status & service_manager::Sandbox::kPIDNS);
  source->AddBoolean("netNs", status & service_manager::Sandbox::kNetNS);
  source->AddBoolean("seccompBpf",
                     status & service_manager::Sandbox::kSeccompBPF);
  source->AddBoolean("seccompTsync",
                     status & service_manager::Sandbox::kSeccompTSYNC);
  source->AddBoolean("yama", status & service_manager::Sandbox::kYama);

  // Require either the setuid or namespace sandbox for our first-layer sandbox.
  bool good_layer1 = (status & service_manager::Sandbox::kSUID ||
                      status & service_manager::Sandbox::kUserNS) &&
                     status & service_manager::Sandbox::kPIDNS &&
                     status & service_manager::Sandbox::kNetNS;
  // A second-layer sandbox is also required to be adequately sandboxed.
  bool good_layer2 = status & service_manager::Sandbox::kSeccompBPF;
  source->AddBoolean("sandboxGood", good_layer1 && good_layer2);
}
#endif

content::WebUIDataSource* CreateDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISandboxHost);
  source->SetDefaultResource(IDR_SANDBOX_INTERNALS_HTML);
  source->AddResourcePath("sandbox_internals.js", IDR_SANDBOX_INTERNALS_JS);
  source->UseGzip();

#if defined(OS_LINUX)
  SetSandboxStatusData(source);
  source->SetJsonPath("strings.js");
#endif

  return source;
}

}  // namespace

SandboxInternalsUI::SandboxInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateDataSource());
}

void SandboxInternalsUI::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
#if defined(OS_ANDROID)
  render_frame_host->Send(new ChromeViewMsg_AddSandboxStatusExtension(
      render_frame_host->GetRoutingID()));
#endif
}

SandboxInternalsUI::~SandboxInternalsUI() {}
