// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager_delegate.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/one_shot_event.h"
#include "extensions/common/switches.h"

using content::BrowserContext;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;

namespace extensions {
class RenderViewHostDestructionObserver;
}
DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    extensions::RenderViewHostDestructionObserver);

namespace extensions {

namespace {

std::string GetExtensionID(RenderViewHost* render_view_host) {
  // This works for both apps and extensions because the site has been
  // normalized to the extension URL for hosted apps.
  content::SiteInstance* site_instance = render_view_host->GetSiteInstance();
  if (!site_instance)
    return std::string();

  const GURL& site_url = site_instance->GetSiteURL();

  if (!site_url.SchemeIs(kExtensionScheme) &&
      !site_url.SchemeIs(content::kGuestScheme))
    return std::string();

  return site_url.host();
}

std::string GetExtensionIDFromFrame(
    content::RenderFrameHost* render_frame_host) {
  // This works for both apps and extensions because the site has been
  // normalized to the extension URL for apps.
  if (!render_frame_host->GetSiteInstance())
    return std::string();

  return render_frame_host->GetSiteInstance()->GetSiteURL().host();
}

bool IsFrameInExtensionHost(ExtensionHost* extension_host,
                            content::RenderFrameHost* render_frame_host) {
  return WebContents::FromRenderFrameHost(render_frame_host) ==
      extension_host->host_contents();
}

void OnRenderViewHostUnregistered(BrowserContext* context,
                                  RenderViewHost* render_view_host) {
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_VIEW_UNREGISTERED,
      content::Source<BrowserContext>(context),
      content::Details<RenderViewHost>(render_view_host));
}

// Incognito profiles use this process manager. It is mostly a shim that decides
// whether to fall back on the original profile's ProcessManager based
// on whether a given extension uses "split" or "spanning" incognito behavior.
class IncognitoProcessManager : public ProcessManager {
 public:
  IncognitoProcessManager(BrowserContext* incognito_context,
                          BrowserContext* original_context,
                          ProcessManager* original_manager,
                          ExtensionRegistry* extension_registry);
  virtual ~IncognitoProcessManager() {}
  virtual bool CreateBackgroundHost(const Extension* extension,
                                    const GURL& url) OVERRIDE;
  virtual SiteInstance* GetSiteInstanceForURL(const GURL& url) OVERRIDE;

 private:
  ProcessManager* original_manager_;

  DISALLOW_COPY_AND_ASSIGN(IncognitoProcessManager);
};

static void CreateBackgroundHostForExtensionLoad(
    ProcessManager* manager, const Extension* extension) {
  DVLOG(1) << "CreateBackgroundHostForExtensionLoad";
  if (BackgroundInfo::HasPersistentBackgroundPage(extension))
    manager->CreateBackgroundHost(extension,
                                  BackgroundInfo::GetBackgroundURL(extension));
}

}  // namespace

class RenderViewHostDestructionObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<RenderViewHostDestructionObserver> {
 public:
  virtual ~RenderViewHostDestructionObserver() {}

 private:
  explicit RenderViewHostDestructionObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    BrowserContext* context = web_contents->GetBrowserContext();
    process_manager_ = ExtensionSystem::Get(context)->process_manager();
  }

  friend class content::WebContentsUserData<RenderViewHostDestructionObserver>;

  // content::WebContentsObserver overrides.
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) OVERRIDE {
    process_manager_->UnregisterRenderViewHost(render_view_host);
  }

  ProcessManager* process_manager_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostDestructionObserver);
};

struct ProcessManager::BackgroundPageData {
  // The count of things keeping the lazy background page alive.
  int lazy_keepalive_count;

  // Tracks if an impulse event has occured since the last polling check.
  bool keepalive_impulse;
  bool previous_keepalive_impulse;

  // True if the page responded to the ShouldSuspend message and is currently
  // dispatching the suspend event. During this time any events that arrive will
  // cancel the suspend process and an onSuspendCanceled event will be
  // dispatched to the page.
  bool is_closing;

