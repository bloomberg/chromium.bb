// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/pepper_interface.h"
#include <errno.h>
#include <ppapi/c/pp_errors.h>

namespace nacl_io {

void PepperInterface::AddRefResource(PP_Resource resource) {
  GetCoreInterface()->AddRefResource(resource);
}

void PepperInterface::ReleaseResource(PP_Resource resource) {
  GetCoreInterface()->ReleaseResource(resource);
}

ScopedResource::ScopedResource(PepperInterface* ppapi)
    : ppapi_(ppapi), resource_(0) {
}

ScopedResource::ScopedResource(PepperInterface* ppapi, PP_Resource resource)
    : ppapi_(ppapi), resource_(resource) {
}

ScopedResource::~ScopedResource() {
  if (resource_)
    ppapi_->ReleaseResource(resource_);
}

void ScopedResource::Reset(PP_Resource resource) {
  if (resource_)
    ppapi_->ReleaseResource(resource_);

  resource_ = resource;
}

PP_Resource ScopedResource::Release() {
  PP_Resource result = resource_;
  resource_ = 0;
  return result;
}

ScopedVar::ScopedVar(PepperInterface* ppapi)
    : ppapi_(ppapi), var_(PP_MakeUndefined()) {}

ScopedVar::ScopedVar(PepperInterface* ppapi, PP_Var var)
    : ppapi_(ppapi), var_(var) {}

ScopedVar::~ScopedVar() {
  ppapi_->GetVarInterface()->Release(var_);
}

void ScopedVar::Reset(PP_Var var) {
  ppapi_->GetVarInterface()->Release(var_);
  var_ = var;
}

PP_Var ScopedVar::Release() {
  PP_Var result = var_;
  var_ = PP_MakeUndefined();
  return result;
}

int PPErrorToErrno(int32_t err) {
  // If not an error, then just return it.
  if (err >= PP_OK)
    return err;

  switch (err) {
    case PP_OK_COMPLETIONPENDING: return 0;
    case PP_ERROR_FAILED: return EPERM;
    case PP_ERROR_ABORTED: return EPERM;
    case PP_ERROR_BADARGUMENT: return EINVAL;
    case PP_ERROR_BADRESOURCE: return EBADF;
    case PP_ERROR_NOINTERFACE: return ENOSYS;
    case PP_ERROR_NOACCESS: return EACCES;
    case PP_ERROR_NOMEMORY: return ENOMEM;
    case PP_ERROR_NOSPACE: return ENOSPC;
    case PP_ERROR_NOQUOTA: return ENOSPC;
    case PP_ERROR_INPROGRESS: return EBUSY;
    case PP_ERROR_NOTSUPPORTED: return ENOSYS;
    case PP_ERROR_BLOCKS_MAIN_THREAD: return EPERM;
    case PP_ERROR_FILENOTFOUND: return ENOENT;
    case PP_ERROR_FILEEXISTS: return EEXIST;
    case PP_ERROR_FILETOOBIG: return EFBIG;
    case PP_ERROR_FILECHANGED: return EINVAL;
    case PP_ERROR_TIMEDOUT: return EBUSY;
    case PP_ERROR_USERCANCEL: return EPERM;
    case PP_ERROR_NO_USER_GESTURE: return EPERM;
    case PP_ERROR_CONTEXT_LOST: return EPERM;
    case PP_ERROR_NO_MESSAGE_LOOP: return EPERM;
    case PP_ERROR_WRONG_THREAD: return EPERM;
    case PP_ERROR_CONNECTION_ABORTED: return ECONNABORTED;
    case PP_ERROR_CONNECTION_REFUSED: return ECONNREFUSED;
    case PP_ERROR_CONNECTION_FAILED: return ECONNREFUSED;
    case PP_ERROR_CONNECTION_TIMEDOUT: return ETIMEDOUT;
    case PP_ERROR_ADDRESS_UNREACHABLE: return ENETUNREACH;
    case PP_ERROR_ADDRESS_IN_USE: return EADDRINUSE;
  }

  return EINVAL;
}

}  // namespace nacl_io
