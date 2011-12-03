// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_dispatcher.h"

#include "base/command_line.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/extensions/app_bindings.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/chrome_webstore_bindings.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_groups.h"
#include "chrome/renderer/extensions/file_browser_private_bindings.h"
#include "chrome/renderer/extensions/miscellaneous_bindings.h"
#include "chrome/renderer/extensions/schema_generated_bindings.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/public/renderer/render_thread.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

namespace {
static const int64 kInitialExtensionIdleHandlerDelayMs = 5*1000;
static const int64 kMaxExtensionIdleHandlerDelayMs = 5*60*1000;
}

using extensions::MiscellaneousBindings;
using extensions::SchemaGeneratedBindings;
using WebKit::WebDataSource;
using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebSecurityPolicy;
using WebKit::WebString;
using WebKit::WebVector;
using WebKit::WebView;
using content::RenderThread;

ExtensionDispatcher::ExtensionDispatcher()
    : is_webkit_initialized_(false),
      webrequest_adblock_(false),
      webrequest_adblock_plus_(false),
      webrequest_other_(false) {
  const CommandLine& command_line = *(CommandLine::ForCurrentProcess());
  is_extension_process_ =
      command_line.HasSwitch(switches::kExtensionProcess) ||
      command_line.HasSwitch(switches::kSingleProcess);

  if (is_extension_process_) {
    RenderThread::Get()->SetIdleNotificationDelayInMs(
        kInitialExtensionIdleHandlerDelayMs);
  }

  user_script_slave_.reset(new UserScriptSlave(&extensions_));
}

ExtensionDispatcher::~ExtensionDispatcher() {
}

bool ExtensionDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionDispatcher, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnMessageInvoke)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DeliverMessage, OnDeliverMessage)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetFunctionNames, OnSetFunctionNames)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Loaded, OnLoaded)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Unloaded, OnUnloaded)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetScriptingWhitelist,
                        OnSetScriptingWhitelist)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ActivateExtension, OnActivateExtension)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ActivateApplication, OnActivateApplication)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdatePermissions, OnUpdatePermissions)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateUserScripts, OnUpdateUserScripts)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UsingWebRequestAPI, OnUsingWebRequestAPI)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ExtensionDispatcher::WebKitInitialized() {
  // For extensions, we want to ensure we call the IdleHandler every so often,
  // even if the extension keeps up activity.
  if (is_extension_process_) {
    forced_idle_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kMaxExtensionIdleHandlerDelayMs),
        RenderThread::Get(), &RenderThread::IdleHandler);
  }

  RegisterExtension(new AppBindings(this), false);
  RegisterExtension(ChromeWebstoreExtension::Get(), false);

  // Add v8 extensions related to chrome extensions.
  RegisterExtension(new ChromeV8Extension(
      "extensions/json_schema.js", IDR_JSON_SCHEMA_JS, NULL), true);
  RegisterExtension(EventBindings::Get(this), true);
  RegisterExtension(new FileBrowserPrivateBindings(), true);
  RegisterExtension(MiscellaneousBindings::Get(this), true);
  RegisterExtension(SchemaGeneratedBindings::Get(this), true);
  RegisterExtension(new ChromeV8Extension(
      "extensions/apitest.js", IDR_EXTENSION_APITEST_JS, NULL), true);

  // Initialize host permissions for any extensions that were activated before
  // WebKit was initialized.
  for (std::set<std::string>::iterator iter = active_extension_ids_.begin();
       iter != active_extension_ids_.end(); ++iter) {
    const Extension* extension = extensions_.GetByID(*iter);
    if (extension)
      InitOriginPermissions(extension);
  }

  is_webkit_initialized_ = true;
}

void ExtensionDispatcher::IdleNotification() {
  if (is_extension_process_) {
    // Dampen the forced delay as well if the extension stays idle for long
    // periods of time.
    int64 forced_delay_ms = std::max(
        RenderThread::Get()->GetIdleNotificationDelayInMs(),
        kMaxExtensionIdleHandlerDelayMs);
    forced_idle_timer_.Stop();
    forced_idle_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(forced_delay_ms),
        RenderThread::Get(), &RenderThread::IdleHandler);
  }
}

