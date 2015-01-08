// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_USB_USB_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_API_USB_USB_EVENT_ROUTER_H_

#include "content/public/browser/browser_thread.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

namespace device {
class UsbDevice;
}

namespace extensions {

// A BrowserContext-scoped object which is registered as an observer of the
// EventRouter and UsbService in order to generate device add/remove events.
class UsbEventRouter : public BrowserContextKeyedAPI,
                       public EventRouter::Observer {
 public:
  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<UsbEventRouter>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<UsbEventRouter>;

  class FileThreadHelper;

  explicit UsbEventRouter(content::BrowserContext* context);
  ~UsbEventRouter() override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "UsbEventRouter"; }
  static const bool kServiceIsNULLWhileTesting = true;

  // KeyedService implementation.
  void Shutdown() override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

  // Broadcasts a device add or remove event for the given device.
  void DispatchEvent(const std::string& event_name,
                     scoped_refptr<device::UsbDevice> device,
                     const base::string16& serial_number);

  content::BrowserContext* const browser_context_;
  scoped_ptr<FileThreadHelper, content::BrowserThread::DeleteOnFileThread>
      helper_;

  base::WeakPtrFactory<UsbEventRouter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsbEventRouter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_USB_USB_EVENT_ROUTER_H_
