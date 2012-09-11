// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_EVENT_NOTIFIER_H_
#define CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_EVENT_NOTIFIER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/usb/usb_device.h"
#include "googleurl/src/gurl.h"

class Profile;

namespace base {
class ListValue;
}

namespace extensions {
class EventRouter;

enum ApiResourceEventType {
  API_RESOURCE_EVENT_TRANSFER_COMPLETE,
};

extern const char kSrcIdKey[];

// ApiResourceEventNotifier knows how to send an event to a specific app's
// onEvent handler.
class ApiResourceEventNotifier
    : public base::RefCountedThreadSafe<ApiResourceEventNotifier> {
 public:
  ApiResourceEventNotifier(EventRouter* router, Profile* profile,
                           const std::string& src_extension_id, int src_id,
                           const GURL& src_url);

  virtual void OnTransferComplete(UsbTransferStatus status,
                                  const std::string& error,
                                  base::BinaryValue* data);

  static std::string ApiResourceEventTypeToString(
      ApiResourceEventType event_type);

  const std::string& src_extension_id() const { return src_extension_id_; }

 private:
  friend class base::RefCountedThreadSafe<ApiResourceEventNotifier>;
  friend class MockApiResourceEventNotifier;

  virtual ~ApiResourceEventNotifier();

  void DispatchEvent(const std::string &extension, DictionaryValue* event);
  void DispatchEventOnUIThread(const std::string& extension,
                               DictionaryValue* event);
  DictionaryValue* CreateApiResourceEvent(ApiResourceEventType event_type);

  void SendEventWithResultCode(const std::string& extension,
                               ApiResourceEventType event_type,
                               int result_code);

  EventRouter* router_;
  Profile* profile_;
  std::string src_extension_id_;
  int src_id_;
  GURL src_url_;

  DISALLOW_COPY_AND_ASSIGN(ApiResourceEventNotifier);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_EVENT_NOTIFIER_H_
