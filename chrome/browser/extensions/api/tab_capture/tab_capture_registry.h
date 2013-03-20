// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_REGISTRY_H_

#include <string>

#include "base/memory/scoped_vector.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/extensions/api/tab_capture.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace extensions {

struct TabCaptureRequest;
class FullscreenObserver;

namespace tab_capture = extensions::api::tab_capture;

class TabCaptureRegistry : public ProfileKeyedService,
                           public content::NotificationObserver,
                           public MediaCaptureDevicesDispatcher::Observer {
 public:
  typedef std::vector<std::pair<int, tab_capture::TabCaptureState> >
      RegistryCaptureInfo;

  explicit TabCaptureRegistry(Profile* profile);

  // List all pending, active and stopped capture requests.
  const RegistryCaptureInfo GetCapturedTabs(
      const std::string& extension_id) const;

  // Add a tab capture request to the registry when a stream is requested
  // through the API.
  bool AddRequest(int render_process_id,
                  int render_view_id,
                  const std::string& extension_id,
                  int tab_id,
                  tab_capture::TabCaptureState status);

  // The MediaStreamDevicesController will verify the request before creating
  // the stream by checking the registry here.
  bool VerifyRequest(int render_process_id, int render_view_id);

 private:
  friend class FullscreenObserver;

  virtual ~TabCaptureRegistry();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // MediaCaptureDevicesDispatcher::Observer implementation.
  virtual void OnRequestUpdate(
      int render_process_id,
      int render_view_id,
      const content::MediaStreamDevice& device,
      const content::MediaRequestState state) OVERRIDE;

  void DispatchStatusChangeEvent(const TabCaptureRequest* request) const;

  TabCaptureRequest* FindCaptureRequest(int render_process_id,
                                        int render_view_id) const;

  void DeleteCaptureRequest(int render_process_id, int render_view_id);

  content::NotificationRegistrar registrar_;
  Profile* const profile_;
  ScopedVector<TabCaptureRequest> requests_;

  DISALLOW_COPY_AND_ASSIGN(TabCaptureRegistry);
};

}  // namespace extension

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_REGISTRY_H_
