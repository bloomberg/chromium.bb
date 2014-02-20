// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MAPPED_IP_RESOLVER_H_
#define NET_DNS_MAPPED_IP_RESOLVER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/ip_mapping_rules.h"
#include "net/base/net_export.h"
#include "net/dns/host_resolver.h"

namespace net {

// This class wraps an existing HostResolver instance, but modifies the
// resolution response by inserting or replacing IP addresses before returning
// it.  Currently, the only directive suported is a "PREFACE" directive which
// (when a match exists) inserts a single IP address at the start of a list.
class NET_EXPORT MappedIPResolver : public HostResolver {
 public:
  // Creates a MappedIPResolver that forwards all of its requests through
  // |impl|.
  explicit MappedIPResolver(scoped_ptr<HostResolver> impl);
  virtual ~MappedIPResolver();

  // Adds a rule to our IP mapper rules_.
  // The most recently added rule "has priority" and will be used first (in
  // preference to) any previous rules.  Once one rule is found that matches,
  // no other rules will be considered.
  // See ip_mapping_rules.h for syntax and semantics.
  // Returns true if the rule was successfully parsed and added.
  bool AddRuleFromString(const std::string& rule_string) {
    return rules_.AddRuleFromString(rule_string);
  }

  // Takes a semicolon separated list of rules, and assigns them to this
  // resolver, discarding any previously added or set rules.
  void SetRulesFromString(const std::string& rules_string) {
    rules_.SetRulesFromString(rules_string);
  }

  // HostResolver methods:
  virtual int Resolve(const RequestInfo& info,
                      RequestPriority priority,
                      AddressList* addresses,
                      const CompletionCallback& callback,
                      RequestHandle* out_req,
                      const BoundNetLog& net_log) OVERRIDE;
  virtual int ResolveFromCache(const RequestInfo& info,
                               AddressList* addresses,
                               const BoundNetLog& net_log) OVERRIDE;
  virtual void CancelRequest(RequestHandle req) OVERRIDE;
  virtual void SetDnsClientEnabled(bool enabled) OVERRIDE;
  virtual HostCache* GetHostCache() OVERRIDE;
  virtual base::Value* GetDnsConfigAsValue() const OVERRIDE;

 private:
  // Modify the list of resolution |addresses| according to |rules_|, and then
  // calls the |original_callback| with network error code |rv|.
  void ApplyRules(const CompletionCallback& original_callback,
                  AddressList* addresses,
                  int rv) const;

  scoped_ptr<HostResolver> impl_;
  IPMappingRules rules_;

  base::WeakPtrFactory<MappedIPResolver> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(MappedIPResolver);
};

}  // namespace net

#endif  // NET_DNS_MAPPED_IP_RESOLVER_H_
