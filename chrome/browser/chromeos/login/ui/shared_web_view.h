// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_SHARED_WEB_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_SHARED_WEB_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "url/gurl.h"

class Profile;

namespace chromeos {

class WebViewHandle;

// Stores and fetches a views::WebView instance that is ulimately owned by the
// signin profile. This allows for a WebView to be reused over time or
// preloaded. Use SharedWebViewFactory to get an instance of this class.
class SharedWebView : public KeyedService,
                      public content::NotificationObserver {
 public:
  explicit SharedWebView(Profile* profile);
  ~SharedWebView() override;
  void Shutdown() override;

  // Stores a webview instance inside of |out_handle|. |url| is the initial
  // URL that the webview stores (note: this function does not perform the
  // navigation). This returns true if the webview was reused, false if it
  // was freshly created.
  bool Get(const GURL& url, scoped_refptr<WebViewHandle>* out_handle);

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Called when there is a memory pressure event.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  content::NotificationRegistrar registrar_;

  // Profile used for creating the views::WebView instance.
  Profile* const profile_;

  // Cached URL that the |web_view_| instance should point to used for
  // validation.
  GURL web_view_url_;

  // Shared web-view instance. Callers may take a reference on the handle so it
  // could outlive this class.
  scoped_refptr<WebViewHandle> web_view_handle_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  DISALLOW_COPY_AND_ASSIGN(SharedWebView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_SHARED_WEB_VIEW_H_
