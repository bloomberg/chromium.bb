// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_DISCOVERY_H_
#define DEVICE_U2F_U2F_DISCOVERY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"

namespace device {

class U2fDevice;

class U2fDiscovery {
 public:
  class Delegate {
   public:
    virtual void OnStarted(bool success) = 0;
    virtual void OnStopped(bool success) = 0;
    virtual void OnDeviceAdded(std::unique_ptr<U2fDevice> device) = 0;
    virtual void OnDeviceRemoved(base::StringPiece device_id) = 0;
  };

  U2fDiscovery();
  virtual ~U2fDiscovery();

  virtual void Start() = 0;
  virtual void Stop() = 0;

  void SetDelegate(base::WeakPtr<Delegate> delegate);

 protected:
  base::WeakPtr<Delegate> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(U2fDiscovery);
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_DISCOVERY_H_
