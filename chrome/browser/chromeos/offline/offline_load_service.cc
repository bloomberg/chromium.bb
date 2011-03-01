// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/offline/offline_load_service.h"

#include "base/lazy_instance.h"
#include "base/ref_counted.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace chromeos {

// A utility class that serves a singleton instance of OfflineLoadService.
// OfflineLoadSerivce itself cannot be a singleton as it implements
// RefCount interface.
class OfflineLoadServiceSingleton {
 public:
  chromeos::OfflineLoadService* offline_load_service() {
    return offline_load_service_.get();
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<OfflineLoadServiceSingleton>;
  OfflineLoadServiceSingleton()
      : offline_load_service_(new chromeos::OfflineLoadService()) {}
  virtual ~OfflineLoadServiceSingleton() {}

  scoped_refptr<chromeos::OfflineLoadService> offline_load_service_;

  DISALLOW_COPY_AND_ASSIGN(OfflineLoadServiceSingleton);
};

static base::LazyInstance<OfflineLoadServiceSingleton>
    g_offline_load_service_singleton(base::LINKER_INITIALIZED);

// static
OfflineLoadService* OfflineLoadService::Get() {
  return g_offline_load_service_singleton.Get().offline_load_service();
}

void OfflineLoadService::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (type.value == NotificationType::TAB_CLOSED) {
    registrar_.Remove(this, NotificationType::TAB_CLOSED, source);
    NavigationController* tab = Source<NavigationController>(source).ptr();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &OfflineLoadService::RemoveTabContents,
                          tab->tab_contents()));
  }
}

bool OfflineLoadService::ShouldProceed(int process_host_id,
                                       int render_view_id,
                                       const GURL& url) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  TabContents* tab_contents = tab_util::GetTabContentsByID(
      process_host_id, render_view_id);
  DCHECK(tab_contents);
  bool proceed = tabs_.find(tab_contents) != tabs_.end();
  DVLOG(1) << "ShouldProceed:" << proceed << ", url=" << url.spec()
           << ", tab_contents=" << tab_contents;
  return proceed;
}

void OfflineLoadService::Proceeded(int process_host_id,
                                   int render_view_id,
                                   const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  TabContents* tab_contents = tab_util::GetTabContentsByID(
      process_host_id, render_view_id);
  DCHECK(tab_contents);
  if (tabs_.find(tab_contents) == tabs_.end()) {
    DVLOG(1) << "Proceeded: url=" << url.spec()
             << ", tab_contents=" << tab_contents;
    tabs_.insert(tab_contents);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &OfflineLoadService::RegisterNotification,
                          &tab_contents->controller()));
  } else {
    DLOG(WARNING) << "Proceeded: ignoring duplicate";
  }
}

void OfflineLoadService::RemoveTabContents(TabContents* tab_contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  tabs_.erase(tabs_.find(tab_contents));
}

void OfflineLoadService::RegisterNotification(
    NavigationController* navigation_controller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.Add(this, NotificationType::TAB_CLOSED,
                   Source<NavigationController>(
                       navigation_controller));
}

}  // namespace chromeos
