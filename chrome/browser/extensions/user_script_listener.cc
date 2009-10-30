// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_listener.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "net/url_request/url_request.h"

UserScriptListener::UserScriptListener(ResourceDispatcherHost* rdh)
    : resource_dispatcher_host_(rdh),
      user_scripts_ready_(false) {
  DCHECK(resource_dispatcher_host_);

  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::USER_SCRIPTS_UPDATED,
                 NotificationService::AllSources());
}

bool UserScriptListener::ShouldStartRequest(URLRequest* request) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // If it's a frame load, then we need to check the URL against the list of
  // user scripts to see if we need to wait.
  ResourceDispatcherHostRequestInfo* info =
    ResourceDispatcherHost::InfoForRequest(request);
  DCHECK(info);

  if (info->resource_type() != ResourceType::MAIN_FRAME &&
      info->resource_type() != ResourceType::SUB_FRAME) {
    return true;
  }

  if (user_scripts_ready_)
    return true;

  // User scripts aren't ready yet. If one of them wants to inject into this
  // request, we'll need to wait for it before we can start this request.
  bool found_match = false;
  for (URLPatterns::iterator it = url_patterns_.begin();
       it != url_patterns_.end(); ++it) {
    if ((*it).MatchesUrl(request->url())) {
      found_match = true;
      break;
    }
  }

  if (!found_match)
    return true;

  // Queue this request up.
  delayed_request_ids_.push_front(ResourceDispatcherHost::GlobalRequestID(
      info->child_id(), info->request_id()));
  return false;
}

void UserScriptListener::StartDelayedRequests() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  user_scripts_ready_ = true;

  if (resource_dispatcher_host_) {
    for (DelayedRequests::iterator it = delayed_request_ids_.begin();
         it != delayed_request_ids_.end(); ++it) {
      URLRequest* request = resource_dispatcher_host_->GetURLRequest(*it);
      if (request) {
        // The request shouldn't have started (SUCCESS is the initial state).
        DCHECK(request->status().status() == URLRequestStatus::SUCCESS);
        request->Start();
      }
    }
  }

  delayed_request_ids_.clear();
}

void UserScriptListener::AppendNewURLPatterns(const URLPatterns& new_patterns) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  user_scripts_ready_ = false;
  url_patterns_.insert(url_patterns_.end(),
                       new_patterns.begin(), new_patterns.end());
}

void UserScriptListener::ReplaceURLPatterns(const URLPatterns& patterns) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  url_patterns_ = patterns;
}

void UserScriptListener::CollectURLPatterns(Extension* extension,
                                            URLPatterns* patterns) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  const UserScriptList& scripts = extension->content_scripts();
  for (UserScriptList::const_iterator iter = scripts.begin();
       iter != scripts.end(); ++iter) {
    patterns->insert(patterns->end(),
                     (*iter).url_patterns().begin(),
                     (*iter).url_patterns().end());
  }
}

void UserScriptListener::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  switch (type.value) {
    case NotificationType::EXTENSION_LOADED: {
      Extension* extension = Details<Extension>(details).ptr();
      if (extension->content_scripts().empty())
        return;  // no new patterns from this extension.

      URLPatterns new_patterns;
      CollectURLPatterns(Details<Extension>(details).ptr(), &new_patterns);
      if (!new_patterns.empty()) {
        ChromeThread::PostTask(
            ChromeThread::IO, FROM_HERE,
            NewRunnableMethod(
                this, &UserScriptListener::AppendNewURLPatterns, new_patterns));
      }
      break;
    }

    case NotificationType::EXTENSION_UNLOADED: {
      Extension* unloaded_extension = Details<Extension>(details).ptr();
      if (unloaded_extension->content_scripts().empty())
        return;  // no patterns to delete for this extension.

      // Clear all our patterns and reregister all the still-loaded extensions.
      URLPatterns new_patterns;
      ExtensionsService* service = Source<ExtensionsService>(source).ptr();
      for (ExtensionList::const_iterator it = service->extensions()->begin();
           it != service->extensions()->end(); ++it) {
        if (*it != unloaded_extension)
          CollectURLPatterns(*it, &new_patterns);
      }
      ChromeThread::PostTask(
          ChromeThread::IO, FROM_HERE,
          NewRunnableMethod(
              this, &UserScriptListener::ReplaceURLPatterns, new_patterns));
      break;
    }

    case NotificationType::USER_SCRIPTS_UPDATED: {
      ChromeThread::PostTask(
          ChromeThread::IO, FROM_HERE,
          NewRunnableMethod(this, &UserScriptListener::StartDelayedRequests));
      break;
    }

    default:
      NOTREACHED();
  }
}