  // Stores the value of the incremented
  // ProcessManager::last_background_close_sequence_id_ whenever the extension
  // is active. A copy of the ID is also passed in the callbacks and IPC
  // messages leading up to CloseLazyBackgroundPageNow. The process is aborted
  // if the IDs ever differ due to new activity.
  uint64 close_sequence_id;

  // Keeps track of when this page was last suspended. Used for perf metrics.
  linked_ptr<base::ElapsedTimer> since_suspended;

  BackgroundPageData()
      : lazy_keepalive_count(0),
        keepalive_impulse(false),
        previous_keepalive_impulse(false),
        is_closing(false),
        close_sequence_id(0) {}
};

//
// ProcessManager
//

// static
ProcessManager* ProcessManager::Create(BrowserContext* context) {
  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(context);
  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  if (client->IsGuestSession(context)) {
    // In the guest session, there is a single off-the-record context.  Unlike
    // a regular incognito mode, background pages of extensions must be
    // created regardless of whether extensions use "spanning" or "split"
    // incognito behavior.
    BrowserContext* original_context = client->GetOriginalContext(context);
    return new ProcessManager(context, original_context, extension_registry);
  }

  if (context->IsOffTheRecord()) {
    BrowserContext* original_context = client->GetOriginalContext(context);
    ProcessManager* original_manager =
        ExtensionSystem::Get(original_context)->process_manager();
    return new IncognitoProcessManager(
        context, original_context, original_manager, extension_registry);
  }

  return new ProcessManager(context, context, extension_registry);
}

// static
ProcessManager* ProcessManager::CreateForTesting(
    BrowserContext* context,
    ExtensionRegistry* extension_registry) {
  DCHECK(!context->IsOffTheRecord());
  return new ProcessManager(context, context, extension_registry);
}

// static
ProcessManager* ProcessManager::CreateIncognitoForTesting(
    BrowserContext* incognito_context,
    BrowserContext* original_context,
    ProcessManager* original_manager,
    ExtensionRegistry* extension_registry) {
  DCHECK(incognito_context->IsOffTheRecord());
  DCHECK(!original_context->IsOffTheRecord());
  return new IncognitoProcessManager(incognito_context,
                                     original_context,
                                     original_manager,
                                     extension_registry);
}

ProcessManager::ProcessManager(BrowserContext* context,
                               BrowserContext* original_context,
                               ExtensionRegistry* extension_registry)
    : site_instance_(SiteInstance::Create(context)),
      extension_registry_(extension_registry),
      startup_background_hosts_created_(false),
      devtools_callback_(base::Bind(&ProcessManager::OnDevToolsStateChanged,
                                    base::Unretained(this))),
      last_background_close_sequence_id_(0),
      weak_ptr_factory_(this) {
  // ExtensionRegistry is shared between incognito and regular contexts.
  DCHECK_EQ(original_context, extension_registry_->browser_context());
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
                 content::Source<BrowserContext>(original_context));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
                 content::Source<BrowserContext>(original_context));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::Source<BrowserContext>(original_context));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::Source<BrowserContext>(context));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<BrowserContext>(context));
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                 content::NotificationService::AllSources());

  // Note: event_page_idle_time_ must be sufficiently larger (e.g. 2x) than
  // kKeepaliveThrottleIntervalInSeconds in ppapi/proxy/plugin_globals.
  event_page_idle_time_ = base::TimeDelta::FromSeconds(10);
  unsigned idle_time_msec = 0;
  if (base::StringToUint(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          extensions::switches::kEventPageIdleTime), &idle_time_msec)) {
    CHECK_GT(idle_time_msec, 0u);  // OnKeepaliveImpulseCheck requires non zero.
    event_page_idle_time_ = base::TimeDelta::FromMilliseconds(idle_time_msec);
  }
  event_page_suspending_time_ = base::TimeDelta::FromSeconds(5);
  unsigned suspending_time_msec = 0;
  if (base::StringToUint(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                             extensions::switches::kEventPageSuspendingTime),
                         &suspending_time_msec)) {
    event_page_suspending_time_ =
        base::TimeDelta::FromMilliseconds(suspending_time_msec);
  }

  content::DevToolsAgentHost::AddAgentStateCallback(devtools_callback_);

  OnKeepaliveImpulseCheck();
}

