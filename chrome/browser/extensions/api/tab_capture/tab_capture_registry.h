// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_REGISTRY_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/media/media_internals.h"
#include "chrome/browser/media/media_internals_observer.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/extensions/api/tab_capture.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/media_stream_request.h"

class Profile;

namespace extensions {

namespace tab_capture = extensions::api::tab_capture;

class TabCaptureRegistry : public ProfileKeyedService,
                           public content::NotificationObserver {
 public:
  struct TabCaptureRequest {
    std::string extension_id;
    int tab_id;
    tab_capture::TabCaptureState status;

    TabCaptureRequest() : tab_id(-1) {}
    TabCaptureRequest(std::string extension_id, int tab_id,
                      tab_capture::TabCaptureState status)
        : extension_id(extension_id), tab_id(tab_id), status(status) {}
  };
  typedef std::vector<TabCaptureRequest> CaptureRequestList;

  explicit TabCaptureRegistry(Profile* profile);

  const CaptureRequestList GetCapturedTabs(const std::string& extension_id);
  bool AddRequest(const std::pair<int, int>, const TabCaptureRequest& request);
  bool VerifyRequest(int render_process_id, int render_view_id);

 private:
  // Maps device_id to information about the media stream request. This is
  // expected to be small since maintaining a media stream is expensive.
  typedef std::map<const std::pair<int, int>, TabCaptureRequest>
      DeviceCaptureRequestMap;

  class MediaObserverProxy : public MediaInternalsObserver,
                             public base::RefCountedThreadSafe<
                                 MediaObserverProxy> {
   public:
    MediaObserverProxy() : handler_(NULL) {}
    void Attach(TabCaptureRegistry* handler);
    void Detach();

   private:
    friend class base::RefCountedThreadSafe<MediaObserverProxy>;
    virtual ~MediaObserverProxy() {}

    // MediaInternalsObserver.
    virtual void OnRequestUpdate(
        int render_process_id,
        int render_view_id,
        const content::MediaStreamDevice& device,
        const content::MediaRequestState state) OVERRIDE;

    void RegisterAsMediaObserverOnIOThread(bool unregister);
    void UpdateOnUIThread(
        int render_process_id,
        int render_view_id,
        const content::MediaStreamDevice& device,
        const content::MediaRequestState new_state);

    TabCaptureRegistry* handler_;
  };

  virtual ~TabCaptureRegistry();

  void HandleRequestUpdateOnUIThread(
      int render_process_id,
      int render_view_id,
      const content::MediaStreamDevice& device,
      const content::MediaRequestState state);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  scoped_refptr<MediaObserverProxy> proxy_;
  content::NotificationRegistrar registrar_;
  Profile* const profile_;
  DeviceCaptureRequestMap requests_;

  DISALLOW_COPY_AND_ASSIGN(TabCaptureRegistry);
};

}  // namespace extension

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_REGISTRY_H_
