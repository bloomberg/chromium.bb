// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_BRIDGE_ABSTRACT_DATA_CHANNEL_H_
#define COMPONENTS_DEVTOOLS_BRIDGE_ABSTRACT_DATA_CHANNEL_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace devtools_bridge {

/**
 * WebRTC DataChannel adapter for DevTools bridge. Not thread safe.
 */
class AbstractDataChannel {
 public:
  AbstractDataChannel() {}
  virtual ~AbstractDataChannel() {}

  /**
   * Called on WebRTC signaling thread.
   */
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    virtual void OnOpen() = 0;
    virtual void OnClose() = 0;

    virtual void OnMessage(const void* data, size_t length) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  /**
   * Proxy for accessing data channel from a different thread.
   * May outlive data channel (methods will have no effect if DataChannel
   * destroyed).
   */
  class Proxy : public base::RefCountedThreadSafe<Proxy> {
   public:
    virtual void SendBinaryMessage(const void* data, size_t length) = 0;
    virtual void Close() = 0;

   protected:
    Proxy() {}
    virtual ~Proxy() {}

   private:
    friend class base::RefCountedThreadSafe<Proxy>;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  virtual void RegisterObserver(scoped_ptr<Observer> observer) = 0;
  virtual void UnregisterObserver() = 0;

  virtual void SendBinaryMessage(void* data, size_t length) = 0;
  virtual void SendTextMessage(void* data, size_t length) = 0;

  virtual void Close() = 0;

  virtual scoped_refptr<Proxy> proxy() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AbstractDataChannel);
};

}  // namespace devtools_bridge

#endif  // COMPONENTS_DEVTOOLS_BRIDGE_ABSTRACT_DATA_CHANNEL_H_