ProcessManager::~ProcessManager() {
  CloseBackgroundHosts();
  DCHECK(background_hosts_.empty());
  content::DevToolsAgentHost::RemoveAgentStateCallback(devtools_callback_);
}

const ProcessManager::ViewSet ProcessManager::GetAllViews() const {
  ViewSet result;
  for (ExtensionRenderViews::const_iterator iter =
           all_extension_views_.begin();
       iter != all_extension_views_.end(); ++iter) {
    result.insert(iter->first);
  }
  return result;
}

void ProcessManager::AddObserver(ProcessManagerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ProcessManager::RemoveObserver(ProcessManagerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ProcessManager::CreateBackgroundHost(const Extension* extension,
                                          const GURL& url) {
  // Hosted apps are taken care of from BackgroundContentsService. Ignore them
  // here.
  if (extension->is_hosted_app())
    return false;

  // Don't create hosts if the embedder doesn't allow it.
  ProcessManagerDelegate* delegate =
      ExtensionsBrowserClient::Get()->GetProcessManagerDelegate();
  if (delegate && !delegate->IsBackgroundPageAllowed(GetBrowserContext()))
    return false;

  // Don't create multiple background hosts for an extension.
  if (GetBackgroundHostForExtension(extension->id()))
    return true;  // TODO(kalman): return false here? It might break things...

  ExtensionHost* host =
      new ExtensionHost(extension, GetSiteInstanceForURL(url), url,
                        VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
  host->CreateRenderViewSoon();
  OnBackgroundHostCreated(host);
  return true;
}

ExtensionHost* ProcessManager::GetBackgroundHostForExtension(
    const std::string& extension_id) {
  for (ExtensionHostSet::iterator iter = background_hosts_.begin();
       iter != background_hosts_.end(); ++iter) {
    ExtensionHost* host = *iter;
    if (host->extension_id() == extension_id)
      return host;
  }
  return NULL;
}

std::set<RenderViewHost*> ProcessManager::GetRenderViewHostsForExtension(
    const std::string& extension_id) {
  std::set<RenderViewHost*> result;

  SiteInstance* site_instance = GetSiteInstanceForURL(
      Extension::GetBaseURLFromExtensionId(extension_id));
  if (!site_instance)
    return result;

  // Gather up all the views for that site.
  for (ExtensionRenderViews::iterator view = all_extension_views_.begin();
       view != all_extension_views_.end(); ++view) {
    if (view->first->GetSiteInstance() == site_instance)
      result.insert(view->first);
  }

  return result;
}

const Extension* ProcessManager::GetExtensionForRenderViewHost(
    RenderViewHost* render_view_host) {
  if (!render_view_host->GetSiteInstance())
    return NULL;

  return extension_registry_->enabled_extensions().GetByID(
      GetExtensionID(render_view_host));
}

void ProcessManager::UnregisterRenderViewHost(
    RenderViewHost* render_view_host) {
  ExtensionRenderViews::iterator view =
      all_extension_views_.find(render_view_host);
  if (view == all_extension_views_.end())
    return;

  OnRenderViewHostUnregistered(GetBrowserContext(), render_view_host);
  ViewType view_type = view->second;
  all_extension_views_.erase(view);

  // Keepalive count, balanced in RegisterRenderViewHost.
  if (view_type != VIEW_TYPE_INVALID &&
      view_type != VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    const Extension* extension = GetExtensionForRenderViewHost(
        render_view_host);
    if (extension)
      DecrementLazyKeepaliveCount(extension);
  }
}

bool ProcessManager::RegisterRenderViewHost(RenderViewHost* render_view_host) {
  const Extension* extension = GetExtensionForRenderViewHost(
      render_view_host);
  if (!extension)
    return false;

  WebContents* web_contents = WebContents::FromRenderViewHost(render_view_host);
  all_extension_views_[render_view_host] = GetViewType(web_contents);

  // Keep the lazy background page alive as long as any non-background-page
  // extension views are visible. Keepalive count balanced in
  // UnregisterRenderViewHost.
  IncrementLazyKeepaliveCountForView(render_view_host);
  return true;
}

SiteInstance* ProcessManager::GetSiteInstanceForURL(const GURL& url) {
  return site_instance_->GetRelatedSiteInstance(url);
}

bool ProcessManager::IsBackgroundHostClosing(const std::string& extension_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  return (host && background_page_data_[extension_id].is_closing);
}

int ProcessManager::GetLazyKeepaliveCount(const Extension* extension) {
  if (!BackgroundInfo::HasLazyBackgroundPage(extension))
    return 0;

  return background_page_data_[extension->id()].lazy_keepalive_count;
}

void ProcessManager::IncrementLazyKeepaliveCount(const Extension* extension) {
  if (!BackgroundInfo::HasLazyBackgroundPage(extension))
    return;

  int& count = background_page_data_[extension->id()].lazy_keepalive_count;
  if (++count == 1)
    OnLazyBackgroundPageActive(extension->id());
}

void ProcessManager::DecrementLazyKeepaliveCount(const Extension* extension) {
  if (!BackgroundInfo::HasLazyBackgroundPage(extension))
    return;
  DecrementLazyKeepaliveCount(extension->id());
}

void ProcessManager::DecrementLazyKeepaliveCount(
    const std::string& extension_id) {
  int& count = background_page_data_[extension_id].lazy_keepalive_count;
  DCHECK(count > 0 ||
         !extension_registry_->enabled_extensions().Contains(extension_id));

  // If we reach a zero keepalive count when the lazy background page is about
  // to be closed, incrementing close_sequence_id will cancel the close
  // sequence and cause the background page to linger. So check is_closing
  // before initiating another close sequence.
  if (--count == 0 && !background_page_data_[extension_id].is_closing) {
    background_page_data_[extension_id].close_sequence_id =
        ++last_background_close_sequence_id_;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ProcessManager::OnLazyBackgroundPageIdle,
                   weak_ptr_factory_.GetWeakPtr(),
                   extension_id,
                   last_background_close_sequence_id_),
        event_page_idle_time_);
  }
}

void ProcessManager::IncrementLazyKeepaliveCountForView(
    RenderViewHost* render_view_host) {
  WebContents* web_contents =
      WebContents::FromRenderViewHost(render_view_host);
  ViewType view_type = GetViewType(web_contents);
  if (view_type != VIEW_TYPE_INVALID &&
      view_type != VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    const Extension* extension = GetExtensionForRenderViewHost(
        render_view_host);
    if (extension)
      IncrementLazyKeepaliveCount(extension);
  }
}

// This implementation layers on top of the keepalive count. An impulse sets
// a per extension flag. On a regular interval that flag is checked. Changes
// from the flag not being set to set cause an IncrementLazyKeepaliveCount.
void ProcessManager::KeepaliveImpulse(const Extension* extension) {
  if (!BackgroundInfo::HasLazyBackgroundPage(extension))
    return;

  BackgroundPageData& bd = background_page_data_[extension->id()];

  if (!bd.keepalive_impulse) {
    bd.keepalive_impulse = true;
    if (!bd.previous_keepalive_impulse) {
      IncrementLazyKeepaliveCount(extension);
    }
  }

  if (!keepalive_impulse_callback_for_testing_.is_null()) {
    ImpulseCallbackForTesting callback_may_clear_callbacks_reentrantly =
      keepalive_impulse_callback_for_testing_;
    callback_may_clear_callbacks_reentrantly.Run(extension->id());
  }
}

// DecrementLazyKeepaliveCount is called when no calls to KeepaliveImpulse
// have been made for at least event_page_idle_time_. In the best case an
// impulse was made just before being cleared, and the decrement will occur
// event_page_idle_time_ later, causing a 2 * event_page_idle_time_ total time
// for extension to be shut down based on impulses. Worst case is an impulse
// just after a clear, adding one check cycle and resulting in 3x total time.
void ProcessManager::OnKeepaliveImpulseCheck() {
  for (BackgroundPageDataMap::iterator i = background_page_data_.begin();
       i != background_page_data_.end();
       ++i) {
    if (i->second.previous_keepalive_impulse && !i->second.keepalive_impulse) {
      DecrementLazyKeepaliveCount(i->first);
      if (!keepalive_impulse_decrement_callback_for_testing_.is_null()) {
        ImpulseCallbackForTesting callback_may_clear_callbacks_reentrantly =
          keepalive_impulse_decrement_callback_for_testing_;
        callback_may_clear_callbacks_reentrantly.Run(i->first);
      }
    }

    i->second.previous_keepalive_impulse = i->second.keepalive_impulse;
    i->second.keepalive_impulse = false;
  }

  // OnKeepaliveImpulseCheck() is always called in constructor, but in unit
  // tests there will be no message loop. In that event don't schedule tasks.
  if (base::MessageLoop::current()) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ProcessManager::OnKeepaliveImpulseCheck,
                   weak_ptr_factory_.GetWeakPtr()),
        event_page_idle_time_);
  }
}

