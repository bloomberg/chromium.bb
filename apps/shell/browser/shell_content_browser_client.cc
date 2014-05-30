// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_content_browser_client.h"

#include "apps/shell/browser/shell_browser_context.h"
#include "apps/shell/browser/shell_browser_main_parts.h"
#include "apps/shell/browser/shell_extension_system.h"
#include "base/command_line.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/shell/browser/shell_browser_context.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "url/gurl.h"

using content::BrowserThread;
using extensions::ExtensionRegistry;

namespace apps {
namespace {

ShellContentBrowserClient* g_instance = NULL;

}  // namespace

ShellContentBrowserClient::ShellContentBrowserClient(
    ShellBrowserMainDelegate* browser_main_delegate)
    : browser_main_parts_(NULL), browser_main_delegate_(browser_main_delegate) {
  DCHECK(!g_instance);
  g_instance = this;
}

ShellContentBrowserClient::~ShellContentBrowserClient() { g_instance = NULL; }

// static
ShellContentBrowserClient* ShellContentBrowserClient::Get() {
  return g_instance;
}

content::BrowserContext* ShellContentBrowserClient::GetBrowserContext() {
  return browser_main_parts_->browser_context();
}

content::BrowserMainParts* ShellContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  browser_main_parts_ =
      new ShellBrowserMainParts(parameters, browser_main_delegate_);
  return browser_main_parts_;
}

void ShellContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  int render_process_id = host->GetID();
  host->AddFilter(new extensions::ExtensionMessageFilter(
      render_process_id, browser_main_parts_->browser_context()));
}

bool ShellContentBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context,
    const GURL& effective_url) {
  // This ensures that all render views created for a single app will use the
  // same render process (see content::SiteInstance::GetProcess). Otherwise the
  // default behavior of ContentBrowserClient will lead to separate render
  // processes for the background page and each app window view.
  return true;
}

net::URLRequestContextGetter* ShellContentBrowserClient::CreateRequestContext(
    content::BrowserContext* content_browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  // Handle only chrome-extension:// requests. app_shell does not support
  // chrome-extension-resource:// requests (it does not store shared extension
  // data in its installation directory).
  extensions::InfoMap* extension_info_map =
      browser_main_parts_->extension_system()->info_map();
  (*protocol_handlers)[extensions::kExtensionScheme] =
      linked_ptr<net::URLRequestJobFactory::ProtocolHandler>(
          extensions::CreateExtensionProtocolHandler(false /* is_incognito */,
                                                     extension_info_map));
  // Let content::ShellBrowserContext handle the rest of the setup.
  return browser_main_parts_->browser_context()->CreateRequestContext(
      protocol_handlers, request_interceptors.Pass());
}

bool ShellContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  // Keep in sync with ProtocolHandlers added in CreateRequestContext() and in
  // content::ShellURLRequestContextGetter::GetURLRequestContext().
  static const char* const kProtocolList[] = {
      url::kBlobScheme,
      content::kChromeDevToolsScheme,
      content::kChromeUIScheme,
      url::kDataScheme,
      url::kFileScheme,
      url::kFileSystemScheme,
      extensions::kExtensionScheme,
      extensions::kExtensionResourceScheme,
  };
  for (size_t i = 0; i < arraysize(kProtocolList); ++i) {
    if (url.scheme() == kProtocolList[i])
      return true;
  }
  return false;
}

void ShellContentBrowserClient::SiteInstanceGotProcess(
    content::SiteInstance* site_instance) {
  // If this isn't an extension renderer there's nothing to do.
  const extensions::Extension* extension = GetExtension(site_instance);
  if (!extension)
    return;

  extensions::ProcessMap::Get(browser_main_parts_->browser_context())
      ->Insert(extension->id(),
               site_instance->GetProcess()->GetID(),
               site_instance->GetId());

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&extensions::InfoMap::RegisterExtensionProcess,
                 browser_main_parts_->extension_system()->info_map(),
                 extension->id(),
                 site_instance->GetProcess()->GetID(),
                 site_instance->GetId()));
}

void ShellContentBrowserClient::SiteInstanceDeleting(
    content::SiteInstance* site_instance) {
  // If this isn't an extension renderer there's nothing to do.
  const extensions::Extension* extension = GetExtension(site_instance);
  if (!extension)
    return;

  extensions::ProcessMap::Get(browser_main_parts_->browser_context())
      ->Remove(extension->id(),
               site_instance->GetProcess()->GetID(),
               site_instance->GetId());

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&extensions::InfoMap::UnregisterExtensionProcess,
                 browser_main_parts_->extension_system()->info_map(),
                 extension->id(),
                 site_instance->GetProcess()->GetID(),
                 site_instance->GetId()));
}

void ShellContentBrowserClient::AppendExtraCommandLineSwitches(
    CommandLine* command_line, int child_process_id) {
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type == switches::kRendererProcess) {
    // TODO(jamescook): Should we check here if the process is in the extension
    // service process map, or can we assume all renderers are extension
    // renderers?
    command_line->AppendSwitch(extensions::switches::kExtensionProcess);
  }
}

void ShellContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
    std::vector<std::string>* additional_allowed_schemes) {
  ContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
      additional_allowed_schemes);
  additional_allowed_schemes->push_back(extensions::kExtensionScheme);
}

const extensions::Extension* ShellContentBrowserClient::GetExtension(
    content::SiteInstance* site_instance) {
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(site_instance->GetBrowserContext());
  return registry->enabled_extensions().GetExtensionOrAppByURL(
      site_instance->GetSiteURL());
}

}  // namespace apps
