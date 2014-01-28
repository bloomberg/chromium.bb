// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_host_resolver_interface.h"

#include <netinet/in.h>

#include "fake_ppapi/fake_pepper_interface.h"
#include "fake_ppapi/fake_resource_manager.h"
#include "fake_ppapi/fake_var_manager.h"
#include "gtest/gtest.h"

namespace  {

class FakeHostResolverResource : public FakeResource {
 public:
  FakeHostResolverResource() : resolved(false), name(NULL) {}
  static const char* classname() { return "FakeHostResolverResource"; }

  bool resolved;
  in_addr_t address;
  const char* name;
};

int32_t RunCompletionCallback(PP_CompletionCallback* callback, int32_t result) {
  if (callback->func) {
    PP_RunCompletionCallback(callback, result);
    return PP_OK_COMPLETIONPENDING;
  }
  return result;
}

}

FakeHostResolverInterface::FakeHostResolverInterface(FakePepperInterface* ppapi)
    : ppapi_(ppapi) {}

PP_Resource FakeHostResolverInterface::Create(PP_Instance instance) {
  if (instance != ppapi_->GetInstance())
    return PP_ERROR_BADRESOURCE;

  FakeHostResolverResource* resolver_resource = new FakeHostResolverResource;

  return CREATE_RESOURCE(ppapi_->resource_manager(),
                         FakeHostResolverResource,
                         resolver_resource);
}

int32_t FakeHostResolverInterface::Resolve(PP_Resource resource,
                                           const char* hostname,
                                           uint16_t,
                                           const PP_HostResolver_Hint*,
                                           PP_CompletionCallback callback) {
  FakeHostResolverResource* resolver =
      ppapi_->resource_manager()->Get<FakeHostResolverResource>(resource);
  resolver->resolved = false;
  if (!strcmp(hostname, FAKE_HOSTNAME)) {
    resolver->resolved = true;
    resolver->name = FAKE_HOSTNAME;
    resolver->address = htonl(FAKE_IP);
    return RunCompletionCallback(&callback, PP_OK);
  }
  return RunCompletionCallback(&callback, PP_ERROR_NAME_NOT_RESOLVED);
}

PP_Var FakeHostResolverInterface::GetCanonicalName(PP_Resource resource) {
  FakeHostResolverResource* res =
      ppapi_->resource_manager()->Get<FakeHostResolverResource>(resource);
  if (!res->resolved)
    return PP_Var();
  return ppapi_->GetVarInterface()->VarFromUtf8(res->name, strlen(res->name));
}

uint32_t FakeHostResolverInterface::GetNetAddressCount(PP_Resource resolver) {
  FakeHostResolverResource* res =
      ppapi_->resource_manager()->Get<FakeHostResolverResource>(resolver);
  if (!res->resolved)
    return 0;
  return 1;
}

PP_Resource FakeHostResolverInterface::GetNetAddress(PP_Resource resource,
                                                     uint32_t index) {
  FakeHostResolverResource* res =
      ppapi_->resource_manager()->Get<FakeHostResolverResource>(resource);
  if (!res->resolved)
    return 0;

  if (index != 0)
    return 0;

  // Create a new NetAddress resource and return it.
  PP_NetAddress_IPv4 addr;
  memcpy(addr.addr, &res->address, sizeof(res->address));
  nacl_io::NetAddressInterface* iface = ppapi_->GetNetAddressInterface();
  return iface->CreateFromIPv4Address(ppapi_->GetInstance(), &addr);
}
