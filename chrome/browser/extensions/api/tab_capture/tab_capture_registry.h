// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_REGISTRY_H_

#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/scoped_observer.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/common/extensions/api/tab_capture.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {

struct TabCaptureRequest;
class ExtensionRegistry;
class FullscreenObserver;

namespace tab_capture = api::tab_capture;

class TabCaptureRegistry : public BrowserContextKeyedAPI,
                           public content::NotificationObserver,
                           public ExtensionRegistryObserver,
                           public MediaCaptureDevicesDispatcher::Observer {
 public:
  typedef std::vector<std::pair<int, tab_capture::TabCaptureState> >
      RegistryCaptureInfo;

  static TabCaptureRegistry* Get(content::BrowserContext* context);

  // Used by BrowserContextKeyedAPI.
  static BrowserContextKeyedAPIFactory<TabCaptureRegistry>*
      GetFactoryInstance();

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
  friend class BrowserContextKeyedAPIFactory<TabCaptureRegistry>;
  friend class FullscreenObserver;

  explicit TabCaptureRegistry(content::BrowserContext* context);
  virtual ~TabCaptureRegistry();

  // Used by BrowserContextKeyedAPI.
  static const char* service_name() {
    return "TabCaptureRegistry";
  }

  static const bool kServiceIsCreatedWithBrowserContext = false;
  static const bool kServiceRedirectedInIncognito = true;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

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
  content::BrowserContext* const browser_context_;
  ScopedVector<TabCaptureRequest> requests_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(TabCaptureRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_REGISTRY_H_
