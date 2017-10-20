// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/pattern.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/shell/browser/layout_test/blink_test_controller.h"
#include "content/shell/browser/layout_test/layout_test_bluetooth_fake_adapter_setter_impl.h"
#include "content/shell/browser/layout_test/layout_test_browser_context.h"
#include "content/shell/browser/layout_test/layout_test_browser_main_parts.h"
#include "content/shell/browser/layout_test/layout_test_message_filter.h"
#include "content/shell/browser/layout_test/layout_test_notification_manager.h"
#include "content/shell/browser/layout_test/layout_test_resource_dispatcher_host_delegate.h"
#include "content/shell/browser/layout_test/mojo_layout_test_helper.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/common/layout_test/layout_test_switches.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/renderer/layout_test/blink_test_helpers.h"
#include "device/bluetooth/test/fake_bluetooth.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
namespace {

LayoutTestContentBrowserClient* g_layout_test_browser_client;

void BindLayoutTestHelper(mojom::MojoLayoutTestHelperRequest request,
                          RenderFrameHost* render_frame_host) {
  MojoLayoutTestHelper::Create(std::move(request));
}

}  // namespace

LayoutTestContentBrowserClient::LayoutTestContentBrowserClient() {
  DCHECK(!g_layout_test_browser_client);

  layout_test_notification_manager_.reset(new LayoutTestNotificationManager());

  g_layout_test_browser_client = this;
}

LayoutTestContentBrowserClient::~LayoutTestContentBrowserClient() {
  g_layout_test_browser_client = nullptr;
}

LayoutTestContentBrowserClient* LayoutTestContentBrowserClient::Get() {
  return g_layout_test_browser_client;
}

LayoutTestBrowserContext*
LayoutTestContentBrowserClient::GetLayoutTestBrowserContext() {
  return static_cast<LayoutTestBrowserContext*>(browser_context());
}

void LayoutTestContentBrowserClient::SetPopupBlockingEnabled(
    bool block_popups) {
  block_popups_ = block_popups;
}

LayoutTestNotificationManager*
LayoutTestContentBrowserClient::GetLayoutTestNotificationManager() {
  return layout_test_notification_manager_.get();
}

void LayoutTestContentBrowserClient::RenderProcessWillLaunch(
    RenderProcessHost* host) {
  ShellContentBrowserClient::RenderProcessWillLaunch(host);

  StoragePartition* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context());
  host->AddFilter(new LayoutTestMessageFilter(
      host->GetID(),
      partition->GetDatabaseTracker(),
      partition->GetQuotaManager(),
      partition->GetURLRequestContext()));

  host->Send(new ShellViewMsg_SetWebKitSourceDir(GetWebKitRootDirFilePath()));
}

void LayoutTestContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    AssociatedInterfaceRegistry* associated_registry,
    RenderProcessHost* render_process_host) {
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::UI);
  registry->AddInterface(
      base::Bind(&LayoutTestBluetoothFakeAdapterSetterImpl::Create),
      ui_task_runner);

  registry->AddInterface(base::Bind(&bluetooth::FakeBluetooth::Create),
                         ui_task_runner);
  registry->AddInterface(base::Bind(&MojoLayoutTestHelper::Create));
}

void LayoutTestContentBrowserClient::OverrideWebkitPrefs(
    RenderViewHost* render_view_host,
    WebPreferences* prefs) {
  BlinkTestController::Get()->OverrideWebkitPrefs(prefs);
}

void LayoutTestContentBrowserClient::ResourceDispatcherHostCreated() {
  set_resource_dispatcher_host_delegate(
      base::WrapUnique(new LayoutTestResourceDispatcherHostDelegate));
  ResourceDispatcherHost::Get()->SetDelegate(
      resource_dispatcher_host_delegate());
}

void LayoutTestContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  command_line->AppendSwitch(switches::kRunLayoutTest);
  ShellContentBrowserClient::AppendExtraCommandLineSwitches(command_line,
                                                            child_process_id);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAlwaysUseComplexText)) {
    command_line->AppendSwitch(switches::kAlwaysUseComplexText);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFontAntialiasing)) {
    command_line->AppendSwitch(switches::kEnableFontAntialiasing);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStableReleaseMode)) {
    command_line->AppendSwitch(switches::kStableReleaseMode);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLeakDetection)) {
    command_line->AppendSwitchASCII(
        switches::kEnableLeakDetection,
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnableLeakDetection));
  }
}

BrowserMainParts* LayoutTestContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  set_browser_main_parts(new LayoutTestBrowserMainParts(parameters));
  return shell_browser_main_parts();
}

void LayoutTestContentBrowserClient::GetQuotaSettings(
    BrowserContext* context,
    StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  // The 1GB limit is intended to give a large headroom to tests that need to
  // build up a large data set and issue many concurrent reads or writes.
  std::move(callback).Run(storage::GetHardCodedSettings(1024 * 1024 * 1024));
}

bool LayoutTestContentBrowserClient::DoesSiteRequireDedicatedProcess(
    BrowserContext* browser_context,
    const GURL& effective_site_url) {
  if (ShellContentBrowserClient::DoesSiteRequireDedicatedProcess(
          browser_context, effective_site_url))
    return true;
  url::Origin origin = url::Origin::Create(effective_site_url);
  return base::MatchPattern(origin.Serialize(), "*oopif.test");
}

PlatformNotificationService*
LayoutTestContentBrowserClient::GetPlatformNotificationService() {
  return layout_test_notification_manager_.get();
}

bool LayoutTestContentBrowserClient::CanCreateWindow(
    content::RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const GURL& source_origin,
    content::mojom::WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  *no_javascript_access = false;
  return !block_popups_ || user_gesture;
}

void LayoutTestContentBrowserClient::ExposeInterfacesToFrame(
    service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
        registry) {
  registry->AddInterface(base::Bind(&BindLayoutTestHelper));
}

}  // namespace content
