// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PROFILE_PROFILE_SERVICE_IMPL_H_
#define MOJO_SERVICES_PROFILE_PROFILE_SERVICE_IMPL_H_

#include "base/compiler_specific.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/public/interfaces/profile/profile_service.mojom.h"

namespace mojo {

class ApplicationConnection;

class ProfileServiceImpl : public InterfaceImpl<ProfileService> {
 public:
  ProfileServiceImpl(ApplicationConnection* connection);
  virtual ~ProfileServiceImpl();

  // |ProfileService| methods:
  virtual void GetPath(
      PathKey key,
      const mojo::Callback<void(mojo::String)>& callback)
      OVERRIDE;

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(ProfileServiceImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PROFILE_PROFILE_SERVICE_IMPL_H_
