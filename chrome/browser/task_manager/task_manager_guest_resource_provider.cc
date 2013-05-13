// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_guest_resource_provider.h"

#include <string>

#include "base/basictypes.h"
#include "base/i18n/rtl.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::RenderProcessHost;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;
using extensions::Extension;

namespace {

string16 GetProfileNameFromInfoCache(Profile* profile) {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(
      profile->GetOriginalProfile()->GetPath());
  if (index == std::string::npos)
    return string16();
  else
    return cache.GetNameOfProfileAtIndex(index);
}

string16 GetTitleFromWebContents(WebContents* web_contents) {
  string16 title = web_contents->GetTitle();
  if (title.empty()) {
    GURL url = web_contents->GetURL();
    title = UTF8ToUTF16(url.spec());
    // Force URL to be LTR.
    title = base::i18n::GetDisplayStringInLTRDirectionality(title);
  } else {
    // Since the tab_title will be concatenated with
    // IDS_TASK_MANAGER_TAB_PREFIX, we need to explicitly set the tab_title to
    // be LTR format if there is no strong RTL charater in it. Otherwise, if
    // IDS_TASK_MANAGER_TAB_PREFIX is an RTL word, the concatenated result
    // might be wrong. For example, http://mail.yahoo.com, whose title is
    // "Yahoo! Mail: The best web-based Email!", without setting it explicitly
    // as LTR format, the concatenated result will be "!Yahoo! Mail: The best
    // web-based Email :BAT", in which the capital letters "BAT" stands for
    // the Hebrew word for "tab".
    base::i18n::AdjustStringForLocaleDirection(&title);
  }
  return title;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TaskManagerGuestResource class
////////////////////////////////////////////////////////////////////////////////

TaskManagerGuestResource::TaskManagerGuestResource(
    RenderViewHost* render_view_host)
    : TaskManagerRendererResource(
          render_view_host->GetSiteInstance()->GetProcess()->GetHandle(),
          render_view_host) {
}

TaskManagerGuestResource::~TaskManagerGuestResource() {
}

TaskManager::Resource::Type TaskManagerGuestResource::GetType() const {
  return GUEST;
}

string16 TaskManagerGuestResource::GetTitle() const {
  WebContents* web_contents = GetWebContents();
  const int message_id = IDS_TASK_MANAGER_WEBVIEW_TAG_PREFIX;
  if (web_contents) {
    string16 title = GetTitleFromWebContents(web_contents);
    return l10n_util::GetStringFUTF16(message_id, title);
  }
  return l10n_util::GetStringFUTF16(message_id, string16());
}

string16 TaskManagerGuestResource::GetProfileName() const {
  WebContents* web_contents = GetWebContents();
  if (web_contents) {
    Profile* profile = Profile::FromBrowserContext(
        web_contents->GetBrowserContext());
    return GetProfileNameFromInfoCache(profile);
  }
  return string16();
}

gfx::ImageSkia TaskManagerGuestResource::GetIcon() const {
  WebContents* web_contents = GetWebContents();
  if (web_contents && FaviconTabHelper::FromWebContents(web_contents)) {
    return FaviconTabHelper::FromWebContents(web_contents)->
        GetFavicon().AsImageSkia();
  }
  return gfx::ImageSkia();
}

WebContents* TaskManagerGuestResource::GetWebContents() const {
  return WebContents::FromRenderViewHost(render_view_host());
}

const Extension* TaskManagerGuestResource::GetExtension() const {
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerGuestContentsResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerGuestResourceProvider::
    TaskManagerGuestResourceProvider(TaskManager* task_manager)
    :  updating_(false),
       task_manager_(task_manager) {
}

TaskManagerGuestResourceProvider::~TaskManagerGuestResourceProvider() {
}

TaskManager::Resource* TaskManagerGuestResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  // If an origin PID was specified then the request originated in a plugin
  // working on the WebContents's behalf, so ignore it.
  if (origin_pid)
    return NULL;

  for (GuestResourceMap::iterator i = resources_.begin();
       i != resources_.end(); ++i) {
    WebContents* contents = WebContents::FromRenderViewHost(i->first);
    if (contents &&
        contents->GetRenderProcessHost()->GetID() == render_process_host_id &&
        contents->GetRenderViewHost()->GetRoutingID() == routing_id) {
      return i->second;
    }
  }

  return NULL;
}

void TaskManagerGuestResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // Add all the existing guest WebContents.
  for (RenderProcessHost::iterator i(
           RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost::RenderWidgetHostsIterator iter =
        i.GetCurrentValue()->GetRenderWidgetHostsIterator();
    for (; !iter.IsAtEnd(); iter.Advance()) {
      const RenderWidgetHost* widget = iter.GetCurrentValue();
      if (widget->IsRenderView()) {
        RenderViewHost* rvh =
            RenderViewHost::From(const_cast<RenderWidgetHost*>(widget));
        if (rvh->IsSubframe())
          Add(rvh);
      }
    }
  }

  // Then we register for notifications to get new guests.
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
}

void TaskManagerGuestResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Unregister for notifications.
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());

  resources_.clear();
}

void TaskManagerGuestResourceProvider::Add(
    RenderViewHost* render_view_host) {
  TaskManagerGuestResource* resource =
      new TaskManagerGuestResource(render_view_host);
  resources_[render_view_host] = resource;
  task_manager_->AddResource(resource);
}

void TaskManagerGuestResourceProvider::Remove(
    RenderViewHost* render_view_host) {
  if (!updating_)
    return;

  GuestResourceMap::iterator iter = resources_.find(render_view_host);
  if (iter == resources_.end())
    return;

  TaskManagerGuestResource* resource = iter->second;
  task_manager_->RemoveResource(resource);
  resources_.erase(iter);
  delete resource;
}

void TaskManagerGuestResourceProvider::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  WebContents* web_contents = content::Source<WebContents>(source).ptr();
  if (!web_contents || !web_contents->GetRenderViewHost()->IsSubframe())
    return;

  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_CONNECTED:
      Add(web_contents->GetRenderViewHost());
      break;
    case content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED:
      Remove(web_contents->GetRenderViewHost());
      break;
    default:
      NOTREACHED() << "Unexpected notification.";
  }
}