void ExtensionDispatcher::OnSetFunctionNames(
    const std::vector<std::string>& names) {
  function_names_.clear();
  for (size_t i = 0; i < names.size(); ++i)
    function_names_.insert(names[i]);
}

void ExtensionDispatcher::OnMessageInvoke(const std::string& extension_id,
                                          const std::string& function_name,
                                          const ListValue& args,
                                          const GURL& event_url) {
  v8_context_set_.DispatchChromeHiddenMethod(
      extension_id, function_name, args, NULL, event_url);

  // Reset the idle handler each time there's any activity like event or message
  // dispatch, for which Invoke is the chokepoint.
  if (is_extension_process_) {
    RenderThread::Get()->ScheduleIdleHandler(
        kInitialExtensionIdleHandlerDelayMs);
  }

  // Tell the browser process that the event is dispatched and we're idle.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLazyBackgroundPages) &&
      function_name == "Event.dispatchJSON") { // may always be true
    RenderThread::Get()->Send(
        new ExtensionHostMsg_ExtensionEventAck(extension_id));
    CheckIdleStatus(extension_id);
  }
}

void ExtensionDispatcher::CheckIdleStatus(const std::string& extension_id) {
  if (!SchemaGeneratedBindings::HasPendingRequests(extension_id))
    RenderThread::Get()->Send(new ExtensionHostMsg_ExtensionIdle(extension_id));
}

void ExtensionDispatcher::OnDeliverMessage(int target_port_id,
                                           const std::string& message) {
  MiscellaneousBindings::DeliverMessage(
      v8_context_set_.GetAll(),
      target_port_id,
      message,
      NULL);  // All render views.
}

void ExtensionDispatcher::OnLoaded(
    const std::vector<ExtensionMsg_Loaded_Params>& loaded_extensions) {
  std::vector<WebString> platform_app_patterns;

  std::vector<ExtensionMsg_Loaded_Params>::const_iterator i;
  for (i = loaded_extensions.begin(); i != loaded_extensions.end(); ++i) {
    scoped_refptr<const Extension> extension(i->ConvertToExtension());
    if (!extension) {
      // This can happen if extension parsing fails for any reason. One reason
      // this can legitimately happen is if the
      // --enable-experimental-extension-apis changes at runtime, which happens
      // during browser tests. Existing renderers won't know about the change.
      continue;
    }

    extensions_.Insert(extension);

    if (extension->is_platform_app()) {
      platform_app_patterns.push_back(
          WebString::fromUTF8(extension->url().spec() + "*"));
    }
  }

  if (!platform_app_patterns.empty()) {
    // We have collected a set of platform-app extensions, so let's tell WebKit
    // about them so that it can provide a default stylesheet for them.
    //
    // TODO(miket): consider enhancing WebView to allow removing
    // single stylesheets, or else to edit the pattern set associated
    // with one.
    WebVector<WebString> patterns;
    patterns.assign(platform_app_patterns);
    WebView::addUserStyleSheet(
        WebString::fromUTF8(ResourceBundle::GetSharedInstance().
                            GetRawDataResource(IDR_PLATFORM_APP_CSS)),
        patterns,
        WebView::UserContentInjectInAllFrames,
        WebView::UserStyleInjectInExistingDocuments);
  }
}

void ExtensionDispatcher::OnUnloaded(const std::string& id) {
  extensions_.Remove(id);
  // If the extension is later reloaded with a different set of permissions,
  // we'd like it to get a new isolated world ID, so that it can pick up the
  // changed origin whitelist.
  user_script_slave_->RemoveIsolatedWorld(id);

  // We don't do anything with existing platform-app stylesheets. They will
  // stay resident, but the URL pattern corresponding to the unloaded
  // extension's URL just won't match anything anymore.
}

