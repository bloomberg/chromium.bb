// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_WEBUSB_BROWSER_CLIENT_H_
#define CHROME_BROWSER_CHROME_WEBUSB_BROWSER_CLIENT_H_

#include "base/macros.h"
#include "components/webusb/webusb_browser_client.h"

class GURL;

// Implementation of webusb::WebUsbBrowserClient.
// singletons appropriate for use within the Chrome application.
class ChromeWebUsbBrowserClient : public webusb::WebUsbBrowserClient {
 public:
  ChromeWebUsbBrowserClient();
  ~ChromeWebUsbBrowserClient() override;

  // webusb::WebUsbBrowserClient implementation
  void OnDeviceAdded(const base::string16& product_name,
                     const GURL& landing_page,
                     const std::string& notification_id) override;

  // webusb::WebUsbBrowserClient implementation
  void OnDeviceRemoved(const std::string& notification_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeWebUsbBrowserClient);
};

#endif  // CHROME_BROWSER_CHROME_WEBUSB_BROWSER_CLIENT_H_
