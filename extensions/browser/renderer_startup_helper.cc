// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/renderer_startup_helper.h"

#include "base/values.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extensions_client.h"
#include "ui/base/webui/web_ui_util.h"

using content::BrowserContext;

namespace extensions {

RendererStartupHelper::RendererStartupHelper(BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(browser_context);
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

RendererStartupHelper::~RendererStartupHelper() {}

void RendererStartupHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_RENDERER_PROCESS_CREATED, type);
  content::RenderProcessHost* process =
      content::Source<content::RenderProcessHost>(source).ptr();
  if (!ExtensionsBrowserClient::Get()->IsSameContext(
          browser_context_, process->GetBrowserContext()))
    return;

  // Platform apps need to know the system font.
  // TODO(dbeam): this is not the system font in all cases.
  process->Send(new ExtensionMsg_SetSystemFont(webui::GetFontFamily(),
                                               webui::GetFontSize()));

  // Scripting whitelist. This is modified by tests and must be communicated
  // to renderers.
  process->Send(new ExtensionMsg_SetScriptingWhitelist(
      extensions::ExtensionsClient::Get()->GetScriptingWhitelist()));

  // If the new render process is a WebView guest process, propagate the WebView
  // partition ID to it.
  std::string webview_partition_id = WebViewGuest::GetPartitionID(process);
  if (!webview_partition_id.empty()) {
    process->Send(new ExtensionMsg_SetWebViewPartitionID(
        WebViewGuest::GetPartitionID(process)));
  }

  // Loaded extensions.
  std::vector<ExtensionMsg_Loaded_Params> loaded_extensions;
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();
  for (const auto& ext : extensions) {
    // Renderers don't need to know about themes.
    if (!ext->is_theme()) {
      // TODO(kalman): Only include tab specific permissions for extension
      // processes, no other process needs it, so it's mildly wasteful.
      // I am not sure this is possible to know this here, at such a low
      // level of the stack. Perhaps site isolation can help.
      bool include_tab_permissions = true;
      loaded_extensions.push_back(
          ExtensionMsg_Loaded_Params(ext.get(), include_tab_permissions));
    }
  }
  process->Send(new ExtensionMsg_Loaded(loaded_extensions));
}

//////////////////////////////////////////////////////////////////////////////

// static
RendererStartupHelper* RendererStartupHelperFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<RendererStartupHelper*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
RendererStartupHelperFactory* RendererStartupHelperFactory::GetInstance() {
  return base::Singleton<RendererStartupHelperFactory>::get();
}

RendererStartupHelperFactory::RendererStartupHelperFactory()
    : BrowserContextKeyedServiceFactory(
          "RendererStartupHelper",
          BrowserContextDependencyManager::GetInstance()) {
  // No dependencies on other services.
}

RendererStartupHelperFactory::~RendererStartupHelperFactory() {}

KeyedService* RendererStartupHelperFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new RendererStartupHelper(context);
}

BrowserContext* RendererStartupHelperFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

bool RendererStartupHelperFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
