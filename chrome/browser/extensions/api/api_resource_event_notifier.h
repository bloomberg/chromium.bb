// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_EVENT_NOTIFIER_H_
#define CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_EVENT_NOTIFIER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"

class ExtensionEventRouter;
class Profile;

namespace base {
class ListValue;
}

namespace extensions {

enum APIResourceEventType {
  API_RESOURCE_EVENT_CONNECT_COMPLETE,
  API_RESOURCE_EVENT_DATA_READ,
  API_RESOURCE_EVENT_WRITE_COMPLETE,
  API_RESOURCE_EVENT_TRANSFER_COMPLETE,
};

extern const char kSrcIdKey[];

// TODO(miket): It's possible that we'll further refactor these new classes in
// light of some changes that mihaip has suggested. The names might change,
// too:
//
// IOResource
// IOResourceExtensionFunction
// IOResourceEventNotifier
// IOResourceController

// APIResourceEventNotifier knows how to send an event to a specific app's
// onEvent handler. It handles all platform-API events.
class APIResourceEventNotifier
    : public base::RefCountedThreadSafe<APIResourceEventNotifier> {
 public:
  APIResourceEventNotifier(ExtensionEventRouter* router,
                           Profile* profile,
                           const std::string& src_extension_id, int src_id,
                           const GURL& src_url);

  virtual void OnConnectComplete(int result_code);

  // Takes ownership of data.
  virtual void OnDataRead(int result_code,
                          base::ListValue* data,
                          const std::string& address,
                          int port);

  virtual void OnWriteComplete(int result_code);

  virtual void OnTransferComplete(int result_code, base::BinaryValue* data);

  static std::string APIResourceEventTypeToString(
      APIResourceEventType event_type);

 private:
  friend class base::RefCountedThreadSafe<APIResourceEventNotifier>;
  friend class MockAPIResourceEventNotifier;

  virtual ~APIResourceEventNotifier();

  void DispatchEvent(const std::string &extension, DictionaryValue* event);
  void DispatchEventOnUIThread(const std::string& extension,
                               DictionaryValue* event);
  DictionaryValue* CreateAPIResourceEvent(APIResourceEventType event_type);

  void SendEventWithResultCode(const std::string& extension,
                               APIResourceEventType event_type,
                               int result_code);

  ExtensionEventRouter* router_;
  Profile* profile_;
  std::string src_extension_id_;
  int src_id_;
  GURL src_url_;

  DISALLOW_COPY_AND_ASSIGN(APIResourceEventNotifier);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_EVENT_NOTIFIER_H_
