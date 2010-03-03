// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_

#include <map>
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"

class GeolocationDispatcherHost;
class Profile;
class RenderViewHost;

// GeolocationPermissionContext manages Geolocation permissions per host.
// It keeps an in-memory cache of permissions, and if not available, loads
// from disk. If there's no data, it'll trigger the UI elements to ask the
// user for permission.
// Regardless of where the permission data came from, it always notifies the
// requesting render_view asynchronously via ViewMsg_Geolocation_PermissionSet.
class GeolocationPermissionContext
    : public base::RefCountedThreadSafe<GeolocationPermissionContext> {
 public:
  explicit GeolocationPermissionContext(Profile* profile);

  // The render is requesting permission to use Geolocation.
  // Response will be sent asynchronously as ViewMsg_Geolocation_PermissionSet.
  // Must be called from the IO thread.
  void RequestGeolocationPermission(
      int render_process_id, int render_view_id, int bridge_id,
      const std::string& host);

  // Called once the user sets the geolocation permission.
  // It'll update the internal state on different threads via
  // SetPermissionMemoryCacheOnIOThread and SetPermissionOnFileThread.
  void SetPermission(
      int render_process_id, int render_view_id, int bridge_id,
      const std::string& host, bool allowed);

 private:
  friend class base::RefCountedThreadSafe<GeolocationPermissionContext>;
  virtual ~GeolocationPermissionContext();

  // This is initially called on the IO thread by the public API
  // RequestGeolocationPermission when there's no data available in the
  // in-memory cache.
  // It forwards a call to the FILE thread which tries to load permission data
  // from disk:
  // - If available, it will call SetPermissionMemoryCacheOnIOThread() to write
  // the in-memory cache in the IO thread, and NotifyPermissionSet to send the
  // message to the corresponding render.
  // - If not available, it'll delegate to RequestPermissionDataFromUI.
  void HandlePermissionMemoryCacheMiss(
      int render_process_id, int render_view_id, int bridge_id,
      const std::string& host);

  // Triggers the associated UI element to request permission.
  void RequestPermissionFromUI(
      int render_process_id, int render_view_id, int bridge_id,
      const std::string& host);

  // Notifies whether or not the corresponding render is allowed to use
  // geolocation.
  void NotifyPermissionSet(
      int render_process_id, int render_view_id, int bridge_id,
      bool allowed);

  // Sets permissions_ cache (if not on IO thread, will forward to it).
  void SetPermissionMemoryCacheOnIOThread(
      const std::string& host, bool allowed);
  // Sets permissions file data (if not on FILE thread, will forward to it).
  void SetPermissionOnFileThread(const std::string& host, bool allowed);

  // This should only be accessed from the UI thread.
  Profile* const profile_;
  // Indicates whether profile_ is off the record.
  bool const is_off_the_record_;
  // The path where geolocation permission data is stored.
  FilePath const permissions_path_;
  // This should only be accessed from the UI thread.
  std::map<std::string, bool> permissions_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationPermissionContext);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