void ProcessManager::OnLazyBackgroundPageIdle(const std::string& extension_id,
                                              uint64 sequence_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  if (host && !background_page_data_[extension_id].is_closing &&
      sequence_id == background_page_data_[extension_id].close_sequence_id) {
    // Tell the renderer we are about to close. This is a simple ping that the
    // renderer will respond to. The purpose is to control sequencing: if the
    // extension remains idle until the renderer responds with an ACK, then we
    // know that the extension process is ready to shut down. If our
    // close_sequence_id has already changed, then we would ignore the
    // ShouldSuspendAck, so we don't send the ping.
    host->render_view_host()->Send(new ExtensionMsg_ShouldSuspend(
        extension_id, sequence_id));
  }
}

void ProcessManager::OnLazyBackgroundPageActive(
    const std::string& extension_id) {
  if (!background_page_data_[extension_id].is_closing) {
    // Cancel the current close sequence by changing the close_sequence_id,
    // which causes us to ignore the next ShouldSuspendAck.
    background_page_data_[extension_id].close_sequence_id =
        ++last_background_close_sequence_id_;
  }
}

void ProcessManager::OnShouldSuspendAck(const std::string& extension_id,
                                        uint64 sequence_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  if (host &&
      sequence_id == background_page_data_[extension_id].close_sequence_id) {
    host->render_view_host()->Send(new ExtensionMsg_Suspend(extension_id));
  }
}

