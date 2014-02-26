// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_EVENT_DISPATCHER_H_

#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/common/extensions/api/serial.h"

class Profile;

namespace extensions {

struct Event;
class SerialConnection;

namespace api {

// Per-profile dispatcher for events on serial connections.
class SerialEventDispatcher : public ProfileKeyedAPI {
 public:
  explicit SerialEventDispatcher(content::BrowserContext* context);
  virtual ~SerialEventDispatcher();

  // Start receiving data and firing events for a connection.
  void PollConnection(const std::string& extension_id, int connection_id);

  static SerialEventDispatcher* Get(content::BrowserContext* context);

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<SerialEventDispatcher>* GetFactoryInstance();

 private:
  typedef ApiResourceManager<SerialConnection>::ApiResourceData ConnectionData;
  friend class ProfileKeyedAPIFactory<SerialEventDispatcher>;

  // ProfileKeyedAPI implementation.
  static const char *service_name() {
    return "SerialEventDispatcher";
  }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  struct ReceiveParams {
    ReceiveParams();
    ~ReceiveParams();

    content::BrowserThread::ID thread_id;
    void* profile_id;
    std::string extension_id;
    scoped_refptr<ConnectionData> connections;
    int connection_id;
  };

  static void StartReceive(const ReceiveParams& params);

  static void ReceiveCallback(const ReceiveParams& params,
                              const std::string& data,
                              serial::ReceiveError error);

  static void PostEvent(const ReceiveParams& params,
                        scoped_ptr<extensions::Event> event);

  static void DispatchEvent(void *profile_id,
                            const std::string& extension_id,
                            scoped_ptr<extensions::Event> event);

  content::BrowserThread::ID thread_id_;
  Profile* const profile_;
  scoped_refptr<ConnectionData> connections_;
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_EVENT_DISPATCHER_H_
