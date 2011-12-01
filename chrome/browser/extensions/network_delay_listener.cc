// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/network_delay_listener.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_view_type.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

NetworkDelayListener::NetworkDelayListener()
    : resource_queue_(NULL),
      extensions_ready_(true),
      recorded_startup_delay_(false) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                 content::NotificationService::AllSources());
  AddRef();  // Will be balanced in Cleanup().
}

void NetworkDelayListener::Initialize(ResourceQueue* resource_queue) {
  resource_queue_ = resource_queue;
}

bool NetworkDelayListener::ShouldDelayRequest(
    net::URLRequest* request,
    const ResourceDispatcherHostRequestInfo& request_info,
    const GlobalRequestID& request_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Don't block internal URLs.
  if (request->url().SchemeIs(chrome::kChromeUIScheme) ||
      request->url().SchemeIs(chrome::kExtensionScheme) ||
      request->url().SchemeIs(chrome::kChromeDevToolsScheme)) {
    return false;
  }

  return !extensions_ready_;
}

NetworkDelayListener::~NetworkDelayListener() {
}

void NetworkDelayListener::WillShutdownResourceQueue() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  resource_queue_ = NULL;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NetworkDelayListener::Cleanup, this));
}

void NetworkDelayListener::OnExtensionPending(const std::string& id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DCHECK(pending_extensions_.count(id) == 0);
  extensions_ready_ = false;
  pending_extensions_.insert(id);
  delay_start_times_[id] = base::TimeTicks::Now();
}

void NetworkDelayListener::OnExtensionReady(const std::string& id) {
  // This may be called multiple times, if an extension finishes loading and is
  // then disabled or uninstalled.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Quick escape to save work in the common case.
  if (pending_extensions_.count(id) == 0)
    return;

  UMA_HISTOGRAM_TIMES("Extensions.StartupDelay",
      base::TimeTicks::Now() - delay_start_times_[id]);
  delay_start_times_.erase(id);
  pending_extensions_.erase(id);

  StartDelayedRequestsIfReady();
}

void NetworkDelayListener::StartDelayedRequestsIfReady() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (pending_extensions_.empty()) {
    extensions_ready_ = true;
    if (!recorded_startup_delay_) {
      UMA_HISTOGRAM_TIMES("Extensions.StartupDelay_Total",
          overall_start_time_.Elapsed());
      recorded_startup_delay_ = true;
    }
    if (resource_queue_)
        resource_queue_->StartDelayedRequests(this);
  }
}

void NetworkDelayListener::Cleanup() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.RemoveAll();
  Release();
}

void NetworkDelayListener::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      ExtensionService* service =
          content::Source<Profile>(source).ptr()->GetExtensionService();
      // We only wait for background pages to load. If the extension has no
      // background page, ignore it.
      if (service->extension_prefs()->DelaysNetworkRequests(extension->id()) &&
          !extension->background_url().is_empty()) {
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(&NetworkDelayListener::OnExtensionPending,
                       this, extension->id()));
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      ExtensionService* service =
          content::Source<Profile>(source).ptr()->GetExtensionService();
      if (service->extension_prefs()->DelaysNetworkRequests(extension->id())) {
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(&NetworkDelayListener::OnExtensionReady,
                       this, extension->id()));
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING: {
      const ExtensionHost* eh = content::Details<ExtensionHost>(details).ptr();
      if (eh->extension_host_type() !=
          chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE)
        return;

      const Extension* extension = eh->extension();
      ExtensionService* service =
          content::Source<Profile>(source).ptr()->GetExtensionService();
      if (service->extension_prefs()->DelaysNetworkRequests(extension->id())) {
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(&NetworkDelayListener::OnExtensionReady,
                       this, extension->id()));
      }
      break;
    }

    default:
      NOTREACHED();
  }
}