void ProcessManager::OnSuspendAck(const std::string& extension_id) {
  background_page_data_[extension_id].is_closing = true;
  uint64 sequence_id = background_page_data_[extension_id].close_sequence_id;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ProcessManager::CloseLazyBackgroundPageNow,
                 weak_ptr_factory_.GetWeakPtr(), extension_id, sequence_id),
      event_page_suspending_time_);
}

void ProcessManager::CloseLazyBackgroundPageNow(const std::string& extension_id,
                                                uint64 sequence_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  if (host &&
      sequence_id == background_page_data_[extension_id].close_sequence_id) {
    ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
    if (host)
      CloseBackgroundHost(host);
  }
}

void ProcessManager::OnNetworkRequestStarted(
    content::RenderFrameHost* render_frame_host) {
  ExtensionHost* host = GetBackgroundHostForExtension(
      GetExtensionIDFromFrame(render_frame_host));
  if (host && IsFrameInExtensionHost(host, render_frame_host))
    IncrementLazyKeepaliveCount(host->extension());
}

void ProcessManager::OnNetworkRequestDone(
    content::RenderFrameHost* render_frame_host) {
  ExtensionHost* host = GetBackgroundHostForExtension(
      GetExtensionIDFromFrame(render_frame_host));
  if (host && IsFrameInExtensionHost(host, render_frame_host))
    DecrementLazyKeepaliveCount(host->extension());
}

