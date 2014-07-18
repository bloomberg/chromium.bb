// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/lazy_dom_distiller_service.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "content/public/browser/notification_source.h"

namespace dom_distiller {

LazyDomDistillerService::LazyDomDistillerService(
    Profile* profile,
    const DomDistillerServiceFactory* service_factory)
    : profile_(profile), service_factory_(service_factory) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::Source<Profile>(profile));
}

LazyDomDistillerService::~LazyDomDistillerService() {
}

// This will create an object and schedule work the first time it's called
// and just return an existing object after that.
DomDistillerServiceInterface* LazyDomDistillerService::instance() const {
  return service_factory_->GetForBrowserContext(profile_);
}

void LazyDomDistillerService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);
  DCHECK_EQ(profile_, content::Source<Profile>(source).ptr());
  delete this;
}

syncer::SyncableService* LazyDomDistillerService::GetSyncableService() const {
  return instance()->GetSyncableService();
}

const std::string LazyDomDistillerService::AddToList(
    const GURL& url,
    scoped_ptr<DistillerPage> distiller_page,
    const ArticleAvailableCallback& article_cb) {
  return instance()->AddToList(url, distiller_page.Pass(), article_cb);
}

std::vector<ArticleEntry> LazyDomDistillerService::GetEntries() const {
  return instance()->GetEntries();
}

scoped_ptr<ArticleEntry> LazyDomDistillerService::RemoveEntry(
    const std::string& entry_id) {
  return instance()->RemoveEntry(entry_id);
}

scoped_ptr<ViewerHandle> LazyDomDistillerService::ViewEntry(
    ViewRequestDelegate* delegate,
    scoped_ptr<DistillerPage> distiller_page,
    const std::string& entry_id) {
  return instance()->ViewEntry(delegate, distiller_page.Pass(), entry_id);
}

scoped_ptr<ViewerHandle> LazyDomDistillerService::ViewUrl(
    ViewRequestDelegate* delegate,
    scoped_ptr<DistillerPage> distiller_page,
    const GURL& url) {
  return instance()->ViewUrl(delegate, distiller_page.Pass(), url);
}

scoped_ptr<DistillerPage>
LazyDomDistillerService::CreateDefaultDistillerPage(
    const gfx::Size& render_view_size) {
  return instance()->CreateDefaultDistillerPage(render_view_size);
}

scoped_ptr<DistillerPage>
LazyDomDistillerService::CreateDefaultDistillerPageWithHandle(
    scoped_ptr<SourcePageHandle> handle) {
  return instance()->CreateDefaultDistillerPageWithHandle(handle.Pass());
}

void LazyDomDistillerService::AddObserver(DomDistillerObserver* observer) {
  instance()->AddObserver(observer);
}

void LazyDomDistillerService::RemoveObserver(DomDistillerObserver* observer) {
  instance()->RemoveObserver(observer);
}

DistilledPagePrefs* LazyDomDistillerService::GetDistilledPagePrefs() {
  return instance()->GetDistilledPagePrefs();
}

}  // namespace dom_distiller
