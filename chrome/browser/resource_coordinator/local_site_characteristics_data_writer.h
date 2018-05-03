// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_WRITER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_WRITER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace resource_coordinator {

namespace internal {
class LocalSiteCharacteristicsDataImpl;
}  // namespace internal

// Used to record local characteristics usage observations in the local
// database.
class LocalSiteCharacteristicsDataWriter {
 public:
  ~LocalSiteCharacteristicsDataWriter();

  // Records tab load/unload events.
  void NotifySiteLoaded();
  void NotifySiteUnloaded();

  // Records feature usage.
  void NotifyUpdatesFaviconInBackground();
  void NotifyUpdatesTitleInBackground();
  void NotifyUsesAudioInBackground();
  void NotifyUsesNotificationsInBackground();

 private:
  friend class LocalSiteCharacteristicsDataWriterTest;
  friend class LocalSiteCharacteristicsDataStoreTest;
  friend class LocalSiteCharacteristicsDataStore;

  // Private constructor, these objects are meant to be created by a site
  // characteristics data store.
  explicit LocalSiteCharacteristicsDataWriter(
      scoped_refptr<internal::LocalSiteCharacteristicsDataImpl> impl);

  // The LocalSiteCharacteristicDataInternal object we delegate to.
  const scoped_refptr<internal::LocalSiteCharacteristicsDataImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsDataWriter);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_WRITER_H_