void ProcessManager::CancelSuspend(const Extension* extension) {
  bool& is_closing = background_page_data_[extension->id()].is_closing;
  ExtensionHost* host = GetBackgroundHostForExtension(extension->id());
  if (host && is_closing) {
    is_closing = false;
    host->render_view_host()->Send(
        new ExtensionMsg_CancelSuspend(extension->id()));
    // This increment / decrement is to simulate an instantaneous event. This
    // has the effect of invalidating close_sequence_id, preventing any in
    // progress closes from completing and starting a new close process if
    // necessary.
    IncrementLazyKeepaliveCount(extension);
    DecrementLazyKeepaliveCount(extension);
  }
}

void ProcessManager::CloseBackgroundHosts() {
  for (ExtensionHostSet::iterator iter = background_hosts_.begin();
       iter != background_hosts_.end();) {
    ExtensionHostSet::iterator current = iter++;
    delete *current;
  }
}

content::BrowserContext* ProcessManager::GetBrowserContext() const {
  return site_instance_->GetBrowserContext();
}

void ProcessManager::SetKeepaliveImpulseCallbackForTesting(
    const ImpulseCallbackForTesting& callback) {
  keepalive_impulse_callback_for_testing_ = callback;
}

void ProcessManager::SetKeepaliveImpulseDecrementCallbackForTesting(
    const ImpulseCallbackForTesting& callback) {
  keepalive_impulse_decrement_callback_for_testing_ = callback;
}

void ProcessManager::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED: {
      // TODO(jamescook): Convert this to use ExtensionSystem::ready() instead
      // of a notification.
      MaybeCreateStartupBackgroundHosts();
      break;
    }

    case extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED: {
      BrowserContext* context = content::Source<BrowserContext>(source).ptr();
      ExtensionSystem* system = ExtensionSystem::Get(context);
      if (system->ready().is_signaled()) {
        // The extension system is ready, so create the background host.
        const Extension* extension =
            content::Details<const Extension>(details).ptr();
        CreateBackgroundHostForExtensionLoad(this, extension);
      }
      break;
    }

    case extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED: {
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      for (ExtensionHostSet::iterator iter = background_hosts_.begin();
           iter != background_hosts_.end(); ++iter) {
        ExtensionHost* host = *iter;
        if (host->extension_id() == extension->id()) {
          CloseBackgroundHost(host);
          break;
        }
      }
      UnregisterExtension(extension->id());
      break;
    }

    case extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
      if (background_hosts_.erase(host)) {
        ClearBackgroundPageData(host->extension()->id());
        background_page_data_[host->extension()->id()].since_suspended.reset(
            new base::ElapsedTimer());
      }
      break;
    }

    case extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE: {
      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
      if (host->extension_host_type() == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
        CloseBackgroundHost(host);
      }
      break;
    }

    case content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED: {
      // We get this notification both for new WebContents and when one
      // has its RenderViewHost replaced (e.g. when a user does a cross-site
      // navigation away from an extension URL). For the replaced case, we must
      // unregister the old RVH so it doesn't count as an active view that would
      // keep the event page alive.
      WebContents* contents = content::Source<WebContents>(source).ptr();
      if (contents->GetBrowserContext() != GetBrowserContext())
        break;

      typedef std::pair<RenderViewHost*, RenderViewHost*> RVHPair;
      RVHPair* switched_details = content::Details<RVHPair>(details).ptr();
      if (switched_details->first)
        UnregisterRenderViewHost(switched_details->first);

      // The above will unregister a RVH when it gets swapped out with a new
      // one. However we need to watch the WebContents to know when a RVH is
      // deleted because the WebContents has gone away.
      if (RegisterRenderViewHost(switched_details->second)) {
        RenderViewHostDestructionObserver::CreateForWebContents(contents);
      }
      break;
    }

    case content::NOTIFICATION_WEB_CONTENTS_CONNECTED: {
      WebContents* contents = content::Source<WebContents>(source).ptr();
      if (contents->GetBrowserContext() != GetBrowserContext())
        break;
      const Extension* extension = GetExtensionForRenderViewHost(
          contents->GetRenderViewHost());
      if (!extension)
        return;

      // RegisterRenderViewHost is called too early (before the process is
      // available), so we need to wait until now to notify.
      content::NotificationService::current()->Notify(
          extensions::NOTIFICATION_EXTENSION_VIEW_REGISTERED,
          content::Source<BrowserContext>(GetBrowserContext()),
          content::Details<RenderViewHost>(contents->GetRenderViewHost()));
      break;
    }

    default:
      NOTREACHED();
  }
}

