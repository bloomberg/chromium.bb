// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/merge_session_throttle.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/signin/merge_session_load_page.h"
#include "chrome/browser/chromeos/login/signin/merge_session_xhr_request_waiter.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager_factory.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceType;
using content::WebContents;

namespace {

const int64 kMaxSessionRestoreTimeInSec = 60;

// The set of blocked profiles.
class ProfileSet : public base::NonThreadSafe,
                   public std::set<Profile*> {
 public:
  ProfileSet() {
  }

  virtual ~ProfileSet() {
  }

  static ProfileSet* Get();

 private:
  friend struct ::base::DefaultLazyInstanceTraits<ProfileSet>;

  DISALLOW_COPY_AND_ASSIGN(ProfileSet);
};

// Set of all of profiles for which restore session is in progress.
// This static member is accessible only form UI thread.
static base::LazyInstance<ProfileSet> g_blocked_profiles =
    LAZY_INSTANCE_INITIALIZER;

ProfileSet* ProfileSet::Get() {
  return g_blocked_profiles.Pointer();
}

}  // namespace

base::AtomicRefCount MergeSessionThrottle::all_profiles_restored_(0);

MergeSessionThrottle::MergeSessionThrottle(net::URLRequest* request,
                                           ResourceType resource_type)
    : request_(request),
      resource_type_(resource_type) {
}

MergeSessionThrottle::~MergeSessionThrottle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void MergeSessionThrottle::WillStartRequest(bool* defer) {
  if (!ShouldDelayUrl(request_->url()))
    return;

  DVLOG(1) << "WillStartRequest: defer " << request_->url();
  const content::ResourceRequestInfo* info =
    content::ResourceRequestInfo::ForRequest(request_);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &MergeSessionThrottle::DeleayResourceLoadingOnUIThread,
          resource_type_,
          info->GetChildID(),
          info->GetRouteID(),
          request_->url(),
          base::Bind(
              &MergeSessionThrottle::OnBlockingPageComplete,
              AsWeakPtr())));
  *defer = true;
}

const char* MergeSessionThrottle::GetNameForLogging() const {
  return "MergeSessionThrottle";
}

// static.
bool MergeSessionThrottle::AreAllSessionMergedAlready() {
  return !base::AtomicRefCountIsZero(&all_profiles_restored_);
}

void MergeSessionThrottle::OnBlockingPageComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  controller()->Resume();
}

bool MergeSessionThrottle::ShouldDelayUrl(const GURL& url) const {
  // If we are loading google properties while merge session is in progress,
  // we will show delayed loading page instead.
  return !net::NetworkChangeNotifier::IsOffline() &&
         !AreAllSessionMergedAlready() &&
         google_util::IsGoogleHostname(url.host(),
                                       google_util::ALLOW_SUBDOMAIN);
}

// static
void MergeSessionThrottle::BlockProfile(Profile* profile) {
  // Add a new profile to the list of those that we are currently blocking
  // blocking page loading for.
  if (ProfileSet::Get()->find(profile) == ProfileSet::Get()->end()) {
    DVLOG(1) << "Blocking profile " << profile;
    ProfileSet::Get()->insert(profile);

    // Since a new profile just got blocked, we can not assume that
    // all sessions are merged anymore.
    if (AreAllSessionMergedAlready()) {
      base::AtomicRefCountDec(&all_profiles_restored_);
      DVLOG(1) << "Marking all sessions unmerged!";
    }
  }
}

// static
void MergeSessionThrottle::UnblockProfile(Profile* profile) {
  // Have we blocked loading of pages for this this profile
  // before?
  DVLOG(1) << "Unblocking profile " << profile;
  ProfileSet::Get()->erase(profile);

  // Check if there is any other profile to block on.
  if (ProfileSet::Get()->size() == 0) {
    base::AtomicRefCountInc(&all_profiles_restored_);
    DVLOG(1) << "All profiles merged " << all_profiles_restored_;
  }
}

// static
bool MergeSessionThrottle::ShouldDelayRequest(
    int render_process_id,
    int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!user_manager::UserManager::Get()->IsUserLoggedIn()) {
    return false;
  } else if (!user_manager::UserManager::Get()->IsLoggedInAsRegularUser()) {
    // This is not a regular user session, let's remove the throttle
    // permanently.
    if (!AreAllSessionMergedAlready())
      base::AtomicRefCountInc(&all_profiles_restored_);

    return false;
  }

  RenderViewHost* render_view_host =
      RenderViewHost::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return false;

  WebContents* web_contents =
      WebContents::FromRenderViewHost(render_view_host);
  if (!web_contents)
    return false;

  content::BrowserContext* browser_context =
      web_contents->GetBrowserContext();
  if (!browser_context)
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;

  chromeos::OAuth2LoginManager* login_manager =
      chromeos::OAuth2LoginManagerFactory::GetInstance()->GetForProfile(
          profile);
  if (!login_manager)
    return false;

  switch (login_manager->state()) {
    case chromeos::OAuth2LoginManager::SESSION_RESTORE_NOT_STARTED:
      // The session restore for this profile hasn't even started yet. Don't
      // block for now.
      // In theory this should not happen since we should
      // kick off the session restore process for the newly added profile
      // before we attempt loading any page.
      if (user_manager::UserManager::Get()->IsLoggedInAsRegularUser() &&
          !user_manager::UserManager::Get()->IsLoggedInAsStub()) {
        LOG(WARNING) << "Loading content for a profile without "
                     << "session restore?";
      }
      return false;
    case chromeos::OAuth2LoginManager::SESSION_RESTORE_PREPARING:
    case chromeos::OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS: {
      // Check if the session restore has been going on for a while already.
      // If so, don't attempt to block page loading.
      if ((base::Time::Now() -
           login_manager->session_restore_start()).InSeconds() >
               kMaxSessionRestoreTimeInSec) {
        UnblockProfile(profile);
        return false;
      }

      // Add a new profile to the list of those that we are currently blocking
      // blocking page loading for.
      BlockProfile(profile);
      return true;
    }
    case chromeos::OAuth2LoginManager::SESSION_RESTORE_DONE:
    case chromeos::OAuth2LoginManager::SESSION_RESTORE_FAILED:
    case chromeos::OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED: {
      UnblockProfile(profile);
      return false;
    }
  }

  NOTREACHED();
  return false;
}

// static.
void MergeSessionThrottle::DeleayResourceLoadingOnUIThread(
    ResourceType resource_type,
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const CompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (ShouldDelayRequest(render_process_id, render_view_id)) {
    // There is a chance that the tab closed after we decided to show
    // the offline page on the IO thread and before we actually show the
    // offline page here on the UI thread.
    RenderViewHost* render_view_host =
        RenderViewHost::FromID(render_process_id, render_view_id);
    WebContents* web_contents = render_view_host ?
        WebContents::FromRenderViewHost(render_view_host) : NULL;
    if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
      DVLOG(1) << "Creating page waiter for " << url.spec();
      (new chromeos::MergeSessionLoadPage(web_contents, url, callback))->Show();
    } else {
      DVLOG(1) << "Creating XHR waiter for " << url.spec();
      DCHECK(resource_type == content::RESOURCE_TYPE_XHR);
      Profile* profile = Profile::FromBrowserContext(
          web_contents->GetBrowserContext());
      (new chromeos::MergeSessionXHRRequestWaiter(profile,
                                                  callback))->StartWaiting();
    }
  } else {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, callback);
  }
}
