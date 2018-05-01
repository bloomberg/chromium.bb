// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sandbox_internals_ui.h"

#include <string>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

#if defined(OS_ANDROID)
#include "chrome/common/sandbox_status_extension_android.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#endif

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

  source->AddBoolean("suid", status & service_manager::SandboxLinux::kSUID);
  source->AddBoolean("userNs", status & service_manager::SandboxLinux::kUserNS);
  source->AddBoolean("pidNs", status & service_manager::SandboxLinux::kPIDNS);
  source->AddBoolean("netNs", status & service_manager::SandboxLinux::kNetNS);
  source->AddBoolean("seccompBpf",
                     status & service_manager::SandboxLinux::kSeccompBPF);
  source->AddBoolean("seccompTsync",
                     status & service_manager::SandboxLinux::kSeccompTSYNC);
  source->AddBoolean("yama", status & service_manager::SandboxLinux::kYama);

  // Require either the setuid or namespace sandbox for our first-layer sandbox.
  bool good_layer1 = (status & service_manager::SandboxLinux::kSUID ||
                      status & service_manager::SandboxLinux::kUserNS) &&
                     status & service_manager::SandboxLinux::kPIDNS &&
                     status & service_manager::SandboxLinux::kNetNS;
  // A second-layer sandbox is also required to be adequately sandboxed.
  bool good_layer2 = status & service_manager::SandboxLinux::kSeccompBPF;
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
  chrome::mojom::SandboxStatusExtensionAssociatedPtr sandbox_status;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &sandbox_status);
  sandbox_status->AddSandboxStatusExtension();
#endif
}

SandboxInternalsUI::~SandboxInternalsUI() {}