void ExtensionDispatcher::OnSetScriptingWhitelist(
    const Extension::ScriptingWhitelist& extension_ids) {
  Extension::SetScriptingWhitelist(extension_ids);
}

bool ExtensionDispatcher::IsApplicationActive(
    const std::string& extension_id) const {
  return active_application_ids_.find(extension_id) !=
      active_application_ids_.end();
}

bool ExtensionDispatcher::IsExtensionActive(
    const std::string& extension_id) const {
  return active_extension_ids_.find(extension_id) !=
      active_extension_ids_.end();
}

bool ExtensionDispatcher::AllowScriptExtension(
    WebFrame* frame,
    const std::string& v8_extension_name,
    int extension_group) {
  // NULL in unit tests.
  if (!RenderThread::Get())
    return true;

  // If we don't know about it, it was added by WebCore, so we should allow it.
  if (!RenderThread::Get()->IsRegisteredExtension(v8_extension_name))
    return true;

  // If the V8 extension is not restricted, allow it to run anywhere.
  if (!restricted_v8_extensions_.count(v8_extension_name))
    return true;

  // Extension-only bindings should be restricted to content scripts and
  // extension-blessed URLs.
  if (extension_group == EXTENSION_GROUP_CONTENT_SCRIPTS ||
      extensions_.ExtensionBindingsAllowed(ExtensionURLInfo(
          frame->document().securityOrigin(),
          UserScriptSlave::GetDataSourceURLForFrame(frame)))) {
    return true;
  }

  return false;
}

void ExtensionDispatcher::DidCreateScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> v8_context, int world_id) {
  std::string extension_id;
  if (!test_extension_id_.empty()) {
    extension_id = test_extension_id_;
  } else if (world_id != 0) {
    extension_id = user_script_slave_->GetExtensionIdForIsolatedWorld(world_id);
  } else {
    GURL frame_url = UserScriptSlave::GetDataSourceURLForFrame(frame);
    extension_id = extensions_.GetIdByURL(
      ExtensionURLInfo(frame->document().securityOrigin(), frame_url));
  }

  ChromeV8Context* context =
      new ChromeV8Context(v8_context, frame, extension_id);
  v8_context_set_.Add(context);

  context->DispatchOnLoadEvent(
      is_extension_process_,
      ChromeRenderProcessObserver::is_incognito_process());

  VLOG(1) << "Num tracked contexts: " << v8_context_set_.size();
}

void ExtensionDispatcher::WillReleaseScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> v8_context, int world_id) {
  ChromeV8Context* context = v8_context_set_.GetByV8Context(v8_context);
  if (!context)
    return;

  context->DispatchOnUnloadEvent();

  ChromeV8Extension::InstanceSet extensions = ChromeV8Extension::GetAll();
  for (ChromeV8Extension::InstanceSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    (*iter)->ContextWillBeReleased(context);
  }

  v8_context_set_.Remove(context);
  VLOG(1) << "Num tracked contexts: " << v8_context_set_.size();
}

void ExtensionDispatcher::SetTestExtensionId(const std::string& id) {
  test_extension_id_ = id;
}

void ExtensionDispatcher::OnActivateApplication(
    const std::string& extension_id) {
  active_application_ids_.insert(extension_id);
}

void ExtensionDispatcher::OnActivateExtension(
    const std::string& extension_id) {
  active_extension_ids_.insert(extension_id);

  // This is called when starting a new extension page, so start the idle
  // handler ticking.
  RenderThread::Get()->ScheduleIdleHandler(kInitialExtensionIdleHandlerDelayMs);

  UpdateActiveExtensions();

  const Extension* extension = extensions_.GetByID(extension_id);
  if (!extension)
    return;

  if (is_webkit_initialized_)
    InitOriginPermissions(extension);
}

