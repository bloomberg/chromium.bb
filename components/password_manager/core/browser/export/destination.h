// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_DESTINATION_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_DESTINATION_H_

namespace password_manager {

// Interface of a medium, to where a serialised list of passwords can be
// exported.
class Destination {
 public:
  // Send the data to the destination, synchronously.
  virtual bool Write(const std::string& data) = 0;
}

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_DESTINATION_H_