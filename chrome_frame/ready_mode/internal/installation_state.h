// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_READY_MODE_INTERNAL_INSTALLATION_STATE_H_
#define CHROME_FRAME_READY_MODE_INTERNAL_INSTALLATION_STATE_H_
#pragma once

// Provides an interface to query and manipulate the registration and
// installation state of the product.
class InstallationState {
 public:
  virtual ~InstallationState() {}

  // Queries the installation state of the product (whether the product appears
  // in "Add/Remove Programs" or its equivalent).
  virtual bool IsProductInstalled() = 0;

  // Queries the registration state of the product (whether the COM objects,
  // BHO, etc. are registered).
  virtual bool IsProductRegistered() = 0;

  // Installs the product. Returns true iff successful.
  virtual bool InstallProduct() = 0;

  // Unregisters the product. Fails if the product is installed. Returns true
  // iff successful.
  virtual bool UnregisterProduct() = 0;
};  // class InstallationState

#endif  // CHROME_FRAME_READY_MODE_INTERNAL_INSTALLATION_STATE_H_
