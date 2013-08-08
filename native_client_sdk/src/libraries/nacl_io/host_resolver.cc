// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/host_resolver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sstream>
#include <string>

#include "nacl_io/kernel_proxy.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/pepper_interface.h"

#ifdef PROVIDES_SOCKET_API

namespace nacl_io {

HostResolver::HostResolver() : hostent_(), ppapi_(NULL) {
}

HostResolver::~HostResolver() {
  hostent_cleanup();
}

void HostResolver::Init(PepperInterface* ppapi) {
  ppapi_ = ppapi;
}

struct hostent* HostResolver::gethostbyname(const char* name) {
  h_errno = NETDB_INTERNAL;

  if (NULL == ppapi_)
    return NULL;

  HostResolverInterface* resolver_interface =
    ppapi_->GetHostResolverInterface();
  ScopedResource resolver(ppapi_,
                          resolver_interface->Create(ppapi_->GetInstance()));

  struct PP_CompletionCallback callback;
  callback.func = NULL;
  struct PP_HostResolver_Hint hint;
  hint.family = PP_NETADDRESS_FAMILY_IPV4;
  hint.flags = PP_HOSTRESOLVER_FLAG_CANONNAME;

  int err = resolver_interface->Resolve(resolver.pp_resource(),
                                        name, 0, &hint, callback);
  if (err) {
    switch (err) {
    case PP_ERROR_NOACCESS:
      h_errno = NO_RECOVERY;
      break;
    case PP_ERROR_NAME_NOT_RESOLVED:
      h_errno = HOST_NOT_FOUND;
      break;
    default:
      h_errno = NETDB_INTERNAL;
      break;
    }
    return NULL;
  }

  // We use a single hostent struct for all calls to to gethostbyname
  // (as explicitly permitted by the spec - gethostbyname is NOT supposed to
  // be threadsafe!), so the first thing we do is free all the malloced data
  // left over from the last call.
  hostent_cleanup();

  PP_Var name_var =
    resolver_interface->GetCanonicalName(resolver.pp_resource());
  if (PP_VARTYPE_STRING != name_var.type)
    return NULL;

  uint32_t len;
  const char* name_ptr = ppapi_->GetVarInterface()->VarToUtf8(name_var, &len);
  if (NULL == name_ptr)
    return NULL;
  if (0 == len) {
    // Sometimes GetCanonicalName gives up more easily than gethostbyname should
    // (for example, it returns "" when asked to resolve "localhost"), so if we
    // get an empty string we copy over the input string instead.
    len = strlen(name);
    name_ptr = name;
  }
  hostent_.h_name = static_cast<char*>(malloc(len + 1));
  if (NULL == hostent_.h_name)
    return NULL;
  memcpy(hostent_.h_name, name_ptr, len);
  hostent_.h_name[len] = '\0';

  // Aliases aren't supported at the moment, so we just make an empty list.
  hostent_.h_aliases = static_cast<char**>(malloc(sizeof(char*)));
  if (NULL == hostent_.h_aliases)
    return NULL;
  hostent_.h_aliases[0] = NULL;

  NetAddressInterface* netaddr_interface = ppapi_->GetNetAddressInterface();
  PP_Resource addr =
      resolver_interface->GetNetAddress(resolver.pp_resource(), 0);
  if (!PP_ToBool(netaddr_interface->IsNetAddress(addr)))
    return NULL;

  switch (netaddr_interface->GetFamily(addr)) {
    case PP_NETADDRESS_FAMILY_IPV4:
      hostent_.h_addrtype = AF_INET;
      hostent_.h_length = 4;
      break;
    case PP_NETADDRESS_FAMILY_IPV6:
      hostent_.h_addrtype = AF_INET6;
      hostent_.h_length = 16;
      break;
    default:
      return NULL;
  }

  const uint32_t num_addresses =
    resolver_interface->GetNetAddressCount(resolver.pp_resource());
  if (0 == num_addresses)
    return NULL;
  hostent_.h_addr_list =
    static_cast<char**>(calloc(num_addresses + 1, sizeof(char*)));
  if (NULL == hostent_.h_addr_list)
    return NULL;

  for (uint32_t i = 0; i < num_addresses; i++) {
    PP_Resource addr =
      resolver_interface->GetNetAddress(resolver.pp_resource(), i);
    if (!PP_ToBool(netaddr_interface->IsNetAddress(addr)))
      return NULL;
    if (AF_INET == hostent_.h_addrtype) {
      struct PP_NetAddress_IPv4 addr_struct;
      if (!netaddr_interface->DescribeAsIPv4Address(addr, &addr_struct)) {
        return NULL;
      }
      hostent_.h_addr_list[i] = static_cast<char*>(malloc(hostent_.h_length));
      if (NULL == hostent_.h_addr_list[i])
        return NULL;
      memcpy(hostent_.h_addr_list[i], addr_struct.addr, hostent_.h_length);
    } else { // IPv6
      struct PP_NetAddress_IPv6 addr_struct;
      if (!netaddr_interface->DescribeAsIPv6Address(addr, &addr_struct))
        return NULL;
      hostent_.h_addr_list[i] = static_cast<char*>(malloc(hostent_.h_length));
      if (NULL == hostent_.h_addr_list[i])
        return NULL;
      memcpy(hostent_.h_addr_list[i], addr_struct.addr, hostent_.h_length);
    }
  }
  return &hostent_;
}

// Frees all of the deep pointers in a hostent struct. Called between uses of
// gethostbyname, and when the kernel_proxy object is destroyed.
void HostResolver::hostent_cleanup() {
  if (NULL != hostent_.h_name) {
    free(hostent_.h_name);
  }
  if (NULL != hostent_.h_aliases) {
    for (int i = 0;  NULL != hostent_.h_aliases[i]; i++) {
      free(hostent_.h_aliases[i]);
    }
    free(hostent_.h_aliases);
  }
  if (NULL != hostent_.h_addr_list) {
    for (int i = 0;  NULL != hostent_.h_addr_list[i]; i++) {
      free(hostent_.h_addr_list[i]);
    }
    free(hostent_.h_addr_list);
  }
  hostent_.h_name = NULL;
  hostent_.h_aliases = NULL;
  hostent_.h_addr_list = NULL;
}

void HostResolver::herror(const char* s) {
  if (s) {
    fprintf(stderr, "%s: ", s);
  }

  fprintf(stderr, "%s\n", hstrerror(h_errno));
}

const char* HostResolver::hstrerror(int err) {
  // These error message texts are taken straight from the man page
  const char* host_not_found_msg =
    "The specified host is unknown.";
  const char* no_address_msg =
    "The requested name is valid but does not have an IP address.";
  const char* no_recovery_msg =
    "A nonrecoverable name server error occurred.";
  const char* try_again_msg =
    "A temporary error occurred on an authoritative name server. "
    "Try again later.";
  const char* internal_msg =
    "Internal error in gethostbyname.";
  const char* unknown_msg_base =
    "Unknown error in gethostbyname: ";

  switch (err) {
    case HOST_NOT_FOUND:
      return host_not_found_msg;
    case NO_ADDRESS:
      return no_address_msg;
    case NO_RECOVERY:
      return no_recovery_msg;
    case TRY_AGAIN:
      return try_again_msg;
    case NETDB_INTERNAL:
      return internal_msg;
    default:
      std::stringstream msg;
      msg << unknown_msg_base << err << ".";

      static std::string unknown_msg;
      unknown_msg.assign(msg.str());
      return unknown_msg.c_str();
  }
}

}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API
