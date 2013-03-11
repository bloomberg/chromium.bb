// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HOST_ZOOM_MAP_IMPL_H_
#define CONTENT_BROWSER_HOST_ZOOM_MAP_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {

// HostZoomMap needs to be deleted on the UI thread because it listens
// to notifications on there (and holds a NotificationRegistrar).
class CONTENT_EXPORT HostZoomMapImpl : public NON_EXPORTED_BASE(HostZoomMap),
                                       public NotificationObserver,
                                       public base::SupportsUserData::Data {
 public:
  HostZoomMapImpl();
  virtual ~HostZoomMapImpl();

  // HostZoomMap implementation:
  virtual void CopyFrom(HostZoomMap* copy) OVERRIDE;
  virtual double GetZoomLevelForHostAndScheme(
      const std::string& scheme,
      const std::string& host) const OVERRIDE;
  virtual void SetZoomLevelForHost(
      const std::string& host,
      double level) OVERRIDE;
  virtual void SetZoomLevelForHostAndScheme(
      const std::string& scheme,
      const std::string& host,
      double level) OVERRIDE;
  virtual double GetDefaultZoomLevel() const OVERRIDE;
  virtual void SetDefaultZoomLevel(double level) OVERRIDE;
  virtual void AddZoomLevelChangedCallback(
      const ZoomLevelChangedCallback& callback) OVERRIDE;
  virtual void RemoveZoomLevelChangedCallback(
      const ZoomLevelChangedCallback& callback) OVERRIDE;

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

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  double GetZoomLevelForHost(const std::string& host) const;

  typedef std::map<std::string, double> HostZoomLevels;
  typedef std::map<std::string, HostZoomLevels> SchemeHostZoomLevels;

  // Callbacks called when zoom level changes.
  std::vector<ZoomLevelChangedCallback> zoom_level_changed_callbacks_;

  // Copy of the pref data, so that we can read it on the IO thread.
  HostZoomLevels host_zoom_levels_;
  SchemeHostZoomLevels scheme_host_zoom_levels_;
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

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(HostZoomMapImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_HOST_ZOOM_MAP_IMPL_H_
