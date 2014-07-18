// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_DISTILLER_LAZY_DOM_DISTILLER_SERVICE_H_
#define CHROME_BROWSER_DOM_DISTILLER_LAZY_DOM_DISTILLER_SERVICE_H_

#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class NotificationSource;
class NotificationDetails;
}  // namespace content

class Profile;

namespace dom_distiller {

class DomDistillerServiceFactory;

// A class which helps with lazy instantiation of the DomDistillerService, using
// the BrowserContextKeyedServiceFactory for it. This class will delete itself
// when the profile is destroyed.
class LazyDomDistillerService : public DomDistillerServiceInterface,
                                public content::NotificationObserver {
 public:
  LazyDomDistillerService(Profile* profile,
                          const DomDistillerServiceFactory* service_factory);
  virtual ~LazyDomDistillerService();

 public:
  // DomDistillerServiceInterface implementation:
  virtual syncer::SyncableService* GetSyncableService() const OVERRIDE;
  virtual const std::string AddToList(
      const GURL& url,
      scoped_ptr<DistillerPage> distiller_page,
      const ArticleAvailableCallback& article_cb) OVERRIDE;
  virtual std::vector<ArticleEntry> GetEntries() const OVERRIDE;
  virtual scoped_ptr<ArticleEntry> RemoveEntry(
      const std::string& entry_id) OVERRIDE;
  virtual scoped_ptr<ViewerHandle> ViewEntry(
      ViewRequestDelegate* delegate,
      scoped_ptr<DistillerPage> distiller_page,
      const std::string& entry_id) OVERRIDE;
  virtual scoped_ptr<ViewerHandle> ViewUrl(
      ViewRequestDelegate* delegate,
      scoped_ptr<DistillerPage> distiller_page,
      const GURL& url) OVERRIDE;
  virtual scoped_ptr<DistillerPage> CreateDefaultDistillerPage(
      const gfx::Size& render_view_size) OVERRIDE;
  virtual scoped_ptr<DistillerPage> CreateDefaultDistillerPageWithHandle(
      scoped_ptr<SourcePageHandle> handle) OVERRIDE;
  virtual void AddObserver(DomDistillerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DomDistillerObserver* observer) OVERRIDE;
  virtual DistilledPagePrefs* GetDistilledPagePrefs() OVERRIDE;

 private:
  // Accessor method for the backing service instance.
  DomDistillerServiceInterface* instance() const;

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // The Profile to use when retrieving the DomDistillerService and also the
  // profile to listen for destruction of.
  Profile* profile_;

  // A BrowserContextKeyedServiceFactory for the DomDistillerService.
  const DomDistillerServiceFactory* service_factory_;

  // Used to track when the profile is shut down.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(LazyDomDistillerService);
};

}  // namespace dom_distiller

#endif  // CHROME_BROWSER_DOM_DISTILLER_LAZY_DOM_DISTILLER_SERVICE_H_
