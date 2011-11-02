// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_listener.h"

#include "base/bind.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/url_pattern.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

struct UserScriptListener::ProfileData {
  // True if the user scripts contained in |url_patterns| are ready for
  // injection.
  bool user_scripts_ready;

  // A list of URL patterns that have will have user scripts applied to them.
  URLPatterns url_patterns;

  ProfileData() : user_scripts_ready(false) {}
};

UserScriptListener::UserScriptListener()
    : resource_queue_(NULL),
      user_scripts_ready_(false) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_USER_SCRIPTS_UPDATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllSources());
  AddRef();  // Will be balanced in Cleanup().
}

void UserScriptListener::Initialize(ResourceQueue* resource_queue) {
  resource_queue_ = resource_queue;
}

bool UserScriptListener::ShouldDelayRequest(
    net::URLRequest* request,
    const ResourceDispatcherHostRequestInfo& request_info,
    const GlobalRequestID& request_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // If it's a frame load, then we need to check the URL against the list of
  // user scripts to see if we need to wait.
  if (request_info.resource_type() != ResourceType::MAIN_FRAME &&
      request_info.resource_type() != ResourceType::SUB_FRAME) {
    return false;
  }

  // Note: we could delay only requests made by the profile who is causing the
  // delay, but it's a little more complicated to associate requests with the
  // right profile. Since this is a rare case, we'll just take the easy way
  // out.
  if (user_scripts_ready_)
    return false;

  for (ProfileDataMap::const_iterator pt = profile_data_.begin();
       pt != profile_data_.end(); ++pt) {
    for (URLPatterns::const_iterator it = pt->second.url_patterns.begin();
         it != pt->second.url_patterns.end(); ++it) {
      if ((*it).MatchesURL(request->url())) {
        // One of the user scripts wants to inject into this request, but the
        // script isn't ready yet. Delay the request.
        return true;
      }
    }
  }

  return false;
}

void UserScriptListener::WillShutdownResourceQueue() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  resource_queue_ = NULL;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&UserScriptListener::Cleanup, this));
}

UserScriptListener::~UserScriptListener() {
}

void UserScriptListener::CheckIfAllUserScriptsReady() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool was_ready = user_scripts_ready_;

  user_scripts_ready_ = true;
  for (ProfileDataMap::const_iterator it = profile_data_.begin();
       it != profile_data_.end(); ++it) {
    if (!it->second.user_scripts_ready)
      user_scripts_ready_ = false;
  }

  if (user_scripts_ready_ && !was_ready) {
    if (resource_queue_)
      resource_queue_->StartDelayedRequests(this);
  }
}

void UserScriptListener::UserScriptsReady(void* profile_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  profile_data_[profile_id].user_scripts_ready = true;
  CheckIfAllUserScriptsReady();
}

void UserScriptListener::ProfileDestroyed(void* profile_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  profile_data_.erase(profile_id);

  // We may have deleted the only profile we were waiting on.
  CheckIfAllUserScriptsReady();
}

void UserScriptListener::AppendNewURLPatterns(void* profile_id,
                                              const URLPatterns& new_patterns) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  user_scripts_ready_ = false;

  ProfileData& data = profile_data_[profile_id];
  data.user_scripts_ready = false;

  data.url_patterns.insert(data.url_patterns.end(),
                           new_patterns.begin(), new_patterns.end());
}

void UserScriptListener::ReplaceURLPatterns(void* profile_id,
                                            const URLPatterns& patterns) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ProfileData& data = profile_data_[profile_id];
  data.url_patterns = patterns;
}

void UserScriptListener::Cleanup() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.RemoveAll();
  Release();
}

void UserScriptListener::CollectURLPatterns(const Extension* extension,
                                            URLPatterns* patterns) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const UserScriptList& scripts = extension->content_scripts();
  for (UserScriptList::const_iterator iter = scripts.begin();
       iter != scripts.end(); ++iter) {
    patterns->insert(patterns->end(),
                     (*iter).url_patterns().begin(),
                     (*iter).url_patterns().end());
  }
}

void UserScriptListener::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (extension->content_scripts().empty())
        return;  // no new patterns from this extension.

      URLPatterns new_patterns;
      CollectURLPatterns(extension, &new_patterns);
      if (!new_patterns.empty()) {
        BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
            &UserScriptListener::AppendNewURLPatterns, this,
            profile, new_patterns));
      }
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      const Extension* unloaded_extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      if (unloaded_extension->content_scripts().empty())
        return;  // no patterns to delete for this extension.

      // Clear all our patterns and reregister all the still-loaded extensions.
      URLPatterns new_patterns;
      ExtensionService* service = profile->GetExtensionService();
      for (ExtensionList::const_iterator it = service->extensions()->begin();
           it != service->extensions()->end(); ++it) {
        if (*it != unloaded_extension)
          CollectURLPatterns(*it, &new_patterns);
      }
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
          &UserScriptListener::ReplaceURLPatterns, this,
          profile, new_patterns));
      break;
    }

    case chrome::NOTIFICATION_USER_SCRIPTS_UPDATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
          &UserScriptListener::UserScriptsReady, this, profile));
      break;
    }

    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
          &UserScriptListener::ProfileDestroyed, this, profile));
      break;
    }

    default:
      NOTREACHED();
  }
}