void ProcessManager::OnDevToolsStateChanged(
    content::DevToolsAgentHost* agent_host,
    bool attached) {
  WebContents* web_contents = agent_host->GetWebContents();
  // Ignore unrelated notifications.
  if (!web_contents || web_contents->GetBrowserContext() != GetBrowserContext())
    return;
  if (GetViewType(web_contents) != VIEW_TYPE_EXTENSION_BACKGROUND_PAGE)
    return;
  const Extension* extension =
      GetExtensionForRenderViewHost(web_contents->GetRenderViewHost());
  if (!extension)
    return;
  if (attached) {
    // Keep the lazy background page alive while it's being inspected.
    CancelSuspend(extension);
    IncrementLazyKeepaliveCount(extension);
  } else {
    DecrementLazyKeepaliveCount(extension);
  }
}

void ProcessManager::MaybeCreateStartupBackgroundHosts() {
  if (startup_background_hosts_created_)
    return;

  // The embedder might disallow background pages entirely.
  ProcessManagerDelegate* delegate =
      ExtensionsBrowserClient::Get()->GetProcessManagerDelegate();
  if (delegate && !delegate->IsBackgroundPageAllowed(GetBrowserContext()))
    return;

  // The embedder might want to defer background page loading. For example,
  // Chrome defers background page loading when it is launched to show the app
  // list, then triggers a load later when a browser window opens.
  if (delegate &&
      delegate->DeferCreatingStartupBackgroundHosts(GetBrowserContext()))
    return;

  CreateStartupBackgroundHosts();
  startup_background_hosts_created_ = true;

  // Background pages should only be loaded once. To prevent any further loads
  // occurring, we remove the notification listeners.
  BrowserContext* original_context =
      ExtensionsBrowserClient::Get()->GetOriginalContext(GetBrowserContext());
  if (registrar_.IsRegistered(
          this,
          extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
          content::Source<BrowserContext>(original_context))) {
    registrar_.Remove(this,
                      extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
                      content::Source<BrowserContext>(original_context));
  }
}

void ProcessManager::CreateStartupBackgroundHosts() {
  DCHECK(!startup_background_hosts_created_);
  const ExtensionSet& enabled_extensions =
      extension_registry_->enabled_extensions();
  for (ExtensionSet::const_iterator extension = enabled_extensions.begin();
       extension != enabled_extensions.end();
       ++extension) {
    CreateBackgroundHostForExtensionLoad(this, extension->get());

    FOR_EACH_OBSERVER(ProcessManagerObserver,
                      observer_list_,
                      OnBackgroundHostStartup(*extension));
  }
}

