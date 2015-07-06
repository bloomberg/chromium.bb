// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_WEBUI_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H
#define COMPONENTS_PROXIMITY_AUTH_WEBUI_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H

#include "base/memory/scoped_ptr.h"
#include "components/proximity_auth/cryptauth/cryptauth_enroller.h"

namespace proximity_auth {

class ProximityAuthUIDelegate;

// Implementation of CryptAuthEnrollerFactory with dependencies on Chrome.
// TODO(tengs): Move this class out of the webui directory when we are ready to
// do enrollments in Chrome.
class CryptAuthEnrollerFactoryImpl : public CryptAuthEnrollerFactory {
 public:
  explicit CryptAuthEnrollerFactoryImpl(ProximityAuthUIDelegate* delegate);
  ~CryptAuthEnrollerFactoryImpl() override;

  // CryptAuthEnrollerFactory:
  scoped_ptr<CryptAuthEnroller> CreateInstance() override;

 private:
  // |delegate_| is not owned and must outlive this instance.
  ProximityAuthUIDelegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthEnrollerFactoryImpl);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_WEBUI_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H
