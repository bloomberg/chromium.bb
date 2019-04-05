// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_USER_DATA_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_USER_DATA_H_

#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"

// Macro for OverlayUserData setup [add to .h file]:
// - Declares a static variable inside subclasses.  The address of this static
//   variable is used as the key to associate the OverlayUserData with its
//   user data container.
// - Adds a friend specification for OverlayUserData so it can access
//   specializations' private constructors and user data keys.
#define OVERLAY_USER_DATA_SETUP(Type)    \
  static constexpr int kUserDataKey = 0; \
  friend class OverlayUserData<Type>

// Macro for OverlayUserData setup implementation [add to .cc/.mm file]:
// - Instantiates the static variable declared by the previous macro. It must
//   live in a .cc/.mm file to ensure that there is only one instantiation of
//   the static variable.
#define OVERLAY_USER_DATA_SETUP_IMPL(Type) const int Type::kUserDataKey

// A base class for classes attached to, and scoped to, the lifetime of a user
// data container (e.g. OverlayRequest, OverlayResponse).
//
// --- in data.h ---
// class Data : public OverlayUserData<Data> {
//  public:
//   ~Data() override;
//   // ... more public stuff here ...
//  private:
//   OVERLAY_USER_DATA_SETUP(Data);
//   explicit Data( \* ANY ARGUMENT LIST SUPPORTED *\);
//   // ... more private stuff here ...
// };
//
// --- in data.cc ---
// OVERLAY_USER_DATA_SETUP_IMPL(Data);
template <class DataType>
class OverlayUserData : public base::SupportsUserData::Data {
 public:
  // Creates an OverlayUserData of type DataType.  The DataType instance is
  // constructed using the arguments passed to this function.  For example, if
  // the constructor for an OverlayUserData of type StringData takes a string,
  // one can be created using:
  //
  // StringData::Create("string");
  template <typename... Args>
  static std::unique_ptr<DataType> Create(Args&&... args) {
    return base::WrapUnique(new DataType(std::forward<Args>(args)...));
  }

  // The key under which to store the user data.
  static const void* UserDataKey() { return &DataType::kUserDataKey; }
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_USER_DATA_H_
