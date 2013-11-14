// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_SCHEMA_REGISTRY_H_
#define CHROME_BROWSER_POLICY_SCHEMA_REGISTRY_H_

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/policy/schema_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema.h"

namespace policy {

class SchemaMap;

// Holds the main reference to the current SchemaMap, and allows a list of
// observers to get notified whenever it is updated.
// This object is not thread safe and must be used from the owner's thread,
// usually UI.
class SchemaRegistry : public base::NonThreadSafe {
 public:
  class Observer {
   public:
    // Invoked whenever schemas are registered or unregistered.
    // |has_new_schemas| is true if a new component has been registered since
    // the last update; this allows observers to ignore updates when
    // components are unregistered but still get a handle to the current map
    // (e.g. for periodic reloads).
    virtual void OnSchemaRegistryUpdated(bool has_new_schemas) = 0;

    // Invoked when all policy domains become ready.
    virtual void OnSchemaRegistryReady() = 0;

   protected:
    virtual ~Observer();
  };

  SchemaRegistry();
  virtual ~SchemaRegistry();

  const scoped_refptr<SchemaMap>& schema_map() const { return schema_map_; }

  // Register a single component.
  void RegisterComponent(const PolicyNamespace& ns,
                         const Schema& schema);

  // Register a list of components for a given domain.
  virtual void RegisterComponents(PolicyDomain domain,
                                  const ComponentMap& components);

  virtual void UnregisterComponent(const PolicyNamespace& ns);

  // Returns true if all domains have registered the initial components.
  bool IsReady() const;

  // This indicates that the initial components for |domain| have all been
  // registered. It must be invoked at least once for each policy domain;
  // subsequent calls for the same domain are ignored.
  void SetReady(PolicyDomain domain);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool HasObservers() const;

 protected:
  void Notify(bool has_new_schemas);

  scoped_refptr<SchemaMap> schema_map_;

 private:
  ObserverList<Observer, true> observers_;
  bool domains_ready_[POLICY_DOMAIN_SIZE];

  DISALLOW_COPY_AND_ASSIGN(SchemaRegistry);
};

// A registry that combines the maps of other registries.
class CombinedSchemaRegistry : public SchemaRegistry,
                               public SchemaRegistry::Observer {
 public:
  CombinedSchemaRegistry();
  virtual ~CombinedSchemaRegistry();

  void Track(SchemaRegistry* registry);
  void Untrack(SchemaRegistry* registry);

  virtual void RegisterComponents(PolicyDomain domain,
                                  const ComponentMap& components) OVERRIDE;

  virtual void UnregisterComponent(const PolicyNamespace& ns) OVERRIDE;

  virtual void OnSchemaRegistryUpdated(bool has_new_schemas) OVERRIDE;

  virtual void OnSchemaRegistryReady() OVERRIDE;

 private:
  void Combine(bool has_new_schemas);

  std::set<SchemaRegistry*> registries_;
  scoped_refptr<SchemaMap> own_schema_map_;

  DISALLOW_COPY_AND_ASSIGN(CombinedSchemaRegistry);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_SCHEMA_REGISTRY_H_