void ExtensionDispatcher::InitOriginPermissions(const Extension* extension) {
  // TODO(jstritar): We should try to remove this special case. Also, these
  // whitelist entries need to be updated when the kManagement permission
  // changes.
  if (extension->HasAPIPermission(ExtensionAPIPermission::kManagement)) {
    WebSecurityPolicy::addOriginAccessWhitelistEntry(
        extension->url(),
        WebString::fromUTF8(chrome::kChromeUIScheme),
        WebString::fromUTF8(chrome::kChromeUIExtensionIconHost),
        false);
  }

  UpdateOriginPermissions(UpdatedExtensionPermissionsInfo::ADDED,
                          extension,
                          extension->GetActivePermissions()->explicit_hosts());
}

void ExtensionDispatcher::UpdateOriginPermissions(
    UpdatedExtensionPermissionsInfo::Reason reason,
    const Extension* extension,
    const URLPatternSet& origins) {
  for (URLPatternSet::const_iterator i = origins.begin();
       i != origins.end(); ++i) {
    const char* schemes[] = {
      chrome::kHttpScheme,
      chrome::kHttpsScheme,
      chrome::kFileScheme,
      chrome::kChromeUIScheme,
    };
    for (size_t j = 0; j < arraysize(schemes); ++j) {
      if (i->MatchesScheme(schemes[j])) {
        ((reason == UpdatedExtensionPermissionsInfo::REMOVED) ?
         WebSecurityPolicy::removeOriginAccessWhitelistEntry :
         WebSecurityPolicy::addOriginAccessWhitelistEntry)(
              extension->url(),
              WebString::fromUTF8(schemes[j]),
              WebString::fromUTF8(i->host()),
              i->match_subdomains());
      }
    }
  }
}

void ExtensionDispatcher::OnUpdatePermissions(
    int reason_id,
    const std::string& extension_id,
    const ExtensionAPIPermissionSet& apis,
    const URLPatternSet& explicit_hosts,
    const URLPatternSet& scriptable_hosts) {
  const Extension* extension = extensions_.GetByID(extension_id);
  if (!extension)
    return;

  scoped_refptr<const ExtensionPermissionSet> delta =
      new ExtensionPermissionSet(apis, explicit_hosts, scriptable_hosts);
  scoped_refptr<const ExtensionPermissionSet> old_active =
      extension->GetActivePermissions();
  UpdatedExtensionPermissionsInfo::Reason reason =
      static_cast<UpdatedExtensionPermissionsInfo::Reason>(reason_id);

  const ExtensionPermissionSet* new_active = NULL;
  if (reason == UpdatedExtensionPermissionsInfo::ADDED) {
    new_active = ExtensionPermissionSet::CreateUnion(old_active, delta);
  } else {
    CHECK_EQ(UpdatedExtensionPermissionsInfo::REMOVED, reason);
    new_active = ExtensionPermissionSet::CreateDifference(old_active, delta);
  }

  extension->SetActivePermissions(new_active);
  UpdateOriginPermissions(reason, extension, explicit_hosts);
}

void ExtensionDispatcher::OnUpdateUserScripts(
    base::SharedMemoryHandle scripts) {
  DCHECK(base::SharedMemory::IsHandleValid(scripts)) << "Bad scripts handle";
  user_script_slave_->UpdateScripts(scripts);
  UpdateActiveExtensions();
}

void ExtensionDispatcher::UpdateActiveExtensions() {
  // In single-process mode, the browser process reports the active extensions.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  std::set<std::string> active_extensions = active_extension_ids_;
  user_script_slave_->GetActiveExtensions(&active_extensions);
  child_process_logging::SetActiveExtensions(active_extensions);
}

void ExtensionDispatcher::RegisterExtension(v8::Extension* extension,
                                            bool restrict_to_extensions) {
  if (restrict_to_extensions)
    restricted_v8_extensions_.insert(extension->name());

  RenderThread::Get()->RegisterExtension(extension);
}

void ExtensionDispatcher::OnUsingWebRequestAPI(
    bool adblock, bool adblock_plus, bool other) {
  webrequest_adblock_ = adblock;
  webrequest_adblock_plus_ = adblock_plus;
  webrequest_other_ = other;
}