void ProcessManager::OnBackgroundHostCreated(ExtensionHost* host) {
  DCHECK_EQ(GetBrowserContext(), host->browser_context());
  background_hosts_.insert(host);

  if (BackgroundInfo::HasLazyBackgroundPage(host->extension())) {
    linked_ptr<base::ElapsedTimer> since_suspended(
        background_page_data_[host->extension()->id()].
            since_suspended.release());
    if (since_suspended.get()) {
      UMA_HISTOGRAM_LONG_TIMES("Extensions.EventPageIdleTime",
                               since_suspended->Elapsed());
    }
  }
}

void ProcessManager::CloseBackgroundHost(ExtensionHost* host) {
  CHECK(host->extension_host_type() ==
        VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
  delete host;
  // |host| should deregister itself from our structures.
  CHECK(background_hosts_.find(host) == background_hosts_.end());
}

void ProcessManager::UnregisterExtension(const std::string& extension_id) {
  // The lazy_keepalive_count may be greater than zero at this point because
  // RenderViewHosts are still alive. During extension reloading, they will
  // decrement the lazy_keepalive_count to negative for the new extension
  // instance when they are destroyed. Since we are erasing the background page
  // data for the unloaded extension, unregister the RenderViewHosts too.
  BrowserContext* context = GetBrowserContext();
  for (ExtensionRenderViews::iterator it = all_extension_views_.begin();
       it != all_extension_views_.end(); ) {
    if (GetExtensionID(it->first) == extension_id) {
      OnRenderViewHostUnregistered(context, it->first);
      all_extension_views_.erase(it++);
    } else {
      ++it;
    }
  }

  background_page_data_.erase(extension_id);
}

void ProcessManager::ClearBackgroundPageData(const std::string& extension_id) {
  background_page_data_.erase(extension_id);

  // Re-register all RenderViews for this extension. We do this to restore
  // the lazy_keepalive_count (if any) to properly reflect the number of open
  // views.
  for (ExtensionRenderViews::const_iterator it = all_extension_views_.begin();
       it != all_extension_views_.end(); ++it) {
    if (GetExtensionID(it->first) == extension_id)
      IncrementLazyKeepaliveCountForView(it->first);
  }
}

//
// IncognitoProcessManager
//

IncognitoProcessManager::IncognitoProcessManager(
    BrowserContext* incognito_context,
    BrowserContext* original_context,
    ProcessManager* original_manager,
    ExtensionRegistry* extension_registry)
    : ProcessManager(incognito_context, original_context, extension_registry),
      original_manager_(original_manager) {
  DCHECK(incognito_context->IsOffTheRecord());

  // The original profile will have its own ProcessManager to
  // load the background pages of the spanning extensions. This process
  // manager need only worry about the split mode extensions, which is handled
  // in the NOTIFICATION_BROWSER_WINDOW_READY notification handler.
  registrar_.Remove(this,
                    extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
                    content::Source<BrowserContext>(original_context));
}

bool IncognitoProcessManager::CreateBackgroundHost(const Extension* extension,
                                                   const GURL& url) {
  if (IncognitoInfo::IsSplitMode(extension)) {
    if (ExtensionsBrowserClient::Get()->IsExtensionIncognitoEnabled(
            extension->id(), GetBrowserContext()))
      return ProcessManager::CreateBackgroundHost(extension, url);
  } else {
    // Do nothing. If an extension is spanning, then its original-profile
    // background page is shared with incognito, so we don't create another.
  }
  return false;
}

SiteInstance* IncognitoProcessManager::GetSiteInstanceForURL(const GURL& url) {
  const Extension* extension =
      extension_registry_->enabled_extensions().GetExtensionOrAppByURL(url);
  if (extension && !IncognitoInfo::IsSplitMode(extension))
    return original_manager_->GetSiteInstanceForURL(url);

  return ProcessManager::GetSiteInstanceForURL(url);
}

}  // namespace extensions
