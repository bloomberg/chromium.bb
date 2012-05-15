// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HOST_ZOOM_MAP_IMPL_H_
#define CONTENT_BROWSER_HOST_ZOOM_MAP_IMPL_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/message_loop_helpers.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

// HostZoomMap needs to be deleted on the UI thread because it listens
// to notifications on there (and holds a NotificationRegistrar).
class CONTENT_EXPORT HostZoomMapImpl
    : public NON_EXPORTED_BASE(content::HostZoomMap),
      public content::NotificationObserver,
      public base::SupportsUserData::Data {
 public:
  HostZoomMapImpl();
  virtual ~HostZoomMapImpl();

  // HostZoomMap implementation:
  virtual void CopyFrom(HostZoomMap* copy) OVERRIDE;
  virtual double GetZoomLevel(const std::string& host) const OVERRIDE;
  virtual void SetZoomLevel(const std::string& host, double level) OVERRIDE;
  virtual double GetDefaultZoomLevel() const OVERRIDE;
  virtual void SetDefaultZoomLevel(double level) OVERRIDE;

  // Returns the temporary zoom level that's only valid for the lifetime of
  // the given WebContents (i.e. isn't saved and doesn't affect other
  // WebContentses) if it exists, the default zoom level otherwise.
  //
  // This may be called on any thread.
  double GetTemporaryZoomLevel(int render_process_id,
                               int render_view_id) const;

  // Sets the temporary zoom level that's only valid for the lifetime of this
  // WebContents.
  //
  // This should only be called on the UI thread.
  void SetTemporaryZoomLevel(int render_process_id,
                             int render_view_id,
                             double level);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  typedef std::map<std::string, double> HostZoomLevels;

  // Copy of the pref data, so that we can read it on the IO thread.
  HostZoomLevels host_zoom_levels_;
  double default_zoom_level_;

  struct TemporaryZoomLevel {
    int render_process_id;
    int render_view_id;
    double zoom_level;
  };

  // Don't expect more than a couple of tabs that are using a temporary zoom
  // level, so vector is fine for now.
  std::vector<TemporaryZoomLevel> temporary_zoom_levels_;

  // Used around accesses to |host_zoom_levels_|, |default_zoom_level_| and
  // |temporary_zoom_levels_| to guarantee thread safety.
  mutable base::Lock lock_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(HostZoomMapImpl);
};

#endif  // CONTENT_BROWSER_HOST_ZOOM_MAP_IMPL_H_
